# esp-bench

Benchmark for miniesp on the ESP32: how fast is the CPU, what do two cores give, what does the OS cost.

```
esp-bench                 # everything, 400 ms per test (about 20 s)
esp-bench quick|long      # 120 ms / 1.5 s per test
esp-bench cpu native      # only some tests: cpu native load sys net
esp-bench --save          # also writes /www/bench/last.json + history.csv: results page at http://BOARD/bench/
esp-bench --json          # machine-readable output only
```
Stop pump-critical work first: while it runs it holds the runtime, so a service such as `wtms` skips its runs (no pump command is ever sent by the benchmark; it never touches GPIO, ADC, PWM or relays).

## Tests
Eight kernels (`programs/bench_kernels.h` in the miniesp repo, shared by the firmware and the program so the same C source runs both as AOT and natively):

| kernel | one iteration | stands for |
|---|---|---|
| `int` | 1000 LCG/xorshift steps | control loops, state machines, hashing |
| `float` | 400 f32 multiply-adds | filters, sensor maths (software float in AOT) |
| `double` | 400 f64 multiply-adds | calculations that need precision |
| `memcpy` | 1 KB copy + sum | buffers, framing |
| `sort` | shell sort of 128 words | log processing, ranking |
| `crc32` | bitwise CRC-32 of 64 bytes | protocols, checks |
| `matmul` | 8x8x8 int32 matrix product | small DSP / transforms |
| `chase` | 256 dependent loads | pointer-heavy code, memory latency |

Suites:
- **cpu**: each kernel as AOT code on the core that runs your program (this is what an app gets). Score = mean of (iterations/s / reference x 1000), 1000 = reference ESP32-WROOM at 240 MHz.
- **native**: the same kernels compiled natively in the firmware (`sys_bench`): AOT/native (how much the WebAssembly layer costs) and, on a dual-core build, core 0, core 1 and both at once (scaling, 200 % = perfect).
- **load**: AOT kernels on this core while core 1 runs a memory-bound native job (bus contention, multitasking).
- **sys**: syscall cost, `stat`, NVS read, LittleFS write/read speed (32 KB scratch file), in-RAM pipe.
- **net**: TCP over loopback (lwIP only, no radio).

## Notes
- The miniesp firmware is **single-core** (`CONFIG_FREERTOS_UNICORE`, needed for the 98.8 KB IRAM pool that holds program files), so the 2nd ESP32 core is not running. On it `native` shows core 0 only, `load` is skipped and the two-core columns are empty. On a board with PSRAM the **psram firmware variant** (miniesp `tools/build.sh psram`, `tools/flash.sh`) is dual-core: the program files then live in PSRAM, `esp-bench` shows the 1-core vs 2-core scaling and the `load` test, and the header line shows the PSRAM size. That variant is compile-checked only (no PSRAM board tested).
- Results vary with Wi-Fi activity on core 0 (run twice, compare `quick` with `long`).
- Needs firmware ABI 6 (`sys_bench`, `SI_CORES`); older firmware: esp-bench refuses to run.
