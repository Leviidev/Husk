/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for Java apps on Husk's Dalvik runtime: runs an APK's launcher activity on a Mac, off-screen.
 *
 *   javaapp-test <apk> [seconds] [width height]
 *
 * Environment: TL_JNI_TRACE=1|2, TL_VERBOSE=0..2, TL_DATA=<dir> (kept between runs), TL_PACKAGE, TL_AUDIO=1,
 * TL_CTL=<file> (lines "tap X Y", "hold X Y MS", "swipe X1 Y1 X2 Y2 MS", "wait MS", "shot PNG" (+ PNG.web.png of a web view), "js CODE", "key CODE", "pause",
 * "resume", "quit"), TL_FRAMES=<n> (save every nth frame; default every frame, latest only).
 */
#include <mach/mach.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-audio.h"
#include "husk-tl-dvm.h"
#include "husk-tl-dvm-javaapp.h"
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
bool tl_web_tap(float px, float py);
void tl_web_snapshot(const char *png);
void tl_web_eval_log(const char *js);
void tl_web_harness_size(int w, int h, float scale);
void tl_web_main_loop(void);
static void sleep_ms(long ms) { usleep((useconds_t)ms * 1000); }
static void do_swipe(float x1, float y1, float x2, float y2, long ms)
{
    int steps = (int)(ms / 16); if (steps < 2) steps = 2;
    tl_javaapp_touch(0, 0, x1, y1);
    for (int i = 1; i <= steps; i++) { sleep_ms(ms / steps); tl_javaapp_touch(1, 0, x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps); }
    tl_javaapp_touch(2, 0, x2, y2);
}
static char *g_clip;
static void h_keyboard(int show, int type, int ime) { fprintf(stderr, "host: keyboard %s (input type %#x, ime %#x)\n", show ? "shown" : "hidden", type, ime); }
static void h_set_clip(const char *t) { free(g_clip); g_clip = strdup(t); fprintf(stderr, "host: clipboard = %.60s\n", t); }
static char *h_get_clip(void) { return g_clip ? strdup(g_clip) : NULL; }
static void h_share(const char *t) { fprintf(stderr, "host: share %.80s\n", t); }
static void h_orientation(int o) { fprintf(stderr, "host: orientation %d\n", o); }
static void *control_thread(void *arg)
{
    const char *path = arg;
    for (;;) {
        FILE *f = fopen(path, "r");
        if (!f) { sleep_ms(200); continue; }
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            float a, b, c, d; long ms; char p[400];
            if (sscanf(line, "tap %f %f", &a, &b) == 2) { if (tl_web_tap(a, b)) continue; tl_javaapp_touch(0, 0, a, b); sleep_ms(80); tl_javaapp_touch(2, 0, a, b); }
            else if (sscanf(line, "hold %f %f %ld", &a, &b, &ms) == 3) { tl_javaapp_touch(0, 0, a, b); sleep_ms(ms); tl_javaapp_touch(2, 0, a, b); }
            else if (sscanf(line, "swipe %f %f %f %f %ld", &a, &b, &c, &d, &ms) == 5) do_swipe(a, b, c, d, ms);
            else if (sscanf(line, "wait %ld", &ms) == 1) sleep_ms(ms);
            else if (sscanf(line, "shot %399s", p) == 1) {
                char cmd[900]; snprintf(cmd, sizeof(cmd), "sips -s format png '%s/latest.bmp' --out '%s' >/dev/null 2>&1", g_frame_dir, p);
                if (system(cmd)) fprintf(stderr, "ctl: shot failed\n");
                else fprintf(stderr, "ctl: shot %s (frame %lu)\n", p, tl_javaapp_frames());
                char wp[420]; snprintf(wp, sizeof(wp), "%.*s.web.png", (int)(strlen(p) > 4 ? strlen(p) - 4 : strlen(p)), p);
                tl_web_snapshot(wp);
            }
            else if (!strncmp(line, "js ", 3)) { line[strcspn(line, "\n")] = 0; tl_web_eval_log(line + 3); }
            else if (sscanf(line, "key %ld", &ms) == 1) { tl_javaapp_key((int)ms, true); sleep_ms(60); tl_javaapp_key((int)ms, false); }
            else if (!strncmp(line, "text ", 5)) { line[strcspn(line, "\n")] = 0; tl_javaapp_text(line + 5); }
            else if (!strncmp(line, "del", 3)) tl_javaapp_text_delete();
            else if (!strncmp(line, "enter", 5)) tl_javaapp_text_action();
            else if (!strncmp(line, "back", 4)) tl_javaapp_back();
            else if (!strncmp(line, "pause", 5)) tl_javaapp_set_paused(true);
            else if (!strncmp(line, "resume", 6)) tl_javaapp_set_paused(false);
            else if (!strncmp(line, "quit", 4)) { fprintf(stderr, "ctl: quit\n"); fflush(stderr); _exit(0); }
        }
        fclose(f);
    }
    return NULL;
}


typedef struct { int argc; char **argv; } job;
static void *run(void *p)
{
    job *j = p; char **argv = j->argv; int argc = j->argc;
    const char *art = getenv("TL_ART") ? getenv("TL_ART") : "/Volumes/GTAV/husk2/root/apex/com.android.art";
    static const char *const jars[] = { "core-oj", "core-libart", "okhttp", "bouncycastle", "apache-xml" };
    static char paths[10][700]; const char *boot[10]; int nb = 0;
    for (int i = 0; i < 5; i++) { snprintf(paths[nb], sizeof(paths[nb]), "%s/javalib/%s.jar", art, jars[i]); boot[nb] = paths[nb]; nb++; }
    snprintf(paths[nb], sizeof(paths[nb]), "%s/../com.android.i18n/javalib/core-icu4j.jar", art); boot[nb] = paths[nb]; nb++;
    snprintf(paths[nb], sizeof(paths[nb]), "%s/../com.android.conscrypt/javalib/conscrypt.jar", art); boot[nb] = paths[nb]; nb++;
    snprintf(paths[nb], sizeof(paths[nb]), "%s", getenv("TL_FRAMEWORK") ? getenv("TL_FRAMEWORK") : "/Volumes/GTAV/husk2/java/husk-framework.dex"); boot[nb] = paths[nb]; nb++;
    snprintf(paths[nb], sizeof(paths[nb]), "/Volumes/GTAV/husk2/modules/jars/org.apache.http.legacy.jar"); boot[nb] = paths[nb]; nb++;
    { char v[800];
      snprintf(v, sizeof(v), "%s", art); setenv("ANDROID_ART_ROOT", v, 0);
      snprintf(v, sizeof(v), "%s/../com.android.i18n", art); setenv("ANDROID_I18N_ROOT", v, 0);
      snprintf(v, sizeof(v), "%s/../com.android.tzdata", art); setenv("ANDROID_TZDATA_ROOT", v, 0);
      snprintf(v, sizeof(v), "%s/../../system", art); setenv("ANDROID_ROOT", v, 0); }
    tl_jni_init();
    if (!tl_dvm_start(boot, nb)) { fprintf(stderr, "dvm start failed\n"); _exit(1); }
    { char d[800]; snprintf(d, sizeof(d), "%s/../com.android.i18n/lib64", art); tl_ld_add_search_dir(d); }
    { char d[800]; snprintf(d, sizeof(d), "%s/../com.android.conscrypt/lib64", art); tl_ld_add_search_dir(d); }
    { void tl_set_cacerts_dir(const char *); static char d[800]; snprintf(d, sizeof(d), "%s/../com.android.conscrypt/cacerts", art); tl_set_cacerts_dir(d); }   /* the trusted roots, as the app ships them */
    { char d[800]; snprintf(d, sizeof(d), "%s/lib64", art); tl_dvm_load_natives(d); }
    char tmp[600] = "/Volumes/GTAV/husk2/tmp/husk-javaapp-XXXXXX";
    if (getenv("TL_DATA")) { snprintf(tmp, sizeof(tmp), "%s", getenv("TL_DATA")); mkdir(tmp, 0755); } else mkdtemp(tmp);
    const char *cef = "/Users/davi/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Chromium Embedded Framework.framework/Versions/A/Libraries";
    static char egl[700], gles[700]; snprintf(egl, sizeof(egl), "%s/libEGL.dylib", cef); snprintf(gles, sizeof(gles), "%s/libGLESv2.dylib", cef);
    static char frames[] = "/Volumes/GTAV/husk2/tmp/husk-jframes-XXXXXX"; mkdtemp(frames);
    g_frame_dir = frames;
    fprintf(stderr, "frames: %s\ndata: %s\n", frames, tmp);
    int w = argc > 4 ? atoi(argv[3]) : 804, h = argc > 4 ? atoi(argv[4]) : 1748;
    /* A folder is an app as Google Play delivers it: <package>.apk and its splits and asset packs (<package>.<split>.apk), as the app
       is given them; TL_SPLITS (colon-separated) adds split APKs to a single APK */
    static char base[1024];
    const char *apk = argv[1];
    struct stat st;
    if (stat(apk, &st) == 0 && S_ISDIR(st.st_mode)) {
        DIR *d = opendir(apk); struct dirent *de; char names[32][256]; int n = 0;
        while (d && (de = readdir(d)) && n < 32) { size_t l = strlen(de->d_name); if (l > 4 && !strcmp(de->d_name + l - 4, ".apk")) snprintf(names[n++], 256, "%s", de->d_name); }
        if (d) closedir(d);
        int bi = -1;
        for (int i = 0; i < n; i++) if (bi < 0 || strlen(names[i]) < strlen(names[bi])) bi = i;
        if (bi < 0) { fprintf(stderr, "javaapp: no APK in %s\n", apk); _exit(1); }
        snprintf(base, sizeof(base), "%s/%s", apk, names[bi]);
        for (int i = 0; i < n; i++) if (i != bi) { char sp[1400]; snprintf(sp, sizeof(sp), "%s/%s", apk, names[i]); tl_ld_queue_split(sp); fprintf(stderr, "javaapp: split %s\n", names[i]); }
        apk = base;
    }
    if (getenv("TL_SPLITS")) { char *l = strdup(getenv("TL_SPLITS")); for (char *t = strtok(l, ":"); t; t = strtok(NULL, ":")) tl_ld_queue_split(t); }
    tl_javaapp_config cfg = { .apk_path = apk, .data_dir = tmp, .package_name = getenv("TL_PACKAGE"), .width = w, .height = h, .density = 2.625f,
                              .angle_egl = getenv("TL_ANGLE_EGL") ? getenv("TL_ANGLE_EGL") : egl, .angle_gles = getenv("TL_ANGLE_GLES") ? getenv("TL_ANGLE_GLES") : gles,
                              .frame_dir = frames, .frame_every = getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : -1,
                              .framework_res = getenv("TL_FRAMEWORK_RES") ? getenv("TL_FRAMEWORK_RES") : "/Volumes/GTAV/husk2/java/framework-res.apk",
                              .show_keyboard = h_keyboard, .set_clipboard = h_set_clip, .get_clipboard = h_get_clip, .share = h_share, .set_orientation = h_orientation };
    if (getenv("TL_INSETS")) sscanf(getenv("TL_INSETS"), "%d,%d,%d,%d", &cfg.insets[0], &cfg.insets[1], &cfg.insets[2], &cfg.insets[3]);
    tl_web_harness_size(w, h, cfg.density);
    if (!tl_javaapp_start(&cfg)) { fprintf(stderr, "javaapp: start failed\n"); _exit(1); }
    if (getenv("TL_CTL")) { static pthread_t ct; pthread_create(&ct, NULL, control_thread, getenv("TL_CTL")); }
    int secs = argc > 2 ? atoi(argv[2]) : 10;
    for (int i = 0; i < secs && !tl_javaapp_ended(); i++) sleep(1);
    fprintf(stderr, "javaapp: %lu frames in %d s\n", tl_javaapp_frames(), secs);
    fflush(stderr);
    _exit(0);
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
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL); sigaction(SIGILL, &sa, NULL); sigaction(SIGTRAP, &sa, NULL); sigaction(SIGABRT, &sa, NULL);
    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 0);
    tl_jni_set_trace(getenv("TL_JNI_TRACE") ? atoi(getenv("TL_JNI_TRACE")) : 0);
    tl_dvm_set_trace(getenv("TL_DVM_TRACE") ? atoi(getenv("TL_DVM_TRACE")) : 0);
    job j = { argc, argv };
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 64u << 20);
    pthread_t t; pthread_create(&t, &a, run, &j);
    tl_web_main_loop();                     /* WebKit runs on the main thread; run() ends the process */
    return 0;
}
