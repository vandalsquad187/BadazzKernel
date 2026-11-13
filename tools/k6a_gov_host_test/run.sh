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
echo "DOC-DRIFT: OK (hysteresis $CODE_FAST/$CODE_NORM, version $VERCODE)"
