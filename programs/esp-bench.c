// esp-bench: ESP32 performance benchmark for miniesp.
//   esp-bench [quick|normal|long] [cpu] [native] [load] [sys] [net] [--json] [--save]
// With no test names everything runs. See packages/esp-bench/README.md. Kernels: miniesp programs/bench_kernels.h (shared with the firmware).
#include "mini.h"
#include "bench_kernels.h"

static uint32_t sc[BK_SCRATCH / 4];
static const char *const NAMES[BK_KINDS] = BK_NAMES;
static const char *const UNITN[BK_KINDS] = BK_UNIT_NAMES;
static const int UNITS[BK_KINDS] = BK_UNITS;
// score references: AOT iterations per second that count as 1000 points (measured on an ESP32-WROOM-32 at 240 MHz, miniesp 0.3)
static const int REF[BK_KINDS] = { 1500, 1000, 1000, 1000, 1000, 1000, 1000, 1000 };

static int ms = 400, json, save, t_cpu, t_nat, t_load, t_sys, t_net;
static int aot[BK_KINDS], n0[BK_KINDS], n1[BK_KINDS], nb[BK_KINDS], ldA[3], ldB[3], ncores, mycore;
static int sys_ns, stat_us, kv_us, fs_w, fs_r, pipe_k, tcp_k;

static int aot_rate(int kind)                    // iterations per second of one kernel run as AOT code on this core
{
    bk_prepare(sc);
    uint32_t t0 = sys_micros(), n = 0, acc = 0, d;
    do { for (int i = 0; i < 8; i++) acc ^= bk_iter(kind, sc, acc + n++); d = sys_micros() - t0; } while (d < (uint32_t)ms * 1000u);
    if (acc == 0x12345678u) m_puts("");
    return (int)((uint64_t)n * 1000000u / d);
}

static const char *fmt(int v, char *b)           // 1234567 -> "1.23M"
{
    if (v >= 1000000) m_snprintf(b, 12, "%d.%02dM", v / 1000000, v % 1000000 / 10000);
    else if (v >= 10000) m_snprintf(b, 12, "%d.%dk", v / 1000, v % 1000 / 100);
    else m_snprintf(b, 12, "%d", v);
    return b;
}

static void head(const char *t) { if (!json) m_printf("\n== %s ==\n", t); }

static void run_cpu(void)
{
    head("CPU: AOT code (what your programs get), core running it");
    if (!json) m_printf("%-8s %12s %14s %8s\n", "kernel", "iter/s", "units/s", "points");
    for (int k = 0; k < BK_KINDS; k++) {
        aot[k] = aot_rate(k);
        char a[12], b[12];
        if (!json) m_printf("%-8s %12s %9s %s %8d\n", NAMES[k], fmt(aot[k], a), fmt(aot[k] * UNITS[k], b), UNITN[k], (int)((int64_t)aot[k] * 1000 / REF[k]));
    }
}

static void run_native(void)
{
    head(ncores < 2 ? "CPU: native firmware code on the (only) core, vs the AOT result" : "CPU: native firmware code, core 0 / core 1 / both cores at once");
    if (!json) m_printf("%-8s %10s %10s %10s %7s %8s\n", "kernel", "core0", "core1", "both", "scale", "AOT/nat");
    for (int k = 0; k < BK_KINDS; k++) {
        int o[2] = { 0, 0 };
        sys_bench(k, 1, ms, o, 8); n0[k] = o[0];
        if (ncores > 1) { sys_bench(k, 2, ms, o, 8); n1[k] = o[1]; sys_bench(k, 3, ms, o, 8); nb[k] = o[0] + o[1]; }
        char a[12], b[12], c[12];
        if (!json) m_printf("%-8s %10s %10s %10s %5d%% %7d%%\n", NAMES[k], fmt(n0[k], a), ncores > 1 ? fmt(n1[k], b) : "-", ncores > 1 ? fmt(nb[k], c) : "-", n0[k] && ncores > 1 ? nb[k] * 100 / n0[k] : 0, n0[k] ? (int)((int64_t)aot[k] * 100 / n0[k]) : 0);
    }
}

static void run_load(void)
{
    head("Multitasking: AOT on this core while core 1 runs a memory-bound native job");
    static const int ks[3] = { 0, 3, 7 };
    if (ncores < 2) { if (!json) m_puts("one core only: skipped\n"); return; }
    if (!json) m_printf("%-8s %10s %10s %9s\n", "kernel", "alone", "with load", "slowdown");
    for (int i = 0; i < 3; i++) {
        int o[2];
        ldA[i] = aot_rate(ks[i]);
        sys_bench(3, 2 | 8, ms + 120, o, 8);     // memcpy hog on core 1
        ldB[i] = aot_rate(ks[i]);
        sys_bench(-1, 0, 0, o, 8);
        char a[12], b[12];
        if (!json) m_printf("%-8s %10s %10s %7d%%\n", NAMES[ks[i]], fmt(ldA[i], a), fmt(ldB[i], b), ldA[i] ? 100 - ldB[i] * 100 / ldA[i] : 0);
    }
}

static int rate_kbs(int bytes, uint32_t us) { return us ? (int)((uint64_t)bytes * 1000000u / us / 1024u) : 0; }

static void run_sys(void)
{
    head("OS services");
    uint32_t t0 = sys_micros();
    for (int i = 0; i < 4000; i++) sys_millis();
    sys_ns = (int)((sys_micros() - t0) * 1000u / 4000u);
    t0 = sys_micros();
    for (int i = 0; i < 100; i++) sys_stat("/bin");
    stat_us = (int)((sys_micros() - t0) / 100u);
    t0 = sys_micros();
    char b[16];
    for (int i = 0; i < 40; i++) sys_kv_get("bench.none", b, sizeof b);
    kv_us = (int)((sys_micros() - t0) / 40u);
    static char buf[1024];
    for (int i = 0; i < 1024; i++) buf[i] = (char)(i * 7);
    int fd = sys_open("/esp/.bench.tmp", 1);                           // 32 KB written then read back (LittleFS)
    if (fd >= 3) {
        t0 = sys_micros();
        for (int i = 0; i < 32; i++) sys_write(fd, buf, 1024);
        sys_close(fd);
        fs_w = rate_kbs(32768, sys_micros() - t0);
        fd = sys_open("/esp/.bench.tmp", 0);
        t0 = sys_micros();
        int got = 0, r;
        while ((r = sys_read(fd, buf, 1024)) > 0) got += r;
        sys_close(fd);
        fs_r = rate_kbs(got, sys_micros() - t0);
        sys_unlink("/esp/.bench.tmp");
    }
    fd = sys_membuf();                                                  // in-RAM pipe: 64 KB
    if (fd >= 3) {
        t0 = sys_micros();
        for (int i = 0; i < 64; i++) sys_write(fd, buf, 1024);
        sys_rewind(fd);
        int got = 0, r;
        while ((r = sys_read(fd, buf, 1024)) > 0) got += r;
        sys_close(fd);
        pipe_k = rate_kbs(got, sys_micros() - t0);
    }
    if (!json) {
        m_printf("syscall (sys_millis)  %6d ns\nstat() of a directory %6d us\nkv read (NVS)         %6d us\n", sys_ns, stat_us, kv_us);
        m_printf("file write (LittleFS) %6d KB/s\nfile read             %6d KB/s\nin-RAM pipe           %6d KB/s\n", fs_w, fs_r, pipe_k);
    }
}

static void run_net(void)
{
    head("Network: TCP over the loopback interface (lwIP stack, no radio)");
    int l = sys_tcp_listen(17099), c = l > 0 ? sys_tcp_connect("127.0.0.1", 17099, 1500) : -1, a = c > 0 ? sys_tcp_accept(l, 1500) : -1;
    static char buf[1024];
    if (a > 0) {
        sys_sock_timeout(a, 800);
        uint32_t t0 = sys_micros();
        int got = 0;
        for (int i = 0; i < 64; i++) {
            if (sys_write(c, buf, 1024) != 1024) break;
            int r = sys_read(a, buf, 1024);
            if (r <= 0) break;
            got += r;
        }
        tcp_k = rate_kbs(got, sys_micros() - t0);
    }
    if (a > 0) sys_close(a);
    if (c > 0) sys_close(c);
    if (l > 0) sys_close(l);
    if (!json) { if (tcp_k) m_printf("loopback throughput   %6d KB/s (1 KB ping-pong, %d KB)\n", tcp_k, 64); else m_puts("loopback test failed\n"); }
}

static int score(void)
{
    int s = 0;
    for (int k = 0; k < BK_KINDS; k++) s += (int)((int64_t)aot[k] * 1000 / REF[k]);
    return s / BK_KINDS;
}

static char js[1800];
static int jn;
static void ja(const char *f, ...) { va_list ap; va_start(ap, f); int w = m_vsnprintf(js + jn, (int)sizeof js - jn, f, ap); va_end(ap); jn += w < (int)sizeof js - jn ? w : 0; }
static void jarr(const char *n, const int *v, int c) { ja("\"%s\":[", n); for (int i = 0; i < c; i++) ja("%s%d", i ? "," : "", v[i]); ja("]"); }

static void make_json(void)
{
    char ver[64] = "", ip[24] = "";
    sys_version(ver, sizeof ver); sys_netinfo(1, ip, sizeof ip);
    ja("{\"ts\":%u,\"version\":\"%s\",\"mhz\":%d,\"cores\":%d,\"core\":%d,\"mode_ms\":%d,\"score\":%d,\"dram_free\":%d,\"psram\":%d,\"kinds\":[", sys_time(), ver, sys_sysinfo(SI_CPU_MHZ), ncores, mycore, ms, score(), sys_sysinfo(SI_DRAM_FREE), sys_sysinfo(SI_PSRAM_TOTAL));
    for (int k = 0; k < BK_KINDS; k++) ja("%s\"%s\"", k ? "," : "", NAMES[k]);
    ja("],\"units\":["); for (int k = 0; k < BK_KINDS; k++) ja("%s%d", k ? "," : "", UNITS[k]);
    ja("],\"ref\":["); for (int k = 0; k < BK_KINDS; k++) ja("%s%d", k ? "," : "", REF[k]);
    ja("],"); jarr("aot", aot, BK_KINDS); ja(","); jarr("core0", n0, BK_KINDS); ja(","); jarr("core1", n1, BK_KINDS); ja(","); jarr("both", nb, BK_KINDS);
    ja(","); jarr("load_alone", ldA, 3); ja(","); jarr("load_busy", ldB, 3);
    ja(",\"sys\":{\"syscall_ns\":%d,\"stat_us\":%d,\"kv_us\":%d,\"fs_write_kbs\":%d,\"fs_read_kbs\":%d,\"pipe_kbs\":%d,\"tcp_kbs\":%d}}\n", sys_ns, stat_us, kv_us, fs_w, fs_r, pipe_k, tcp_k);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!m_strcmp(a, "quick")) ms = 120;
        else if (!m_strcmp(a, "normal")) ms = 400;
        else if (!m_strcmp(a, "long")) ms = 1500;
        else if (!m_strcmp(a, "cpu")) t_cpu = 1;
        else if (!m_strcmp(a, "native")) t_nat = 1;
        else if (!m_strcmp(a, "load")) t_load = 1;
        else if (!m_strcmp(a, "sys")) t_sys = 1;
        else if (!m_strcmp(a, "net")) t_net = 1;
        else if (!m_strcmp(a, "--json")) json = 1;
        else if (!m_strcmp(a, "--save")) save = 1;
        else { m_eputs("usage: esp-bench [quick|normal|long] [cpu] [native] [load] [sys] [net] [--json] [--save]\n"); return 2; }
    }
    if (!(t_cpu | t_nat | t_load | t_sys | t_net)) t_cpu = t_nat = t_load = t_sys = t_net = 1;
    if (sys_abi() < 6) { m_eputs("esp-bench needs miniesp syscall ABI 6 (firmware with sys_bench): update the firmware\n"); return 1; }
    ncores = sys_sysinfo(SI_CORES); mycore = sys_sysinfo(SI_CORE);
    sys_sigint(1);
    if (!json) {
        char ver[64] = "";
        sys_version(ver, sizeof ver);
        m_printf("esp-bench  %s\nCPU %d MHz, %d core(s), program runs on core %d, DRAM free %d KB, PSRAM %d KB, %d ms per test\n", ver, sys_sysinfo(SI_CPU_MHZ), ncores, mycore, sys_sysinfo(SI_DRAM_FREE) / 1024, sys_sysinfo(SI_PSRAM_TOTAL) / 1024, ms);
        m_puts("Stop other programs first: services (e.g. wtms) skip their runs while this one holds the runtime.\n");
    }
    if (t_cpu || t_nat || t_load) { for (int k = 0; k < BK_KINDS; k++) aot[k] = 0; }
    if (t_cpu | t_nat | t_load | save) run_cpu();       // AOT rates are the base of the score, the AOT/native ratio and the load test
    if (t_nat) run_native();
    if (t_load) run_load();
    if (t_sys) run_sys();
    if (t_net) run_net();
    if (!json) m_printf("\nScore: %d points (1000 = reference ESP32 @ 240 MHz; AOT code, mean of %d kernels)\n", score(), BK_KINDS);
    if (json || save) make_json();
    if (json) m_puts(js);
    if (save) {
        sys_mkdir("/www/bench");
        int fd = sys_open("/www/bench/last.json", 1);
        if (fd >= 3) { sys_write(fd, js, jn); sys_close(fd); }
        fd = sys_open("/www/bench/history.csv", 2);
        if (fd >= 3) { char l[96]; int n = m_snprintf(l, sizeof l, "%u,%d,%d,%d,%d,%d,%d\n", sys_time(), score(), ms, ncores, aot[0], aot[1], nb[0]); sys_write(fd, l, n); sys_close(fd); }
        if (!json) m_puts("saved: /www/bench/last.json (web page: /bench/)\n");
    }
    return 0;
}
