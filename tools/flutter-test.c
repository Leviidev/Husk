/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for Flutter apps on the translation layer.
 *
 *   flutter-test <apk> [seconds] [width height]
 *
 * Environment: TL_PKG (package name), TL_DATA (keep the data dir), TL_FRAMES (every Nth frame to a BMP, -1 every frame),
 * TL_RATIO (device pixel ratio, default 2), TL_FLUTTER_ARGS (one extra engine switch), TL_FLUTTER_TRACE=1, TL_VERBOSE.
 * TL_TAPS="sec:x:y,..." taps; TL_TYPE="sec:text|sec:text" types (\n = Return, \b = backspace); TL_BACK="sec,..." presses back;
 * TL_RESIZE="sec:w:h" resizes the view, as a window being resized does.
 */
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include "husk-tl-flutter.h"
#include "husk-tl-flutter-text.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"

void tl_log_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap);
}

static void describe(const char *label, const void *addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &lib, &sa);
    if (lib) { fprintf(stderr, "  %-6s %p  %s  %s+%#lx\n", label, addr, lib, sym ? sym : "?", sa ? (unsigned long)((const char *)addr - (const char *)sa) : 0ul); return; }
    Dl_info di;
    if (dladdr(addr, &di) && di.dli_sname) fprintf(stderr, "  %-6s %p  [host] %s+%#lx\n", label, addr, di.dli_sname, (unsigned long)((const char *)addr - (const char *)di.dli_saddr));
    else fprintf(stderr, "  %-6s %p\n", label, addr);
}

static void on_crash(int sig, siginfo_t *info, void *uctx)
{
    ucontext_t *uc = uctx;
    _STRUCT_ARM_THREAD_STATE64 *ss = &uc->uc_mcontext->__ss;
    char nm[32] = ""; pthread_getname_np(pthread_self(), nm, sizeof(nm));
    fprintf(stderr, "\n=== CRASH: signal %d, fault address %p, thread %s ===\n", sig, info->si_addr, nm);
    describe("pc", (void *)ss->__pc);
    describe("lr", (void *)ss->__lr);
    if (info->si_addr) describe("fault", info->si_addr);
    uint64_t *fp = (uint64_t *)ss->__fp;
    for (int i = 0; i < 20 && fp && ((uintptr_t)fp & 7) == 0 && (uintptr_t)fp > 0x100000000ull; i++) { describe("frame", (void *)fp[1]); fp = (uint64_t *)fp[0]; }
    fprintf(stderr, "  x0=%#llx x1=%#llx x2=%#llx x3=%#llx x8=%#llx x19=%#llx x20=%#llx\n", ss->__x[0], ss->__x[1], ss->__x[2], ss->__x[3], ss->__x[8], ss->__x[19], ss->__x[20]);
    fflush(stderr);
    _exit(139);
}

/* TL_PROBE=0xOFF,0xOFF: log x0-x5 (and the first longs at x3, a packet) when libflutter.so reaches those offsets */
static void probe_cb(uint64_t *r)
{
    fprintf(stderr, "probe: x0=%#llx x1=%#llx x2=%#llx x3=%#llx x4=%#llx x5=%#llx x8=%#llx x19=%#llx x20=%#llx x21=%#llx x22=%#llx\n", r[0], r[1], r[2], r[3], r[4], r[5], r[8], r[19], r[20], r[21], r[22]);
}

static void keyboard_cb(int show, int kind) { fprintf(stderr, "harness: keyboard %s (kind %#x)\n", show ? "shown" : "hidden", kind); }

/* TL_TYPE, TL_BACK and TL_RESIZE, run at their seconds. */
static void script_tick(int ms)
{
    static char type[2000], back[200], resize[200];
    static bool loaded;
    if (!loaded) {
        loaded = true;
        if (getenv("TL_TYPE")) snprintf(type, sizeof(type), "%s", getenv("TL_TYPE"));
        if (getenv("TL_BACK")) snprintf(back, sizeof(back), "%s", getenv("TL_BACK"));
        if (getenv("TL_RESIZE")) snprintf(resize, sizeof(resize), "%s", getenv("TL_RESIZE"));
    }
    char copy[2000];
    snprintf(copy, sizeof(copy), "%s", type);
    for (char *save = copy, *tok; (tok = strsep(&save, "|"));) {
        double t; int off = 0;
        if (sscanf(tok, "%lf:%n", &t, &off) != 1 || !off || (int)(t * 1000) != ms) continue;
        fprintf(stderr, "harness: type \"%s\"\n", tok + off);
        for (const char *p = tok + off; *p; p++) {
            if (p[0] == '\\' && p[1] == 'n') { tl_flutter_key_action(); p++; }
            else if (p[0] == '\\' && p[1] == 'b') { tl_flutter_key_delete(); p++; }
            else {
                /* one character: a whole UTF-8 sequence */
                int n = (*p & 0x80) == 0 ? 1 : (*p & 0xE0) == 0xC0 ? 2 : (*p & 0xF0) == 0xE0 ? 3 : 4;
                char one[5] = { 0 };
                memcpy(one, p, (size_t)n);
                tl_flutter_key_insert(one);
                p += n - 1;
            }
        }
    }
    snprintf(copy, sizeof(copy), "%s", back);
    for (char *save = copy, *tok; (tok = strsep(&save, ","));)
        if (*tok && (int)(atof(tok) * 1000) == ms) { fprintf(stderr, "harness: back\n"); tl_flutter_back(); }
    snprintf(copy, sizeof(copy), "%s", resize);
    for (char *save = copy, *tok; (tok = strsep(&save, ","));) {
        double t; int w, h;
        if (sscanf(tok, "%lf:%d:%d", &t, &w, &h) == 3 && (int)(t * 1000) == ms) { fprintf(stderr, "harness: resize %dx%d\n", w, h); tl_flutter_resize(w, h); }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <apk> [seconds] [width height]\n", argv[0]); return 2; }
    static uint8_t altstack[1 << 16];
    stack_t st = { .ss_sp = altstack, .ss_size = sizeof(altstack) };
    sigaltstack(&st, NULL);
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash; sa.sa_flags = SA_SIGINFO | SA_ONSTACK; sigemptyset(&sa.sa_mask);
    int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGTRAP, SIGABRT };
    for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) sigaction(sigs[i], &sa, NULL);

    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 1);
    tl_jni_set_trace(getenv("TL_JNI_TRACE") ? atoi(getenv("TL_JNI_TRACE")) : 1);
    char tmp[600] = "/tmp/husk-flutter-XXXXXX";
    if (getenv("TL_DATA")) snprintf(tmp, sizeof(tmp), "%s", getenv("TL_DATA")); else mkdtemp(tmp);
    const char *cef = "/Users/davi/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Chromium Embedded Framework.framework/Versions/A/Libraries";
    char egl[600], gles[600]; snprintf(egl, sizeof(egl), "%s/libEGL.dylib", cef); snprintf(gles, sizeof(gles), "%s/libGLESv2.dylib", cef);
    char frames[] = "/tmp/husk-flutterframes-XXXXXX"; mkdtemp(frames);
    fprintf(stderr, "frames: %s\ndata: %s\n", frames, tmp);
    int w = argc > 4 ? atoi(argv[3]) : 720, h = argc > 4 ? atoi(argv[4]) : 1560;
    tl_ga_config cfg = { .apk_path = argv[1], .data_dir = tmp, .package_name = getenv("TL_PKG") ? getenv("TL_PKG") : "com.example.app", .width = w, .height = h,
                         .angle_egl = getenv("TL_ANGLE_EGL") ? getenv("TL_ANGLE_EGL") : egl, .angle_gles = getenv("TL_ANGLE_GLES") ? getenv("TL_ANGLE_GLES") : gles,
                         .frame_dir = frames, .frame_every = getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : 10 };
    void tl_set_cacerts_dir(const char *dir);
    tl_set_cacerts_dir(getenv("TL_CACERTS") ? getenv("TL_CACERTS") : "/Volumes/GTAV/husk2/root/apex/com.android.conscrypt/cacerts");
    void tl_set_system_fonts_dir(const char *dir);
    tl_set_system_fonts_dir(getenv("TL_SYSFONTS") ? getenv("TL_SYSFONTS") : "/Volumes/GTAV/husk2/root/system");
    tl_flutter_set_pixel_ratio(getenv("TL_RATIO") ? (float)atof(getenv("TL_RATIO")) : 2.0f);
    if (!tl_flutter_start(&cfg)) { fprintf(stderr, "flutter: start failed\n"); return 1; }
    if (getenv("TL_PROBE")) {
        tl_lib *fl = tl_ld_find_lib("libflutter.so");
        char pr[400]; snprintf(pr, sizeof(pr), "%s", getenv("TL_PROBE"));
        for (char *save = pr, *tok; (tok = strsep(&save, ","));) if (fl && !tl_ld_probe(fl, strtoull(tok, NULL, 16), probe_cb)) fprintf(stderr, "probe at %s failed\n", tok);
    }
    tl_flutter_text_set_keyboard_handler(keyboard_cb);
    if (!tl_flutter_run()) { fprintf(stderr, "flutter: run failed\n"); return 1; }
    int secs = argc > 2 ? atoi(argv[2]) : 10;
    /* TL_TAPS="sec:x:y,sec:x:y": a tap (down, then up 80 ms later) at those seconds, in surface pixels */
    char taps[1000] = ""; if (getenv("TL_TAPS")) snprintf(taps, sizeof(taps), "%s", getenv("TL_TAPS"));
    double tsec[64]; float tx[64], ty[64]; int nt = 0;
    for (char *save = taps, *tok; (tok = strsep(&save, ",")) && nt < 64;) if (sscanf(tok, "%lf:%f:%f", &tsec[nt], &tx[nt], &ty[nt]) == 3) nt++;
    for (int ms = 0, next = 0; ms < secs * 1000; ms += 10) {
        usleep(10000);
        while (next < nt && ms >= (int)(tsec[next] * 1000)) {
            fprintf(stderr, "harness: tap %.0f,%.0f\n", tx[next], ty[next]);
            tl_flutter_touch(0, 1, tx[next], ty[next]); usleep(80000); tl_flutter_touch(2, 1, tx[next], ty[next]);
            next++;
        }
        script_tick(ms);
    }
    fprintf(stderr, "flutter: %lu frames in %d s, first frame %s\n", tl_flutter_frames(), secs, tl_flutter_first_frame() ? "yes" : "no");
    return 0;
}
