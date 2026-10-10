# esp-bench bundle (read by tools/mkbundle.py): benchmark program + results page.
# Needs miniesp firmware with syscall ABI 6 (sys_bench) and the web design system (/style.css, /ui.js).
name esp-bench
version 1.0
abi 6
program ../../programs/esp-bench.c -> /esp/.local/bin/esp-bench.aot
dir /www/bench
file web/index.html -> /www/bench/index.html
note esp-bench installed. Run:  esp-bench          (everything, about 20 s; services that need the runtime skip a run meanwhile)
note quick:  esp-bench quick     save for the web page (/bench/):  esp-bench --save     tests: cpu native load sys net
