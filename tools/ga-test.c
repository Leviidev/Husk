/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for the GameActivity driver: runs an APK's GameActivity game (Minecraft) on a Mac, off-screen.
 *
 *   ga-test <apk> [seconds] [width height]
 *
 * Landscape by default (the phone's aspect). Environment: TL_JNI_TRACE=1|2, TL_VERBOSE=0..2,
 * TL_CTL=<fifo> (lines "tap X Y", "hold X Y MS", "swipe X1 Y1 X2 Y2 MS", "wait MS", "shot PNG", "quit").
 */
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/thread_act.h>
#include <mach/arm/thread_status.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-gameactivity.h"
#include "husk-tl-gamepad.h"
#include "husk-tl-audio.h"
#include "husk-tl-xmem.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"

void tl_log_line(const char *fmt, ...)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static struct timespec t0;
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    pthread_mutex_lock(&m);
    if (!t0.tv_sec) t0 = t;
    char line[4096];
    int n = 0;
    if (getenv("TL_LOG_TIME")) n = snprintf(line, sizeof(line), "[%7.3f] ", (t.tv_sec - t0.tv_sec) + (t.tv_nsec - t0.tv_nsec) / 1e9);
    va_list ap; va_start(ap, fmt);
    int w = vsnprintf(line + n, sizeof(line) - (size_t)n - 2, fmt, ap);
    va_end(ap);
    if (w > (int)(sizeof(line) - (size_t)n - 3)) w = (int)(sizeof(line) - (size_t)n - 3);
    n += w;
    line[n++] = '\n';
    fwrite(line, 1, (size_t)n, stderr);
    pthread_mutex_unlock(&m);
}

void tl_egl_recent_calls(char *out, size_t n, int count);

static void describe(const char *label, const void *addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &lib, &sa);
    if (lib) fprintf(stderr, "  %-6s %p  %s  %s+%#lx\n", label, addr, lib, sym ? sym : "?", sa ? (unsigned long)((const char *)addr - (const char *)sa) : 0ul);
    else {
        Dl_info di;
        if (dladdr(addr, &di) && di.dli_fname) fprintf(stderr, "  %-6s %p  [host] %s  %s+%#lx\n", label, addr, strrchr(di.dli_fname, '/') ? strrchr(di.dli_fname, '/') + 1 : di.dli_fname, di.dli_sname ? di.dli_sname : "?", di.dli_saddr ? (unsigned long)((const char *)addr - (const char *)di.dli_saddr) : 0ul);
        else fprintf(stderr, "  %-6s %p\n", label, addr);
    }
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
    for (int i = 0; i < 16 && fp && (fp & 7) == 0; i++) {
        uint64_t fr[2];
        if (!safe_read(fp, fr, sizeof(fr))) break;
        describe("frame", (void *)fr[1]);
        fp = fr[0];
    }
    uint64_t *sp = (uint64_t *)ss->__sp; int shown = 0;
    for (int i = 0; i < 8192 && shown < 24; i++) {
        uint64_t v;
        if (!safe_read((uintptr_t)(sp + i), &v, 8)) break;
        if (v > 0x7000000000ull && v < 0x7100000000ull && tl_ld_lib_of((void *)v) && (v & 3) == 0) { describe("stk", (void *)v); shown++; }
    }
    for (int i = 0; i < 29; i += 4)
        fprintf(stderr, "  x%d=%#llx x%d=%#llx x%d=%#llx x%d=%#llx\n", i, ss->__x[i], i + 1, ss->__x[i + 1], i + 2, i + 2 < 29 ? ss->__x[i + 2] : 0, i + 3, i + 3 < 29 ? ss->__x[i + 3] : 0);
    fprintf(stderr, "  sp=%#llx fp=%#llx\n", ss->__sp, ss->__fp);
    { char gl[2048]; tl_egl_recent_calls(gl, sizeof(gl), 40); if (gl[0]) fprintf(stderr, "  last GL calls: %s\n", gl); }
    fflush(stderr);
    _exit(139);
}

static const char *g_frame_dir;
static void sleep_ms(long ms) { usleep((useconds_t)ms * 1000); }
static void do_swipe(float x1, float y1, float x2, float y2, long ms)
{
    int steps = (int)(ms / 16); if (steps < 2) steps = 2;
    tl_ga_touch(0, 0, x1, y1);
    for (int i = 1; i <= steps; i++) { sleep_ms(ms / steps); tl_ga_touch(1, 0, x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps); }
    tl_ga_touch(2, 0, x2, y2);
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
            if (sscanf(line, "tap %f %f", &a, &b) == 2) { tl_ga_touch(0, 0, a, b); sleep_ms(80); tl_ga_touch(2, 0, a, b); }
            else if (sscanf(line, "hold %f %f %ld", &a, &b, &ms) == 3) { tl_ga_touch(0, 0, a, b); sleep_ms(ms); tl_ga_touch(2, 0, a, b); }
            else if (sscanf(line, "swipe %f %f %f %f %ld", &a, &b, &c, &d, &ms) == 5) do_swipe(a, b, c, d, ms);
            else if (sscanf(line, "wait %ld", &ms) == 1) sleep_ms(ms);
            else if (sscanf(line, "shot %399s", p) == 1) {
                char cmd[900]; snprintf(cmd, sizeof(cmd), "sips -s format png '%s/latest.bmp' --out '%s' >/dev/null 2>&1", g_frame_dir, p);
                if (system(cmd)) fprintf(stderr, "ctl: shot failed\n");
                else fprintf(stderr, "ctl: shot %s (frame %lu)\n", p, tl_ga_frames());
            }
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
            else if (!strncmp(line, "type ", 5)) { line[strcspn(line, "\n")] = 0; tl_ga_insert_text(line + 5); }
            else if (!strncmp(line, "bksp", 4)) tl_ga_delete_backward();
            else if (!strncmp(line, "enter", 5)) tl_ga_editor_action();
            else if (!strncmp(line, "pause", 5)) tl_ga_set_paused(true);
            else if (!strncmp(line, "resume", 6)) tl_ga_set_paused(false);
            else if (!strncmp(line, "quit", 4)) { fprintf(stderr, "ctl: quit\n"); fflush(stderr); _exit(0); }
        }
        fclose(f);
    }
    return NULL;
}

static int g_ran;
static void tl_ran(void) { g_ran++; }

/* Bedrock's assertion handler builds its message just before it decides to crash on purpose (store 0xdeadc0de to
 * address 0). A probe there prints the message, which the game itself only logs after the crash. */
static void assert_probe(uint64_t *r)
{
    const char *msg = (const char *)r[5];
    tl_log_line("ASSERTION FAILED: %.1500s", msg && (uintptr_t)msg > 0x100000 ? msg : "(no message)");
}

/* Bedrock's soft assertion (message, expression, line, file, function): logged and carried on from, so a probe shows which ones the game hits.
 * 0x150df538 is its entry in libminecraftpe 1.26.60.29. */
static void soft_assert_probe(uint64_t *r)
{
    static int n;
    if (n++ > 60) return;
    tl_log_line("SOFT ASSERT: %.120s | %.100s | line %d | %.90s | %.90s", (const char *)r[0], (const char *)r[1], (int)r[2], (const char *)r[3], (const char *)r[4]);
}

/* TL_CXA_THROW=1: say what C++ exception each throw is (type name, thrower and the exception's first words), for finding what a terminate was about. */
static void throw_probe(uint64_t *r)
{
    const void *ti = (const void *)r[1];
    const char *tn = ti ? *(const char *const *)((const char *)ti + 8) : "?";
    const char *ex = (const char *)r[0], *msg = "";
    char shown[100]; shown[0] = 0;
    /* libc++'s logic_error/runtime_error keep a pointer to the message just after the vtable */
    if (ex) { const char *const *q = (const char *const *)ex; for (int i = 1; i <= 2; i++) { const char *c = q[i]; if ((uintptr_t)c > 0x100000 && ((uintptr_t)c >> 40) < 0x1000) { size_t k = 0; while (k < 90 && c[k] >= 32 && c[k] < 127) { shown[k] = c[k]; k++; } shown[k] = 0; if (k > 3) { msg = shown; break; } } } }
    uint64_t lr; __asm__ volatile("mov %0, x30" : "=r"(lr));
    tl_log_line("C++ THROW: %s %s", tn ? tn : "?", msg);
}

/* RenderDragon's bgfx callback: fatal(code in x1, message in x2) */
static void fatal_probe(uint64_t *r)
{
    const char *msg = (const char *)r[2];
    tl_log_line("BGFX FATAL %d: %.1500s", (int)r[1], msg && (uintptr_t)msg > 0x100000 ? msg : "(no message)");
}

/* TL_SAMPLE=<seconds>: every so often, say where every thread is (the guest library and offset of its pc and lr). For finding what a stuck game is waiting for. */
static void *sampler(void *arg)
{
    int period = (int)(intptr_t)arg;
    for (;;) {
        sleep((unsigned)period);
        thread_act_array_t th; mach_msg_type_number_t n;
        if (task_threads(mach_task_self(), &th, &n) != KERN_SUCCESS) continue;
        fprintf(stderr, "---- sample: %u threads ----\n", n);
        { char gl[1500]; tl_egl_recent_calls(gl, sizeof(gl), 24); if (gl[0]) fprintf(stderr, "  last GL calls: %s\n", gl); }
        for (mach_msg_type_number_t i = 0; i < n; i++) {
            arm_thread_state64_t st; mach_msg_type_number_t cnt = ARM_THREAD_STATE64_COUNT;
            pthread_t pt = pthread_from_mach_thread_np(th[i]);
            char name[40] = "";
            if (pt) pthread_getname_np(pt, name, sizeof(name));
            if (th[i] == mach_thread_self()) continue;
            thread_suspend(th[i]);
            if (thread_get_state(th[i], ARM_THREAD_STATE64, (thread_state_t)&st, &cnt) == KERN_SUCCESS) {
                const char *lib = NULL; const void *sa = NULL;
                const char *sym = tl_ld_symbol_at((void *)arm_thread_state64_get_pc(st), &lib, &sa);
                const char *lib2 = NULL; const void *sa2 = NULL;
                const char *sym2 = tl_ld_symbol_at((void *)arm_thread_state64_get_lr(st), &lib2, &sa2);
                Dl_info di; const char *host = "";
                if (!lib && dladdr((void *)arm_thread_state64_get_pc(st), &di) && di.dli_sname) host = di.dli_sname;
                fprintf(stderr, "  [%-24s] pc %s%s+%#lx  lr %s%s+%#lx\n", name, lib ? lib : "[host] ", lib ? sym ? sym : "?" : host,
                        lib && sa ? (unsigned long)(arm_thread_state64_get_pc(st) - (uintptr_t)sa) : 0ul,
                        lib2 ? lib2 : "", lib2 ? sym2 ? sym2 : "?" : "",
                        lib2 && sa2 ? (unsigned long)(arm_thread_state64_get_lr(st) - (uintptr_t)sa2) : 0ul);
            }
            if (getenv("TL_SAMPLE_BT") && strstr(name, getenv("TL_SAMPLE_BT"))) {
                uintptr_t fp = arm_thread_state64_get_fp(st);
                for (int d = 0; d < 14 && fp && (fp & 7) == 0; d++) {
                    uint64_t fr[2];
                    if (!safe_read(fp, fr, sizeof(fr))) break;
                    const char *l3 = NULL; const void *s3 = NULL;
                    const char *sy3 = tl_ld_symbol_at((void *)fr[1], &l3, &s3);
                    fprintf(stderr, "      frame %s %s+%#lx\n", l3 ? l3 : "?", l3 && sy3 ? sy3 : "?", l3 && s3 ? (unsigned long)(fr[1] - (uintptr_t)s3) : 0ul);
                    fp = fr[0];
                }
            }
            thread_resume(th[i]);
        }
    }
    return NULL;
}


struct bias_q { const char *name; uintptr_t bias; };
static int bias_cb(uintptr_t bias, const char *name, const void *ph, unsigned n, void *u)
{ (void)ph; (void)n; struct bias_q *q = u; if (!strcmp(name, q->name)) { q->bias = bias; return 1; } return 0; }
static uintptr_t lib_bias(const char *name) { struct bias_q q = { name, 0 }; tl_ld_iterate(bias_cb, &q); return q.bias; }

/* TL_PROF=<thread-name-part>[:delay[:seconds]]: samples that thread every 2 ms and prints which call sites its stacks pass through most,
 * and the commonest whole stacks (guest return addresses, as library+offset). For finding what a busy-looking thread is doing. */
typedef struct { char key[1200]; int n; } prof_chain;
static void *profiler(void *arg)
{
    char spec[200]; snprintf(spec, sizeof(spec), "%s", (const char *)arg);
    int delay = 15, secs = 6;
    char *c1 = strchr(spec, ':');
    if (c1) { *c1++ = 0; delay = atoi(c1); char *c2 = strchr(c1, ':'); if (c2) secs = atoi(c2 + 1); }
    sleep((unsigned)delay);
    static prof_chain chains[4000]; int nch = 0;
    static struct { uintptr_t addr; int n; } sites[8000]; int ns = 0;
    int total = 0;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
        if ((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000 >= secs * 1000) break;
        usleep(1000);
        thread_act_array_t th; mach_msg_type_number_t n;
        if (task_threads(mach_task_self(), &th, &n) != KERN_SUCCESS) continue;
        for (mach_msg_type_number_t i = 0; i < n; i++) {
            pthread_t pt = pthread_from_mach_thread_np(th[i]);
            char name[40] = "";
            if (pt) pthread_getname_np(pt, name, sizeof(name));
            if (th[i] == mach_thread_self() || !strstr(name, spec)) continue;
            arm_thread_state64_t st; mach_msg_type_number_t cnt = ARM_THREAD_STATE64_COUNT;
            thread_suspend(th[i]);
            if (thread_get_state(th[i], ARM_THREAD_STATE64, (thread_state_t)&st, &cnt) == KERN_SUCCESS) {
                uintptr_t addrs[40]; int na = 0;
                addrs[na++] = arm_thread_state64_get_pc(st);
                addrs[na++] = arm_thread_state64_get_lr(st);
                uintptr_t fp = arm_thread_state64_get_fp(st);
                for (int d = 0; d < 36 && fp && (fp & 7) == 0; d++) {
                    uint64_t fr[2];
                    if (!safe_read(fp, fr, sizeof(fr))) break;
                    addrs[na++] = fr[1]; fp = fr[0];
                }
                char key[1200]; size_t k = 0; key[0] = 0;
                k += (size_t)snprintf(key, sizeof(key), "[%s] ", name);
                for (int a = 0; a < na; a++) {
                    const char *l = NULL; const void *sb = NULL;
                    const char *sy = tl_ld_symbol_at((void *)addrs[a], &l, &sb);
                    (void)sy;
                    if (!l) continue;
                    uintptr_t base = (uintptr_t)lib_bias(l);
                    if (k < sizeof(key) - 40) k += (size_t)snprintf(key + k, sizeof(key) - k, "%s+%lx ", l, (unsigned long)(addrs[a] - base));
                    int f = -1;
                    for (int q = 0; q < ns; q++) if (sites[q].addr == addrs[a]) { f = q; break; }
                    if (f < 0 && ns < 8000) { sites[ns].addr = addrs[a]; sites[ns].n = 0; f = ns++; }
                    if (f >= 0) sites[f].n++;
                }
                int f = -1;
                for (int q = 0; q < nch; q++) if (!strcmp(chains[q].key, key)) { f = q; break; }
                if (f < 0 && nch < 4000) { snprintf(chains[nch].key, sizeof(chains[nch].key), "%s", key); chains[nch].n = 0; f = nch++; }
                if (f >= 0) chains[f].n++;
                total++;
            }
            thread_resume(th[i]);
        }
    }
    fprintf(stderr, "---- profile of '%s': %d samples, %d call sites, %d distinct stacks ----\n", spec, total, ns, nch);
    for (int r = 0; r < 40; r++) {
        int best = -1;
        for (int q = 0; q < ns; q++) if (sites[q].n > 0 && (best < 0 || sites[q].n > sites[best].n)) best = q;
        if (best < 0) break;
        const char *l = NULL; const void *sb = NULL; const char *sy = tl_ld_symbol_at((void *)sites[best].addr, &l, &sb);
        (void)sy;
        fprintf(stderr, "  %5d  %s+%#lx\n", sites[best].n, l ? l : "?", l ? (unsigned long)(sites[best].addr - (uintptr_t)lib_bias(l)) : 0ul);
        sites[best].n = -sites[best].n;
    }
    for (int r = 0; r < 8; r++) {
        int best = -1;
        for (int q = 0; q < nch; q++) if (chains[q].n > 0 && (best < 0 || chains[q].n > chains[best].n)) best = q;
        if (best < 0) break;
        fprintf(stderr, "  stack x%d: %s\n", chains[best].n, chains[best].key);
        chains[best].n = -chains[best].n;
    }
    return NULL;
}

static void keyboard_hook(int action) { fprintf(stderr, "ctl: the game %s the keyboard\n", action == 1 ? "shows" : "hides"); }

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

    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 1);
    tl_jni_set_trace(getenv("TL_JNI_TRACE") ? atoi(getenv("TL_JNI_TRACE")) : 1);
    char tmp[] = "/Volumes/GTAV/husk2/tmp/husk-mc-XXXXXX";
    mkdtemp(tmp);
    const char *cef = "/Users/davi/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Chromium Embedded Framework.framework/Versions/A/Libraries";
    char egl[600], gles[600]; snprintf(egl, sizeof(egl), "%s/libEGL.dylib", cef); snprintf(gles, sizeof(gles), "%s/libGLESv2.dylib", cef);
    char frames[] = "/Volumes/GTAV/husk2/tmp/husk-mframes-XXXXXX"; mkdtemp(frames);
    fprintf(stderr, "frames: %s\ndata: %s\n", frames, tmp);
    int w = argc > 4 ? atoi(argv[3]) : 1200, h = argc > 4 ? atoi(argv[4]) : 552;
    tl_ga_config cfg = { .apk_path = argv[1], .data_dir = tmp, .package_name = "com.mojang.minecraftpe", .width = w, .height = h,
                         .angle_egl = getenv("TL_ANGLE_EGL") ? getenv("TL_ANGLE_EGL") : egl, .angle_gles = getenv("TL_ANGLE_GLES") ? getenv("TL_ANGLE_GLES") : gles,
                         .frame_dir = frames, .frame_every = getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : -6 };
    g_frame_dir = frames;
    if (getenv("TL_AUDIO")) tl_audio_install();
    if (getenv("TL_PAD")) tl_pad_connect(0, "Xbox Wireless Controller");
    tl_ga_set_keyboard_handler(keyboard_hook);
    if (!tl_ga_start(&cfg)) { fprintf(stderr, "minecraft: start failed\n"); return 1; }
    if (getenv("TL_MC_RAND_TEST")) {
        tl_lib *L = tl_ld_find_lib("libminecraftpe.so");
        int (*rb)(void *, int) = L ? tl_ld_sym(L, "RAND_bytes") : NULL;
        int (*status)(void) = L ? tl_ld_sym(L, "RAND_status") : NULL;
        unsigned long (*geterr)(void) = L ? tl_ld_sym(L, "ERR_get_error") : NULL;
        char *(*errstr)(unsigned long, char *) = L ? tl_ld_sym(L, "ERR_error_string") : NULL;
        unsigned char buf[16] = { 0 };
        int (*poll)(void) = L ? tl_ld_sym(L, "RAND_poll") : NULL;
        void *(*master)(void) = L ? tl_ld_sym(L, "RAND_DRBG_get0_master") : NULL;
        int (*inst)(void *, const unsigned char *, size_t) = L ? tl_ld_sym(L, "RAND_DRBG_instantiate") : NULL;
        { int (*ic)(uint64_t, void *) = L ? tl_ld_sym(L, "OPENSSL_init_crypto") : NULL;
          void *(*ln)(void) = L ? tl_ld_sym(L, "CRYPTO_THREAD_lock_new") : NULL;
          { int (*il)(void *, void *) = L ? tl_ld_sym(L, "CRYPTO_THREAD_init_local") : NULL; uint64_t key[2] = { 0xdead, 0xbeef };
            fprintf(stderr, "CRYPTO_THREAD_init_local=%d key=%llx next=%llx\n", il ? il(key, NULL) : -99, (unsigned long long)key[0], (unsigned long long)key[1]);
            key[0] = 0xdead;
            fprintf(stderr, "CRYPTO_THREAD_init_local(dtor)=%d key=%llx\n", il ? il(key, tl_ld_sym(L, "OPENSSL_cleanup")) : -99, (unsigned long long)key[0]); }
          { int (*ro)(void *, void (*)(void)) = L ? tl_ld_sym(L, "CRYPTO_THREAD_run_once") : NULL; static uint64_t once[2];
            fprintf(stderr, "CRYPTO_THREAD_run_once=%d ran=%d\n", ro ? ro(once, tl_ran) : -99, g_ran); }
          { uint8_t *bl = L ? tl_ld_sym(L, "bio_type_lock") : NULL;      /* bio_type_lock is at vaddr 0x15b92528 */
            if (bl) { uint8_t *b = bl + (0x15b94fb0 - 0x15b92528);
              fprintf(stderr, "openssl state @0x15b94fb0:"); for (int i = 0; i < 0x40; i++) { if (i % 16 == 0) fprintf(stderr, "\n  +%02x:", i); fprintf(stderr, " %02x", b[i]); } fprintf(stderr, "\n"); } }
          fprintf(stderr, "OPENSSL_init_crypto(0)=%d\n", ic ? ic(0, NULL) : -99);
          { uint8_t *bl2 = L ? tl_ld_sym(L, "bio_type_lock") : NULL;
            if (bl2) { uint8_t *b = bl2 + (0x15b94fb0 - 0x15b92528);
              fprintf(stderr, "openssl state after:"); for (int i = 0; i < 0x40; i++) { if (i % 16 == 0) fprintf(stderr, "\n  +%02x:", i); fprintf(stderr, " %02x", b[i]); } fprintf(stderr, "\n"); } }
          void *lk = ln ? ln() : NULL; fprintf(stderr, "CRYPTO_THREAD_lock_new=%p\n", lk);
          int (*rl)(void *) = L ? tl_ld_sym(L, "CRYPTO_THREAD_read_lock") : NULL;
          fprintf(stderr, "read_lock=%d\n", rl && lk ? rl(lk) : -99);
          void *(*zalloc)(size_t, const char *, int) = L ? tl_ld_sym(L, "CRYPTO_zalloc") : NULL;
          fprintf(stderr, "CRYPTO_zalloc(64)=%p\n", zalloc ? zalloc(64, "t", 1) : NULL); }
        fprintf(stderr, "RAND_status=%d\n", status ? status() : -99);
        fprintf(stderr, "RAND_poll=%d\n", poll ? poll() : -99);
        void *m = master ? master() : NULL;
        fprintf(stderr, "master DRBG=%p\n", m);
        if (m && inst) fprintf(stderr, "master instantiate=%d\n", inst(m, (const unsigned char *)"x", 1));
        { unsigned long e0; while (geterr && (e0 = geterr()) != 0) { char eb[256]; fprintf(stderr, "ERR %#lx %s\n", e0, errstr ? errstr(e0, eb) : "?"); } }
        fprintf(stderr, "RAND_status=%d\n", status ? status() : -99);
        int r = rb ? rb(buf, 16) : -99;
        fprintf(stderr, "RAND_bytes=%d bytes=", r);
        for (int i = 0; i < 16; i++) fprintf(stderr, "%02x", buf[i]);
        fprintf(stderr, "\n");
        unsigned long e;
        while (geterr && (e = geterr()) != 0) { char eb[256]; fprintf(stderr, "ERR %#lx %s\n", e, errstr ? errstr(e, eb) : "?"); }
    }
    if (getenv("TL_MC_FATAL_PROBE")) {
        tl_lib *L = tl_ld_find_lib("libminecraftpe.so");
        if (L && !tl_ld_probe(L, strtoull(getenv("TL_MC_FATAL_PROBE"), NULL, 16), fatal_probe)) fprintf(stderr, "fatal probe failed\n");
    }
    if (getenv("TL_MC_ASSERT_PROBE")) {
        tl_lib *L = tl_ld_find_lib("libminecraftpe.so");
        if (L && !tl_ld_probe(L, strtoull(getenv("TL_MC_ASSERT_PROBE"), NULL, 16), assert_probe)) fprintf(stderr, "probe failed\n");
    }
    if (getenv("TL_MC_SOFT_ASSERT")) {
        tl_lib *L = tl_ld_find_lib("libminecraftpe.so");
        if (L && !tl_ld_probe(L, 0x150df538, soft_assert_probe)) fprintf(stderr, "soft assert probe failed\n");
    }
    if (getenv("TL_CXA_THROW")) {
        tl_lib *L = tl_ld_find_lib("libc++_shared.so");
        uintptr_t fn = L ? (uintptr_t)tl_ld_sym(L, "__cxa_throw") : 0;
        if (fn && !tl_ld_probe(L, (uint64_t)(fn - lib_bias("libc++_shared.so")), throw_probe)) fprintf(stderr, "throw probe failed\n");
    }
    if (!tl_ga_run()) { fprintf(stderr, "minecraft: run failed\n"); return 1; }
    if (getenv("TL_CTL")) { static pthread_t ct; pthread_create(&ct, NULL, control_thread, getenv("TL_CTL")); }
    if (getenv("TL_PROF")) { pthread_t st; pthread_create(&st, NULL, profiler, getenv("TL_PROF")); }
    if (getenv("TL_SAMPLE")) { pthread_t st; pthread_create(&st, NULL, sampler, (void *)(intptr_t)atoi(getenv("TL_SAMPLE"))); }
    int secs = argc > 2 ? atoi(argv[2]) : 5;
    for (int i = 0; i < secs; i++) sleep(1);
    fprintf(stderr, "minecraft: executable memory used %zu of %zu MiB\n", tl_xmem_used() >> 20, tl_xmem_size() >> 20);
    fprintf(stderr, "minecraft: %lu frames in %d s\n", tl_ga_frames(), secs);
    return 0;
}
