#!/bin/bash
# usage: tools/sim/run.sh [DAYS [DRY_AFTER_DAYS]]  (the second argument makes the pump deliver only 2 L/min from that day on: a dry-run test)   (MINIESP=/path/to/miniesp, default: the parent directory)
set -euo pipefail
cd "$(dirname "$0")"
MINIESP=${MINIESP:-$(cd ../../.. && pwd)}
mkdir -p build www_tank
python3 - "$MINIESP/programs/mini.h" <<'PY'
import sys
s = open(sys.argv[1]).read()
s = s.replace('#define SYS(name) __attribute__((import_module("env"), import_name(name)))', '#define SYS(name)')
a = s.index('/* ---- freestanding runtime: memory primitives'); b = s.index('/* ---- entry point')
s = (s[:a] + s[b:]).replace('__attribute__((export_name("esp_main"))) int esp_main(void)', 'int esp_main(void)')
open('build/mini.h', 'w').write(s)
PY
sed "s#/home/hamim-mahmud/Workspace/miniesp/miniesp-pkg/programs/wtms.c#$(cd ../../programs && pwd)/wtms.c#" wtms_sim.c > build/sim.c
gcc -O1 -w -Ibuild -o build/sim build/sim.c -lm
START=$(( $(date +%s) / 5 * 5 - ${1:-12} * 86400 ))
build/sim "${1:-12}" "$START" ${2:+"$2"}
