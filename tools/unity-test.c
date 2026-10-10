/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for the Unity driver: boots an APK's Unity player on a Mac.
 *
 *   unity-test <apk> [seconds]
 *
 * Environment: TL_JNI_TRACE=1|2 (log JNI lookups / every call), TL_SKIP=a.so,...
 */
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include "husk-tl-jni.h"
#include "husk-tl-ld.h"
#include "husk-tl-unity.h"
#include "husk-tl-gamepad.h"
#include "husk-tl-xmem.h"
#include "husk-tl-audio.h"

static void dump_pushes(void);

void tl_log_line(const char *fmt, ...)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static struct timespec t0;
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    pthread_mutex_lock(&m);
    if (!t0.tv_sec) t0 = t;
    if (getenv("TL_LOG_TIME")) fprintf(stderr, "[%7.3f] ", (t.tv_sec - t0.tv_sec) + (t.tv_nsec - t0.tv_nsec) / 1e9);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap);
    pthread_mutex_unlock(&m);
}

static void describe(const char *label, const void *addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &lib, &sa);
    if (lib) fprintf(stderr, "  %-6s %p  %s  %s+%#lx\n", label, addr, lib, sym ? sym : "?", sa ? (unsigned long)((const char *)addr - (const char *)sa) : 0ul);
    else fprintf(stderr, "  %-6s %p\n", label, addr);
}

static void on_dump(int sig, siginfo_t *info, void *uctx)
{
    (void)sig; (void)info;
    ucontext_t *uc = uctx;
    _STRUCT_ARM_THREAD_STATE64 *ss = &uc->uc_mcontext->__ss;
    fprintf(stderr, "\n--- UnityMain, interrupted ---\n");
    describe("pc", (void *)ss->__pc);
    describe("lr", (void *)ss->__lr);
    fprintf(stderr, "  x19=%#llx x20=%#llx x21=%#llx x22=%#llx x23=%#llx x24=%#llx x25=%#llx x26=%#llx\n",
            ss->__x[19], ss->__x[20], ss->__x[21], ss->__x[22], ss->__x[23], ss->__x[24], ss->__x[25], ss->__x[26]);
    if (getenv("TL_DUMP_MEM")) {
        uint64_t *m = (uint64_t *)ss->__x[21];
        fprintf(stderr, "  [x21 free list head]   = %#llx  (+8: %#llx)\n", (unsigned long long)m[0], (unsigned long long)m[1]);
        m = (uint64_t *)ss->__x[23];
        fprintf(stderr, "  [x23 counter]          = %#llx\n", (unsigned long long)m[0]);
        fprintf(stderr, "  [x24] byte = %#x   [x25] word = %#x\n", *(uint8_t *)ss->__x[24], *(uint32_t *)ss->__x[25]);
        {
            uint64_t *q = (uint64_t *)ss->__x[21];
            for (int i = 0; i < 20; i++) fprintf(stderr, "  list[+%#x] = %#llx\n", i * 8, (unsigned long long)q[i]);
            uint64_t head = q[8] & ~1ull, tail = q[16];
            fprintf(stderr, "  head node %#llx (locked bit %llu), tail %#llx\n", (unsigned long long)head, (unsigned long long)(q[8] & 1), (unsigned long long)tail);
            if (head > 0x100000000ull) for (int i = 0; i < 4; i++) fprintf(stderr, "  head[+%d] = %#llx\n", i * 8, (unsigned long long)((uint64_t *)head)[i]);
            if (tail > 0x100000000ull) for (int i = 0; i < 4; i++) fprintf(stderr, "  tail[+%d] = %#llx\n", i * 8, (unsigned long long)((uint64_t *)tail)[i]);
        }
        {
            /* the chunk the head node lives in: 16 KiB aligned, block size in its first word */
            uint64_t head = ((uint64_t *)ss->__x[21])[8] & ~1ull, tail = ((uint64_t *)ss->__x[21])[16];
            uint64_t chunk = head & ~0x3fffull;
            int bs = *(int *)chunk;
            fprintf(stderr, "  chunk %#llx: block size %d, header words %#x %#x\n", (unsigned long long)chunk, bs, ((int *)chunk)[1], ((int *)chunk)[2]);
            uint64_t first = (chunk + 0x13 + 15) & ~15ull;      /* where the carve loop starts */
            for (uint64_t n = first; bs > 0 && n + bs <= chunk + 0x4000; n += (uint64_t)((bs + 15) & ~15)) {
                uint64_t nx = *(uint64_t *)n;
                fprintf(stderr, "    node %#llx -> %#llx%s%s\n", (unsigned long long)n, (unsigned long long)nx,
                        n == head ? "   <== HEAD" : "", n == tail ? "   <== TAIL" : "");
                if (n > first + 40ull * (uint64_t)((bs + 15) & ~15)) break;
            }
        }
        {
            uint64_t n = ((uint64_t *)ss->__x[21])[8] & ~1ull;
            fprintf(stderr, "  chain from head:");
            for (int i = 0; i < 40 && n > 0x100000000ull; i++) { fprintf(stderr, " %#llx", (unsigned long long)n); n = *(uint64_t *)n; }
            fprintf(stderr, " -> end %#llx\n", (unsigned long long)n);
        }
        uint64_t *base = (uint64_t *)ss->__x[19];
        for (int i = 0; i < 12; i++) fprintf(stderr, "  allocator[+%#x] = %#llx\n", i * 8, (unsigned long long)base[i]);
    }
    /* Frame pointers are not reliable in this code, so scan the stack for return addresses. */
    uint64_t *sp = (uint64_t *)ss->__sp;
    int shown = 0;
    for (int i = 0; i < 4096 && shown < 40; i++) {
        uint64_t v = sp[i];
        if (v > 0x7000000000ull && v < 0x7100000000ull && tl_ld_lib_of((void *)v) && tl_xmem_is_rx((void *)v) && (v & 3) == 0) { describe("ret", (void *)v); shown++; }
    }
    fflush(stderr);
}

static int safe_read(uintptr_t addr, void *out, size_t n)
{
    vm_size_t got = 0;
    return vm_read_overwrite(mach_task_self(), (vm_address_t)addr, n, (vm_address_t)out, &got) == KERN_SUCCESS && got == n;
}

static void on_crash(int sig, siginfo_t *info, void *uctx)
{
    ucontext_t *uc = uctx;
    _STRUCT_ARM_THREAD_STATE64 *ss = &uc->uc_mcontext->__ss;
    char tn[32] = ""; pthread_getname_np(pthread_self(), tn, sizeof(tn));
    fprintf(stderr, "\n=== CRASH: signal %d, fault address %p, thread '%s' ===\n", sig, info->si_addr, tn);
    describe("pc", (void *)ss->__pc);
    describe("lr", (void *)ss->__lr);
    if (info->si_addr) describe("fault", info->si_addr);
    uintptr_t fp = ss->__fp;
    for (int i = 0; i < 14 && fp && (fp & 7) == 0; i++) {
        uint64_t fr[2];
        if (!safe_read(fp, fr, sizeof(fr))) break;
        describe("frame", (void *)fr[1]);
        fp = fr[0];
    }
    for (int i = 0; i < 29; i += 4)
        fprintf(stderr, "  x%d=%#llx x%d=%#llx x%d=%#llx x%d=%#llx\n", i, ss->__x[i], i + 1, ss->__x[i + 1], i + 2, i + 2 < 29 ? ss->__x[i + 2] : 0, i + 3, i + 3 < 29 ? ss->__x[i + 3] : 0);
    fprintf(stderr, "  sp=%#llx fp=%#llx\n", ss->__sp, ss->__fp);
    uint64_t dump_base = getenv("TL_CRASH_DUMP_REG") ? ss->__x[atoi(getenv("TL_CRASH_DUMP_REG"))] : ss->__x[0];
    for (int i = 0; i < 0x120; i += 16) {
        uint64_t q[2];
        if (!safe_read(dump_base + i, q, sizeof(q))) break;
        fprintf(stderr, "  [%#llx+%#x] %016llx %016llx\n", (unsigned long long)dump_base, i, (unsigned long long)q[0], (unsigned long long)q[1]);
    }
    fflush(stderr);
    _exit(139);
}

/* Watches Unity's bucket-1 free list for the inconsistency that wedges it, and says when it appears. */
static uintptr_t g_unity_rw;
static void *monitor(void *arg)
{
    (void)arg;
    uint64_t *list = (uint64_t *)(g_unity_rw + 0x1265af0);
    uint64_t last_head = 0, last_tail = 0, last_next = ~0ull;
    for (;;) {
        usleep(50);
        uint64_t headw = list[8], head = headw & ~1ull, tail = list[16];
        uint64_t next = head > 0x100000000ull ? *(uint64_t *)head : 0;
        if (getenv("TL_MONITOR_VERBOSE") && (headw != last_head || tail != last_tail || next != last_next)) {
            tl_log_line("MONITOR: head=%#llx(%llu) next=%#llx tail=%#llx", (unsigned long long)head, (unsigned long long)(headw & 1), (unsigned long long)next, (unsigned long long)tail);
            last_head = headw; last_tail = tail; last_next = next;
        }
        if (head > 0x100000000ull && *(uint64_t *)head == 0 && tail && tail != head && !(headw & 1)) {
            usleep(20000);                                   /* a transient: confirm it persists */
            if (list[8] != headw || list[16] != tail || *(uint64_t *)head != 0) continue;
            dump_pushes();
            tl_log_line("MONITOR: queue inconsistent: head=%#llx next=0 tail=%#llx", (unsigned long long)head, (unsigned long long)tail);
            tl_log_line("MONITOR: chunk %#llx first words: %#llx %#llx %#llx %#llx", (unsigned long long)(head & ~0x3fffull),
                        (unsigned long long)((uint64_t *)(head & ~0x3fffull))[0], (unsigned long long)((uint64_t *)(head & ~0x3fffull))[1],
                        (unsigned long long)((uint64_t *)(head & ~0x3fffull))[2], (unsigned long long)((uint64_t *)(head & ~0x3fffull))[3]);
            return NULL;
        }
    }
}

static int find_unity(uintptr_t bias, const char *name, const void *phdr, unsigned phnum, void *user)
{
    (void)phdr; (void)phnum; (void)user;
    if (!strcmp(name, "libunity.so")) g_unity_rw = (uintptr_t)((char *)bias + tl_xmem_delta());
    return 0;
}

/* Probes on Unity's bucket free-list push/pop sites: record every operation on 32-byte blocks. */
#define NEV 200000
static struct { uint16_t site, tid; uint64_t node, prev; } g_ev[NEV];
static _Atomic int g_nev;
static const char *g_site_names[] = { "carve", "link1", "head1", "link2", "link3", "head3", "link4", "head4", "link5", "head5", "pop" };
static pthread_t g_tids[32]; static char g_tnames[32][20]; static _Atomic int g_ntid;
static int tid_index(void)
{
    pthread_t me = pthread_self();
    int n = atomic_load(&g_ntid);
    for (int i = 0; i < n && i < 32; i++) if (pthread_equal(g_tids[i], me)) return i;
    int k = atomic_fetch_add(&g_ntid, 1);
    if (k >= 32) return 31;
    g_tids[k] = me; pthread_getname_np(me, g_tnames[k], sizeof(g_tnames[k]));
    return k;
}
/* Shadow model of the 32-byte bucket queue: after every operation, check that each queued node's memory still says what the model says. */
#define MODEL_N 8192
static struct { uint64_t node, exp_next; uint8_t inq, reported; } g_model[MODEL_N];
static uint64_t g_model_tail;
static pthread_mutex_t g_model_mu = PTHREAD_MUTEX_INITIALIZER;
static struct { uint64_t node, found, exp; int ev; } g_corrupt[64]; static int g_ncorrupt;
static int model_slot(uint64_t node)
{
    unsigned h = (unsigned)((node >> 5) * 2654435761u) & (MODEL_N - 1);
    while (g_model[h].node && g_model[h].node != node) h = (h + 1) & (MODEL_N - 1);
    g_model[h].node = node;
    return (int)h;
}
static void model_check(int evi)
{
    for (int h = 0; h < MODEL_N; h++) {
        if (!g_model[h].node || !g_model[h].inq || g_model[h].reported) continue;
        if (g_model[h].node == g_model_tail) continue;
        uint64_t got = *(volatile uint64_t *)g_model[h].node;
        if (got != g_model[h].exp_next) {
            g_model[h].reported = 1;
            if (g_ncorrupt < 64) { g_corrupt[g_ncorrupt].node = g_model[h].node; g_corrupt[g_ncorrupt].found = got; g_corrupt[g_ncorrupt].exp = g_model[h].exp_next; g_corrupt[g_ncorrupt].ev = evi; g_ncorrupt++; }
        }
    }
}
/* The probe fires before the instruction at the site runs, so the previous operation is complete by now: check, then apply this one. */
static void model_apply(int site, uint64_t node, int evi)
{
    pthread_mutex_lock(&g_model_mu);
    model_check(evi);
    if (site == 10) { g_model[model_slot(node)].inq = 0; }
    else {
        int k = model_slot(node);
        g_model[k].exp_next = 0; g_model[k].inq = 1; g_model[k].reported = 0;
        if (g_model_tail) g_model[model_slot(g_model_tail)].exp_next = node;
        g_model_tail = node;
    }
    pthread_mutex_unlock(&g_model_mu);
}
volatile uint64_t g_watch_addr;
static void record(int site, uint64_t node, uint64_t prev)
{
    if (!node) return;
    if (*(volatile int32_t *)(node & ~0x3fffull) != 32) return;
    int i = atomic_fetch_add(&g_nev, 1);
    { static int watch_ev = -2; if (watch_ev == -2) watch_ev = getenv("TL_WATCH_EV") ? atoi(getenv("TL_WATCH_EV")) : -1;
      if (i == watch_ev) { g_watch_addr = node + 0x20; fprintf(stderr, "WATCH addr=%#llx (event %d, node %#llx)\n", (unsigned long long)g_watch_addr, i, (unsigned long long)node); raise(SIGSTOP); } }
    model_apply(site, node, i);
    if (i < NEV) { g_ev[i].site = (uint16_t)site; g_ev[i].tid = (uint16_t)tid_index(); g_ev[i].node = node; g_ev[i].prev = prev; }
}
#define PROBE(sitei, nodereg, tgtreg) static void probe_##sitei(uint64_t *r) { record(sitei, r[nodereg], r[tgtreg]); }
PROBE(0, 8, 13)   PROBE(1, 19, 9)   PROBE(2, 19, 8)   PROBE(3, 1, 8)   PROBE(4, 19, 10)
PROBE(5, 19, 8)   PROBE(6, 1, 9)    PROBE(7, 1, 8)    PROBE(8, 19, 10)  PROBE(9, 19, 8)
static void probe_10(uint64_t *r) { record(10, r[0], 0); }

static void dump_pushes(void)
{
    int n = atomic_load(&g_nev); if (n > NEV) n = NEV;
    int nt = atomic_load(&g_ntid); if (nt > 32) nt = 32;
    for (int i = 0; i < nt; i++) fprintf(stderr, "EVTHREAD %d %s\n", i, g_tnames[i]);
    fprintf(stderr, "EVLOG: %d events\n", n);
    for (int i = 0; i < g_ncorrupt; i++) fprintf(stderr, "CORRUPT node=%#llx expected-next=%#llx found=%#llx found-before-event %d\n", (unsigned long long)g_corrupt[i].node, (unsigned long long)g_corrupt[i].exp, (unsigned long long)g_corrupt[i].found, g_corrupt[i].ev);
    for (int i = 0; i < n; i++)
        fprintf(stderr, "EV %d %s t%d node=%#llx prev=%#llx\n", i, g_site_names[g_ev[i].site], g_ev[i].tid, (unsigned long long)g_ev[i].node, (unsigned long long)g_ev[i].prev);
}

static void probe_icall_missing(uint64_t *r) { fprintf(stderr, "PROBE icall not resolved: %s\n", (const char *)r[19]); }
static void probe_cxa_throw(uint64_t *r) { fprintf(stderr, "PROBE __cxa_throw(obj=%#llx tinfo=%#llx dtor=%#llx) lr=%#llx\n", (unsigned long long)r[0], (unsigned long long)r[1], (unsigned long long)r[2], (unsigned long long)r[30]); }
static void install_icall_probes(void)
{
    tl_lib *L = tl_ld_find_lib("libil2cpp.so");
    if (!L) return;
    if (!tl_ld_probe(L, 0x1e6eb44, probe_icall_missing)) fprintf(stderr, "probe failed (icall)\n");
    if (!tl_ld_probe(L, 0x1edb0e0, probe_cxa_throw)) fprintf(stderr, "probe failed (throw)\n");
}

static void install_probes(void)
{
    tl_lib *L = tl_ld_find_lib("libunity.so");
    struct { uint64_t off; void (*cb)(uint64_t *); } ps[] = {
        { 0x497464, probe_0 }, { 0x4971dc, probe_1 }, { 0x4971ec, probe_2 }, { 0x498324, probe_3 }, { 0x4988e8, probe_4 },
        { 0x4988f8, probe_5 }, { 0x4989a4, probe_6 }, { 0x4989b4, probe_7 }, { 0x499c1c, probe_8 }, { 0x499c2c, probe_9 },
        { 0x49e8d0, probe_10 },
    };
    for (size_t i = 0; i < sizeof(ps) / sizeof(ps[0]); i++) if (!tl_ld_probe(L, ps[i].off, ps[i].cb)) fprintf(stderr, "probe at %#llx failed\n", (unsigned long long)ps[i].off);
}

/* TL_TOUCH="ms:phase:id:x,y;..." replays touches at those times (ms since start); phase 0 down, 1 move, 2 up. */
static void *touch_script(void *arg)
{
    const char *p = arg;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    while (p && *p) {
        long ms; int phase, id; float x, y; int n = 0;
        if (sscanf(p, "%ld:%d:%d:%f,%f%n", &ms, &phase, &id, &x, &y, &n) < 5) break;
        for (;;) {
            struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
            long el = (t.tv_sec - t0.tv_sec) * 1000 + (t.tv_nsec - t0.tv_nsec) / 1000000;
            if (el >= ms) break;
            usleep(5000);
        }
        fprintf(stderr, "touch: phase %d id %d at %.0f,%.0f\n", phase, id, x, y);
        tl_unity_touch(phase, id, x, y);
        p += n;
        while (*p == ';' || *p == ' ') p++;
    }
    return NULL;
}

/* TL_CTL=/path/to/fifo: lines "tap X Y", "hold X Y MS", "swipe X1 Y1 X2 Y2 MS", "shot PNGPATH", "quit". */
static const char *g_frame_dir;
static void sleep_ms(long ms) { usleep((useconds_t)ms * 1000); }
static void do_swipe(float x1, float y1, float x2, float y2, long ms)
{
    int steps = (int)(ms / 16); if (steps < 2) steps = 2;
    tl_unity_touch(0, 0, x1, y1);
    for (int i = 1; i <= steps; i++) { sleep_ms(ms / steps); tl_unity_touch(1, 0, x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps); }
    tl_unity_touch(2, 0, x2, y2);
}
static void *control_thread(void *arg)
{
    const char *path = arg;
    for (;;) {
        FILE *f = fopen(path, "r");
        if (!f) { sleep_ms(200); continue; }
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            float a, b, c, d; long ms; char p[400];
            if (sscanf(line, "tap %f %f", &a, &b) == 2) { tl_unity_touch(0, 0, a, b); sleep_ms(60); tl_unity_touch(2, 0, a, b); }
            else if (sscanf(line, "hold %f %f %ld", &a, &b, &ms) == 3) { tl_unity_touch(0, 0, a, b); sleep_ms(ms); tl_unity_touch(2, 0, a, b); }
            else if (sscanf(line, "swipe %f %f %f %f %ld", &a, &b, &c, &d, &ms) == 5) do_swipe(a, b, c, d, ms);
            /* a virtual controller: padon / padoff, "pad <buttons hex> lx ly rx ry lt rt" (y down, as Android), padtap <button bit> <ms> */
            else if (!strncmp(line, "padon", 5)) tl_pad_connect(0, "Xbox Wireless Controller");
            else if (!strncmp(line, "padoff", 6)) tl_pad_disconnect(0);
            else if (!strncmp(line, "padtap", 6) && sscanf(line + 6, "%f %ld", &a, &ms) == 2) {
                tl_pad_state st = { .buttons = 1u << (int)a }; tl_pad_update(0, &st); sleep_ms(ms); st.buttons = 0; tl_pad_update(0, &st);
            }
            else if (!strncmp(line, "pad ", 4)) {
                unsigned btn = 0; tl_pad_state st = { 0 };
                if (sscanf(line + 4, "%x %f %f %f %f %f %f", &btn, &st.lx, &st.ly, &st.rx, &st.ry, &st.lt, &st.rt) >= 1) { st.buttons = btn; tl_pad_update(0, &st); }
            }
            else if (sscanf(line, "wait %ld", &ms) == 1) sleep_ms(ms);
            else if (sscanf(line, "shot %399s", p) == 1) {
                char cmd[900]; snprintf(cmd, sizeof(cmd), "sips -s format png '%s/latest.bmp' --out '%s' >/dev/null 2>&1", g_frame_dir, p);
                if (system(cmd)) fprintf(stderr, "ctl: shot failed\n");
                else fprintf(stderr, "ctl: shot %s (frame %lu)\n", p, tl_unity_frames());
            }
            else if (!strncmp(line, "quit", 4)) { fprintf(stderr, "ctl: quit\n"); fflush(stderr); _exit(0); }
        }
        fclose(f);
    }
    return NULL;
}

static int print_lib(uintptr_t bias, const char *name, const void *phdr, unsigned phnum, void *user)
{
    (void)phdr; (void)phnum; (void)user;
    fprintf(stderr, "LIB %s %#lx\n", name, (unsigned long)bias);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <apk> [seconds] [width height]\n", argv[0]); return 2; }
    static uint8_t altstack[1 << 16];
    stack_t ss = { .ss_sp = altstack, .ss_size = sizeof(altstack) };
    sigaltstack(&ss, NULL);
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash; sa.sa_flags = SA_SIGINFO | SA_ONSTACK; sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL); sigaction(SIGILL, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL); sigaction(SIGABRT, &sa, NULL);

    { struct sigaction sd; memset(&sd, 0, sizeof(sd)); sd.sa_sigaction = on_dump; sd.sa_flags = SA_SIGINFO | SA_ONSTACK; sigemptyset(&sd.sa_mask); sigaction(SIGUSR2, &sd, NULL); }
    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 1);
    tl_jni_set_trace(getenv("TL_JNI_TRACE") ? atoi(getenv("TL_JNI_TRACE")) : 1);
    char tmp[] = "/Volumes/GTAV/husk2/tmp/husk-unity-XXXXXX";
    mkdtemp(tmp);
    const char *cef = "/Users/davi/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Chromium Embedded Framework.framework/Versions/A/Libraries";
    char egl[600], gles[600]; snprintf(egl, sizeof(egl), "%s/libEGL.dylib", cef); snprintf(gles, sizeof(gles), "%s/libGLESv2.dylib", cef);
    char frames[] = "/Volumes/GTAV/husk2/tmp/husk-frames-XXXXXX"; mkdtemp(frames);
    fprintf(stderr, "frames: %s\n", frames);
    /* TL_SPLITS: the app's split APKs, colon-separated (a Google Play install: libraries and asset packs in their own APKs) */
    if (getenv("TL_SPLITS")) { char *l = strdup(getenv("TL_SPLITS")); for (char *t = strtok(l, ":"); t; t = strtok(NULL, ":")) tl_ld_queue_split(t); }
    tl_unity_config cfg = { .apk_path = argv[1], .data_dir = tmp, .package_name = getenv("TL_PACKAGE") ? getenv("TL_PACKAGE") : "com.kiloo.subwaysurf", .width = argc > 4 ? atoi(argv[3]) : 540, .height = argc > 4 ? atoi(argv[4]) : 1200,
                            .angle_egl = getenv("TL_ANGLE_EGL") ? getenv("TL_ANGLE_EGL") : egl, .angle_gles = getenv("TL_ANGLE_GLES") ? getenv("TL_ANGLE_GLES") : gles,
                            .frame_dir = frames, .frame_every = getenv("TL_CTL") ? -6 : 30 };
    g_frame_dir = frames;
    tl_audio_install();   /* as the app does: the game's sound goes to the speakers (TL_AUDIO_MUTE=1 for a silent run) */
    if (!tl_unity_start(&cfg)) { fprintf(stderr, "unity: start failed\n"); return 1; }
    if (getenv("TL_PROBES")) install_probes();
    if (getenv("TL_ICALL_PROBES")) install_icall_probes();
    if (getenv("TL_STOP_EARLY")) { tl_ld_iterate(print_lib, NULL); raise(SIGSTOP); }
    if (!tl_unity_run()) { fprintf(stderr, "unity: run failed\n"); return 1; }
    tl_ld_iterate(print_lib, NULL);
    tl_unity_register_pad_sink();
    if (getenv("TL_PAD")) tl_pad_connect(0, "Xbox Wireless Controller");
    if (getenv("TL_CTL")) { static pthread_t ct; pthread_create(&ct, NULL, control_thread, getenv("TL_CTL")); }
    if (getenv("TL_TOUCH")) { static pthread_t tt; pthread_create(&tt, NULL, touch_script, getenv("TL_TOUCH")); }
    if (getenv("TL_MONITOR")) { tl_ld_iterate(find_unity, NULL); pthread_t mt; pthread_create(&mt, NULL, monitor, NULL); }
    int secs = argc > 2 ? atoi(argv[2]) : 5;
    if (getenv("TL_STOP_AT")) { sleep((unsigned)atoi(getenv("TL_STOP_AT"))); raise(SIGSTOP); }
    if (getenv("TL_POKE_AT")) { sleep((unsigned)atoi(getenv("TL_POKE_AT"))); tl_unity_poke(SIGUSR2); usleep(300000); secs -= atoi(getenv("TL_POKE_AT")); }
    if (secs > 0) sleep((unsigned)secs);
    fprintf(stderr, "unity: %lu frames in %d s\n", tl_unity_frames(), secs);
    tl_unity_stop();
    return 0;
}
