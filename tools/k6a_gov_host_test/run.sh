#!/bin/sh
# Aufruf: ./run.sh [pfad/zur/k6a_gov.c]   (Default: ../../drivers/thermal/k6a_gov/k6a_gov.c)
set -e
cd "$(dirname "$0")"
SRC="${1:-../../drivers/thermal/k6a_gov/k6a_gov.c}"
python3 extract.py "$SRC" extracted.h
gcc -std=gnu11 -Wall -Wextra -Wno-unused-function -o k6a_host_test harness.c
./k6a_host_test
