/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for the GameMaker driver: runs an APK's GameMaker game on a Mac, off-screen.
 *
 *   gamemaker-test <apk> [seconds] [width height]
 *
 * Environment: TL_JNI_TRACE=1|2, TL_VERBOSE=0..2, TL_DATA=<dir> (kept between runs), TL_PACKAGE, TL_AUDIO=1,
 * TL_CTL=<file> (lines "tap X Y", "hold X Y MS", "swipe X1 Y1 X2 Y2 MS", "wait MS", "shot PNG", "key CODE", "pause",
 * "resume", "quit"), TL_FRAMES=<n> (save every nth frame; default latest only).
 */
#include <mach/mach.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-audio.h"
#include "husk-tl-gamemaker.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"

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
    for (int i = 0; i < 4096 && shown < 24; i++) {
        uint64_t v;
        if (!safe_read((uintptr_t)(sp + i), &v, 8)) break;
        if (v > 0x7000000000ull && v < 0x7100000000ull && tl_ld_lib_of((void *)v) && (v & 3) == 0) { describe("stk", (void *)v); shown++; }
    }
    for (int i = 0; i < 29; i += 4)
        fprintf(stderr, "  x%d=%#llx x%d=%#llx x%d=%#llx x%d=%#llx\n", i, ss->__x[i], i + 1, ss->__x[i + 1], i + 2, i + 2 < 29 ? ss->__x[i + 2] : 0, i + 3, i + 3 < 29 ? ss->__x[i + 3] : 0);
    fprintf(stderr, "  sp=%#llx fp=%#llx\n", ss->__sp, ss->__fp);
    fflush(stderr);
    _exit(139);
}

static const char *g_frame_dir;
static void sleep_ms(long ms) { usleep((useconds_t)ms * 1000); }
static void do_swipe(float x1, float y1, float x2, float y2, long ms)
{
    int steps = (int)(ms / 16); if (steps < 2) steps = 2;
    tl_gm_touch(0, 0, x1, y1);
    for (int i = 1; i <= steps; i++) { sleep_ms(ms / steps); tl_gm_touch(1, 0, x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps); }
    tl_gm_touch(2, 0, x2, y2);
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
            if (sscanf(line, "tap %f %f", &a, &b) == 2) { tl_gm_touch(0, 0, a, b); sleep_ms(80); tl_gm_touch(2, 0, a, b); }
            else if (sscanf(line, "hold %f %f %ld", &a, &b, &ms) == 3) { tl_gm_touch(0, 0, a, b); sleep_ms(ms); tl_gm_touch(2, 0, a, b); }
            else if (sscanf(line, "swipe %f %f %f %f %ld", &a, &b, &c, &d, &ms) == 5) do_swipe(a, b, c, d, ms);
            else if (sscanf(line, "wait %ld", &ms) == 1) sleep_ms(ms);
            else if (sscanf(line, "shot %399s", p) == 1) {
                char cmd[900]; snprintf(cmd, sizeof(cmd), "sips -s format png '%s/latest.bmp' --out '%s' >/dev/null 2>&1", g_frame_dir, p);
                if (system(cmd)) fprintf(stderr, "ctl: shot failed\n");
                else fprintf(stderr, "ctl: shot %s (frame %lu)\n", p, tl_gm_frames());
            }
            else if (sscanf(line, "key %ld", &ms) == 1) { tl_gm_key((int)ms, true); sleep_ms(60); tl_gm_key((int)ms, false); }
            else if (!strncmp(line, "pause", 5)) tl_gm_set_paused(true);
            else if (!strncmp(line, "resume", 6)) tl_gm_set_paused(false);
            else if (!strncmp(line, "quit", 4)) { fprintf(stderr, "ctl: quit\n"); fflush(stderr); _exit(0); }
        }
        fclose(f);
    }
    return NULL;
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

    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 1);
    tl_jni_set_trace(getenv("TL_JNI_TRACE") ? atoi(getenv("TL_JNI_TRACE")) : 1);
    char tmp[600] = "/Volumes/GTAV/husk2/tmp/husk-gm-XXXXXX";
    if (getenv("TL_DATA")) { snprintf(tmp, sizeof(tmp), "%s", getenv("TL_DATA")); mkdir(tmp, 0755); }
    else mkdtemp(tmp);
    const char *cef = "/Users/davi/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Chromium Embedded Framework.framework/Versions/A/Libraries";
    char egl[600], gles[600]; snprintf(egl, sizeof(egl), "%s/libEGL.dylib", cef); snprintf(gles, sizeof(gles), "%s/libGLESv2.dylib", cef);
    char frames[] = "/Volumes/GTAV/husk2/tmp/husk-gframes-XXXXXX"; mkdtemp(frames);
    fprintf(stderr, "frames: %s\ndata: %s\n", frames, tmp);
    int w = argc > 4 ? atoi(argv[3]) : 1200, h = argc > 4 ? atoi(argv[4]) : 552;
    tl_gm_config cfg = { .apk_path = argv[1], .data_dir = tmp, .package_name = getenv("TL_PACKAGE") ? getenv("TL_PACKAGE") : "com.company.game",
                            .width = w, .height = h,
                            .angle_egl = getenv("TL_ANGLE_EGL") ? getenv("TL_ANGLE_EGL") : egl, .angle_gles = getenv("TL_ANGLE_GLES") ? getenv("TL_ANGLE_GLES") : gles,
                            .frame_dir = frames, .frame_every = getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : -6 };
    g_frame_dir = frames;
    if (getenv("TL_AUDIO")) tl_audio_install();
    if (!tl_gm_start(&cfg)) { fprintf(stderr, "gamemaker: start failed\n"); return 1; }
    if (!tl_gm_run()) { fprintf(stderr, "gamemaker: run failed\n"); return 1; }
    if (getenv("TL_CTL")) { static pthread_t ct; pthread_create(&ct, NULL, control_thread, getenv("TL_CTL")); }
    int secs = argc > 2 ? atoi(argv[2]) : 5;
    for (int i = 0; i < secs && !tl_gm_ended(); i++) sleep(1);
    fprintf(stderr, "gamemaker: %lu frames in %d s\n", tl_gm_frames(), secs);

    return 0;
}
