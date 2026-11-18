#!/bin/sh
# Aufruf: ./run.sh [pfad/zur/k6a_gov.c]   (Default: ../../drivers/thermal/k6a_gov/k6a_gov.c)
set -e
cd "$(dirname "$0")"
SRC="${1:-../../drivers/thermal/k6a_gov/k6a_gov.c}"
python3 extract.py "$SRC" extracted.h
gcc -std=gnu11 -Wall -Wextra -Wno-unused-function -o k6a_host_test harness.c
./k6a_host_test

# Build454: Doc-Drift-Guard — dokumentierte Defaults/Versionsrefs gegen Code
README=../../README.md
CODE_FAST=$(grep -o 'hysteresis_fast = [0-9][0-9]*' "$SRC" | head -1 | grep -o '[0-9]*$' || true)
CODE_NORM=$(grep -o 'hysteresis_normal = [0-9][0-9]*' "$SRC" | head -1 | grep -o '[0-9]*$' || true)
VERCODE=$(grep '^#define K6A_GOV_VERSION' "$SRC" | grep -o '[0-9]*\.[0-9]*\.[0-9]*' || true)
RD_LINE=$(grep '`hysteresis` | RW' "$README" | head -1 || true)
RD_F=$(printf '%s\n' "$RD_LINE" | awk -F'default ' '{split($2,a,")"); print a[1]}' | head -1 || true)
RD_N=$(printf '%s\n' "$RD_LINE" | awk -F'default ' '{split($3,a,")"); print a[1]}' | head -1 || true)
[ -n "$CODE_FAST" ] && [ "$CODE_FAST" = "$RD_F" ] || { echo "DOC-DRIFT: hysteresis_fast Code=${CODE_FAST:-?} vs README=${RD_F:-?}"; exit 1; }
[ -n "$CODE_NORM" ] && [ "$CODE_NORM" = "$RD_N" ] || { echo "DOC-DRIFT: hysteresis_normal Code=${CODE_NORM:-?} vs README=${RD_N:-?}"; exit 1; }
grep -q "$VERCODE" "$README" || { echo "DOC-DRIFT: README fehlt Kernel-Version $VERCODE"; exit 1; }

# Doc-Drift-Guard Teil 2: cd_ladder-Default + GAMING cd_thresholds/gpu_caps gegen profiles[]
grep -q 'gov->cd_ladder = true' "$SRC" || { echo "DOC-DRIFT: cd_ladder Code-Default fehlt (gov->cd_ladder = true)"; exit 1; }
grep '`cd_ladder` | RW' "$README" | grep -q 'default 1' || { echo "DOC-DRIFT: README cd_ladder-Row fehlt 'default 1'"; exit 1; }
G_NUMS=$(awk '/K6A_PROFILE_GAMING\]/{f=1} f{print; if(/},/) exit}' "$SRC" | sed 's/.*{//' | grep -o '[0-9][0-9]*' | tr '\n' ' ' | sed 's/ $//')
G_CDT=$(printf '%s\n' "$G_NUMS" | awk '{print $1,$2,$3,$4,$5,$6,$7}')
G_GPU=$(printf '%s\n' "$G_NUMS" | awk '{print $8,$9,$10}')
RD_CDT=$(grep '`cd_thresholds` | RW' "$README" | grep -o '`[0-9][0-9 ]*`' | head -1 | tr -d '`')
RD_GPU=$(grep '`gpu_caps` | RW' "$README" | grep -o '`[0-9][0-9 ]*`' | head -1 | tr -d '`')
[ -n "$G_CDT" ] && [ "$G_CDT" = "$RD_CDT" ] || { echo "DOC-DRIFT: cd_thresholds GAMING Code=$G_CDT vs README=$RD_CDT"; exit 1; }
[ -n "$G_GPU" ] && [ "$G_GPU" = "$RD_GPU" ] || { echo "DOC-DRIFT: gpu_caps GAMING Code=$G_GPU vs README=$RD_GPU"; exit 1; }

echo "DOC-DRIFT: OK (hysteresis $CODE_FAST/$CODE_NORM, version $VERCODE, cd_ladder=1, GAMING cdt=$G_CDT gpu=$G_GPU)"
