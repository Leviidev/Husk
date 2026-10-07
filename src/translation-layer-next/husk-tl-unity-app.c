/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-unity-app.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <os/proc.h>
#endif

#include "husk-tl-bionic.h"
#include "husk-tl-internal.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"
#include "husk-tl-unity.h"
#include "husk-tl-audio.h"
#include "husk-tl-cocos.h"
#include "husk-tl-gameactivity.h"
#include "husk-tl-gamepad.h"
#include "husk-tl-sdl.h"
#include "husk-tl-nativeactivity.h"
#include "husk-tl-gtasa.h"
#include "husk-tl-vulkan.h"

void tl_hle_set_ca_bundle(const char *path);
extern int tl_log_sink_fd;

enum { ENGINE_UNITY = 0, ENGINE_COCOS = 1, ENGINE_GAMEACTIVITY = 2, ENGINE_SDL = 3, ENGINE_UE4 = 4, ENGINE_GTA = 5 };

static unsigned long engine_frames(void);

static struct {
    atomic_int state;
    int engine;
    char apk[1024], data[1024], package[160], angle[1024], ca[1024], vulkan[1024];
    char extra[3][1024];
    int nextra;
    void *layer;
    int width, height;
} A;

/* -------------------------------------------------------------- crash report */

static int find_lib(uintptr_t bias, const char *name, const void *phdr, unsigned phnum, void *user)
{
    struct { uintptr_t addr; const char *name; uintptr_t off; } *r = user;
    const struct { uint32_t type, flags; uint64_t off, vaddr, paddr, filesz, memsz, align; } *p = phdr;
    for (unsigned i = 0; i < phnum; i++)
        if (p[i].type == 1 && r->addr >= bias + p[i].vaddr && r->addr < bias + p[i].vaddr + p[i].memsz) {
            r->name = name; r->off = r->addr - bias;
            return 1;
        }
    return 0;
}

static void where(char *out, size_t n, uintptr_t addr)
{
    struct { uintptr_t addr; const char *name; uintptr_t off; } r = { addr, NULL, 0 };
    tl_ld_iterate(find_lib, &r);
    if (r.name) snprintf(out, n, "%s+%#lx", r.name, (unsigned long)r.off);
    else snprintf(out, n, "%#lx", (unsigned long)addr);
}

static struct sigaction g_prev[32];

/*
 * A fault in guest code ends the process, as it would on Android, but not before the log says where:
 * the guest library and offset of the faulting instruction and of the code that called it. The app's
 * own crash handling (which keeps the log file) runs after.
 */
static void on_fatal(int sig, siginfo_t *info, void *uctx)
{
    ucontext_t *uc = uctx;
    char tn[40] = "", a[200], b[200], c[200];
    pthread_getname_np(pthread_self(), tn, sizeof(tn));
    where(a, sizeof(a), (uintptr_t)uc->uc_mcontext->__ss.__pc);
    where(b, sizeof(b), (uintptr_t)uc->uc_mcontext->__ss.__lr);
    where(c, sizeof(c), (uintptr_t)info->si_addr);
    tl_log_line("=== FATAL signal %d on thread '%s': fault address %p (%s)", sig, tn, info->si_addr, c);
    tl_log_line("    pc %s", a);
    tl_log_line("    lr %s", b);
    uintptr_t fp = uc->uc_mcontext->__ss.__fp;
    for (int i = 0; i < 12 && fp && (fp & 7) == 0 && fp > 0x100000000ull; i++) {
        uintptr_t *f = (uintptr_t *)fp;
        char w[200];
        where(w, sizeof(w), f[1]);
        tl_log_line("    frame %s", w);
        if (f[0] <= fp) break;
        fp = f[0];
    }
    for (int i = 0; i < 29; i += 4)
        tl_log_line("    x%d=%#llx x%d=%#llx x%d=%#llx x%d=%#llx", i, uc->uc_mcontext->__ss.__x[i], i + 1, uc->uc_mcontext->__ss.__x[i + 1],
                    i + 2, i + 2 < 29 ? uc->uc_mcontext->__ss.__x[i + 2] : 0ull, i + 3, i + 3 < 29 ? uc->uc_mcontext->__ss.__x[i + 3] : 0ull);
    if (g_prev[sig].sa_flags & SA_SIGINFO) {
        if (g_prev[sig].sa_sigaction) { g_prev[sig].sa_sigaction(sig, info, uctx); return; }
    } else if (g_prev[sig].sa_handler != SIG_DFL && g_prev[sig].sa_handler != SIG_IGN && g_prev[sig].sa_handler) {
        g_prev[sig].sa_handler(sig);
        return;
    }
    signal(sig, SIG_DFL);                       /* nobody else handles it: re-fault with the default action */
}

static void install_crash_reporter(void)
{
    static bool done;
    if (done) return;
    done = true;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_fatal;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    static const int sigs[] = { SIGSEGV, SIGBUS, SIGILL };      /* not SIGTRAP: the app's brk guard owns it */
    for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) sigaction(sigs[i], &sa, &g_prev[sigs[i]]);
}

/* ------------------------------------------------------------------- launch */

/* exit() from the game ends the game, and the thread that asked. */
static void guest_exit(int status)
{
    tl_log_line("native: the game exited (%d)", status);
    atomic_store(&A.state, HUSK_UNITY_ENDED);
    pthread_exit(NULL);
}

/* Says every few seconds that the engine is alive, and how much memory the phone says is left: a silent death
 * leaves nothing else to tell a frozen game from a killed one, or a jetsam kill from a crash. */
static void *heartbeat_thread(void *arg)
{
    (void)arg;
    pthread_setname_np("husk-unity-hb");
    unsigned long last = 0;
    for (int tick = 0;; tick++) {
        if (tick < 40) usleep(500000); else sleep(3);          /* twice a second for the first twenty seconds */
        unsigned long f = engine_frames();
#if TARGET_OS_IPHONE
        tl_log_line("unity: alive: %lu frames (+%lu), %zu MiB left before jetsam", f, f - last, os_proc_available_memory() >> 20);
#else
        tl_log_line("unity: alive: %lu frames (+%lu)", f, f - last);
#endif
        last = f;
        { extern volatile struct { uint64_t count, x21, impl, mask; } tl_va_clobber;
          static uint64_t seen;
          if (tl_va_clobber.count != seen) {
              seen = tl_va_clobber.count;
              Dl_info di;
              const char *nm = dladdr((void *)tl_va_clobber.impl, &di) && di.dli_sname ? di.dli_sname : "?";
              tl_log_line("unity: a variadic shim's implementation (%s) changed callee-saved registers (%llu times; x21 then %#llx, diff mask %#llx)",
                          nm, (unsigned long long)seen, (unsigned long long)tl_va_clobber.x21, (unsigned long long)tl_va_clobber.mask);
          } }
        if (A.engine == ENGINE_COCOS && tl_cocos_ended()) atomic_store(&A.state, HUSK_UNITY_ENDED);
        if (atomic_load(&A.state) == HUSK_UNITY_ENDED || atomic_load(&A.state) == HUSK_UNITY_FAILED) return NULL;
    }
}

/*
 * The activity a launcher starts: the <activity> that holds <category android:name="android.intent.category.LAUNCHER">, as a JNI class name. A class named in the
 * manifest with a leading dot (or with no dot) belongs to the app's package.
 */
static uint32_t rd32(const uint8_t *p);
static uint16_t rd16(const uint8_t *p);
static bool pool_string(const uint8_t *pool, size_t pool_size, uint32_t index, char *out, size_t n);
static bool manifest_launcher_activity(const char *apk, const char *package, char *out, size_t cap)
{
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return false;
    bool ok = false;
    const tl_zip_entry *e = tl_zip_find(&z, "AndroidManifest.xml");
    const uint8_t *data; size_t len; bool owned = false;
    if (e && tl_zip_data(&z, e, 8u << 20, &data, &len, &owned, err, sizeof(err)) && len > 8 && rd16(data) == 0x0003) {
        const uint8_t *pool = NULL; size_t pool_size = 0;
        char activity[200] = "";
        for (size_t off = rd16(data + 2); off + 8 <= len && !ok; ) {
            uint16_t type = rd16(data + off); uint32_t size = rd32(data + off + 4);
            if (size < 8 || off + size > len) break;
            if (type == 0x0001) { pool = data + off; pool_size = size; }
            else if ((type == 0x0102 || type == 0x0103) && pool && size >= 24) {
                const uint8_t *el = data + off;
                char name[64] = "";
                pool_string(pool, pool_size, rd32(el + 20), name, sizeof(name));
                if (type == 0x0103) { if (!strcmp(name, "activity")) activity[0] = 0; }
                else {
                    uint16_t astart = rd16(el + 24), asize = rd16(el + 26), acount = rd16(el + 28);
                    for (unsigned i = 0; i < acount; i++) {
                        const uint8_t *at = el + 16 + astart + (size_t)i * asize;
                        char an[64] = "", av[200] = "";
                        if (!pool_string(pool, pool_size, rd32(at + 4), an, sizeof(an)) || strcmp(an, "name")) continue;
                        if (rd32(at + 8) != 0xFFFFFFFFu) pool_string(pool, pool_size, rd32(at + 8), av, sizeof(av));
                        if (!strcmp(name, "activity")) snprintf(activity, sizeof(activity), "%s", av);
                        else if (!strcmp(name, "category") && !strcmp(av, "android.intent.category.LAUNCHER") && activity[0]) ok = true;
                    }
                }
            }
            off += size;
        }
        if (ok) {
            char full[260];
            if (activity[0] == '.') snprintf(full, sizeof(full), "%s%s", package, activity);
            else if (!strchr(activity, '.')) snprintf(full, sizeof(full), "%s.%s", package, activity);
            else snprintf(full, sizeof(full), "%s", activity);
            for (char *c = full; *c; c++) if (*c == '.') *c = '/';
            snprintf(out, cap, "%s", full);
        }
    }
    if (owned) free((void *)data);
    tl_zip_close(&z);
    return ok;
}

static void *launch_thread(void *arg)
{
    (void)arg;
    pthread_setname_np("husk-native-start");
    tl_guest_exit_hook = guest_exit;
    {
        char path[1100];
        snprintf(path, sizeof(path), "%s/%s", A.data, A.engine == ENGINE_UNITY ? "unity-run.log" : "native-run.log");
        tl_log_sink_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
    }
    install_crash_reporter();
    tl_hle_set_ca_bundle(A.ca);

    /* Controllers go to the engine that is running. (GameActivity registers its own as it starts.) */
    if (A.engine == ENGINE_UNITY) tl_unity_register_pad_sink();
    else if (A.engine == ENGINE_COCOS) tl_cocos_register_pad_sink();

    bool ok;
    if (A.engine == ENGINE_GAMEACTIVITY) {
        tl_ga_config cfg = {
            .apk_path = A.apk, .data_dir = A.data, .package_name = A.package, .width = A.width, .height = A.height,
            .metal_layer = A.layer, .angle_egl = A.angle, .angle_gles = NULL, .frame_dir = NULL, .frame_every = 0,
        };
        tl_log_line("gameactivity: starting %s as %s, %dx%d", A.apk, A.package, A.width, A.height);
        tl_audio_install();
        ok = tl_ga_start(&cfg) && tl_ga_run();
    } else if (A.engine == ENGINE_SDL) {
        tl_ga_config cfg = {
            .apk_path = A.apk, .data_dir = A.data, .package_name = A.package, .width = A.width, .height = A.height,
            .metal_layer = A.layer, .angle_egl = A.angle, .angle_gles = NULL, .frame_dir = NULL, .frame_every = 0,
        };
        char activity[260];
        if (!manifest_launcher_activity(A.apk, A.package, activity, sizeof(activity))) {
            tl_log_line("sdl: the manifest names no launcher activity");
            atomic_store(&A.state, HUSK_UNITY_FAILED);
            return NULL;
        }
        for (int i = 0; i < A.nextra; i++) if (!tl_sdl_add_package(A.extra[i])) tl_log_line("sdl: cannot add %s", A.extra[i]);
        tl_log_line("sdl: starting %s (%s) as %s, %dx%d", A.apk, activity, A.package, A.width, A.height);
        tl_audio_install();
        ok = tl_sdl_start(&cfg, activity) && tl_sdl_run();
    } else if (A.engine == ENGINE_GTA) {
        tl_ga_config cfg = {
            .apk_path = A.apk, .data_dir = A.data, .package_name = A.package, .width = A.width, .height = A.height,
            .metal_layer = A.layer, .angle_egl = A.angle, .angle_gles = NULL, .frame_dir = NULL, .frame_every = 0,
        };
        tl_log_line("gta: starting %s as %s, %dx%d", A.apk, A.package, A.width, A.height);
        tl_audio_install();
        ok = tl_gta_start(&cfg) && tl_gta_run();
    } else if (A.engine == ENGINE_UE4) {
        tl_ga_config cfg = {
            .apk_path = A.apk, .data_dir = A.data, .package_name = A.package, .width = A.width, .height = A.height,
            .metal_layer = A.layer, .angle_egl = A.angle, .angle_gles = NULL, .frame_dir = NULL, .frame_every = 0,
        };
        if (A.vulkan[0]) tl_vk_configure(A.vulkan, NULL, 0);
        tl_log_line("ue4: starting %s as %s, %dx%d (Vulkan: %s)", A.apk, A.package, A.width, A.height, A.vulkan[0] ? A.vulkan : "none");
        tl_audio_install();
        ok = tl_na_start(&cfg) && tl_na_run();
    } else if (A.engine == ENGINE_COCOS) {
        tl_cocos_config cfg = {
            .apk_path = A.apk, .data_dir = A.data, .package_name = A.package, .width = A.width, .height = A.height,
            .metal_layer = A.layer, .angle_egl = A.angle, .angle_gles = NULL, .frame_dir = NULL, .frame_every = 0,
        };
        tl_log_line("cocos: starting %s as %s, %dx%d", A.apk, A.package, A.width, A.height);
        tl_audio_install();
        tl_cocos_text_install();
        ok = tl_cocos_start(&cfg) && tl_cocos_run();
    } else {
        tl_unity_config cfg = {
            .apk_path = A.apk, .data_dir = A.data, .package_name = A.package, .width = A.width, .height = A.height,
            .metal_layer = A.layer, .angle_egl = A.angle, .angle_gles = NULL, .frame_dir = NULL, .frame_every = 0,
        };
        tl_log_line("unity: starting %s as %s, %dx%d", A.apk, A.package, A.width, A.height);
        ok = tl_unity_start(&cfg) && tl_unity_run();
    }
    if (!ok) {
        tl_log_line("native: the game could not be started");
        atomic_store(&A.state, HUSK_UNITY_FAILED);
        return NULL;
    }
    atomic_store(&A.state, HUSK_UNITY_RUNNING);
    pthread_t hb;
    if (pthread_create(&hb, NULL, heartbeat_thread, NULL) == 0) pthread_detach(hb);
    return NULL;
}

static bool launch(int engine, const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                   const char *angle_dylib, const char *ca_bundle)
{
    int expected = HUSK_UNITY_IDLE;
    if (!apk || !data_dir || !metal_layer || width <= 0 || height <= 0 || !angle_dylib) return false;
    if (!atomic_compare_exchange_strong(&A.state, &expected, HUSK_UNITY_STARTING)) return false;
    A.engine = engine;
    snprintf(A.apk, sizeof(A.apk), "%s", apk);
    snprintf(A.data, sizeof(A.data), "%s", data_dir);
    snprintf(A.angle, sizeof(A.angle), "%s", angle_dylib);
    snprintf(A.ca, sizeof(A.ca), "%s", ca_bundle ? ca_bundle : "");
    A.layer = metal_layer; A.width = width; A.height = height;
    if (!husk_unity_package_name(apk, A.package, sizeof(A.package))) snprintf(A.package, sizeof(A.package), "%s", engine == ENGINE_GAMEACTIVITY ? "com.mojang.minecraftpe" : engine == ENGINE_GTA ? "com.rockstargames.gtasa" : engine == ENGINE_UE4 ? "com.epicgames.ue4" : engine == ENGINE_SDL ? "com.sdl.game" : engine == ENGINE_COCOS ? "com.cocos.game" : "com.unity.game");
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 4u << 20);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    int rc = pthread_create(&t, &at, launch_thread, NULL);
    pthread_attr_destroy(&at);
    if (rc) { atomic_store(&A.state, HUSK_UNITY_FAILED); return false; }
    return true;
}

bool husk_unity_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                       const char *angle_dylib, const char *ca_bundle)
{
    return launch(ENGINE_UNITY, apk, data_dir, metal_layer, width, height, angle_dylib, ca_bundle);
}

bool husk_cocos_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                       const char *angle_dylib, const char *ca_bundle)
{
    return launch(ENGINE_COCOS, apk, data_dir, metal_layer, width, height, angle_dylib, ca_bundle);
}

bool husk_gameactivity_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                              const char *angle_dylib, const char *ca_bundle)
{
    return launch(ENGINE_GAMEACTIVITY, apk, data_dir, metal_layer, width, height, angle_dylib, ca_bundle);
}

bool husk_sdl_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                     const char *angle_dylib, const char *ca_bundle)
{
    return launch(ENGINE_SDL, apk, data_dir, metal_layer, width, height, angle_dylib, ca_bundle);
}
bool husk_gta_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                     const char *angle_dylib, const char *ca_bundle)
{
    return launch(ENGINE_GTA, apk, data_dir, metal_layer, width, height, angle_dylib, ca_bundle);
}
bool husk_ue4_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                     const char *angle_dylib, const char *ca_bundle)
{
    return launch(ENGINE_UE4, apk, data_dir, metal_layer, width, height, angle_dylib, ca_bundle);
}
/* Where MoltenVK is: Unreal's Vulkan renderer runs on it. Before the launch call. */
void husk_ue4_set_vulkan(const char *dylib) { snprintf(A.vulkan, sizeof(A.vulkan), "%s", dylib ? dylib : ""); }
void husk_native_add_package(const char *apk)
{
    if (apk && A.nextra < 3 && atomic_load(&A.state) == HUSK_UNITY_IDLE) snprintf(A.extra[A.nextra++], sizeof(A.extra[0]), "%s", apk);
}
void husk_sdl_set_safe_insets(int left, int top, int right, int bottom) { tl_sdl_set_safe_insets(left, top, right, bottom); }
int husk_sdl_apk_is_portrait(const char *apk) { return tl_sdl_manifest_portrait(apk) ? 1 : 0; }
void husk_sdl_set_keyboard_handler(void (*handler)(int action)) { tl_sdl_set_keyboard_handler(handler); }
void husk_sdl_commit_text(const char *utf8) { if (atomic_load(&A.state) == HUSK_UNITY_RUNNING && A.engine == ENGINE_SDL) tl_sdl_commit_text(utf8); }
void husk_sdl_key(int keycode, int down) { if (atomic_load(&A.state) == HUSK_UNITY_RUNNING && A.engine == ENGINE_SDL) tl_sdl_key(keycode, down != 0); }

/* ---------------------------------------------------------------- controllers */

void husk_gamepad_connect(int slot, const char *name) { tl_pad_connect(slot, name); }
void husk_gamepad_disconnect(int slot) { tl_pad_disconnect(slot); }
void husk_gamepad_update(int slot, unsigned buttons, float lx, float ly, float rx, float ry, float lt, float rt)
{
    tl_pad_state s = { .buttons = buttons, .lx = lx, .ly = -ly, .rx = rx, .ry = -ry, .lt = lt, .rt = rt };   /* iOS has y up; Android has it down */
    tl_pad_update(slot, &s);
}

void husk_cocos_set_keyboard_handler(void (*handler)(int action)) { tl_cocos_keyboard_hook = handler; }
void husk_cocos_set_open_url_handler(void (*handler)(const char *url)) { tl_cocos_open_url_hook = handler; }
void husk_cocos_insert_text(const char *utf8) { if (atomic_load(&A.state) == HUSK_UNITY_RUNNING && A.engine == ENGINE_COCOS) tl_cocos_insert_text(utf8); }
void husk_cocos_delete_backward(void) { if (atomic_load(&A.state) == HUSK_UNITY_RUNNING && A.engine == ENGINE_COCOS) tl_cocos_delete_backward(); }
void husk_cocos_key_down(int keycode) { if (atomic_load(&A.state) == HUSK_UNITY_RUNNING && A.engine == ENGINE_COCOS) tl_cocos_key_down(keycode); }
void husk_cocos_request_text(void (*cb)(const char *utf8)) { if (atomic_load(&A.state) == HUSK_UNITY_RUNNING && A.engine == ENGINE_COCOS) tl_cocos_request_content_text(cb); }

const char *husk_native_loaded_apk(void) { return atomic_load(&A.state) == HUSK_UNITY_IDLE ? NULL : A.apk; }

int husk_unity_state(void)
{
    if (A.engine == ENGINE_COCOS && atomic_load(&A.state) == HUSK_UNITY_RUNNING && tl_cocos_ended()) atomic_store(&A.state, HUSK_UNITY_ENDED);
    return atomic_load(&A.state);
}
static unsigned long engine_frames(void)
{
    return A.engine == ENGINE_GTA ? tl_gta_frames() : A.engine == ENGINE_UE4 ? tl_na_frames() : A.engine == ENGINE_SDL ? tl_sdl_frames() : A.engine == ENGINE_GAMEACTIVITY ? tl_ga_frames() : A.engine == ENGINE_COCOS ? tl_cocos_frames() : tl_unity_frames();
}
unsigned long husk_unity_frames(void) { return engine_frames(); }
void husk_unity_perf_snapshot(husk_unity_perf *out)
{
    if (A.engine == ENGINE_GAMEACTIVITY || A.engine == ENGINE_SDL || A.engine == ENGINE_UE4 || A.engine == ENGINE_GTA) {
        /* The game paces its own frames; the rate is how many it presented since the last look. */
        static unsigned long last; static struct timespec since;
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        unsigned long f = engine_frames();
        double dt = since.tv_sec ? (now.tv_sec - since.tv_sec) + (now.tv_nsec - since.tv_nsec) / 1e9 : 0;
        out->fps = dt > 0.05 ? (double)(f - last) / dt : 0;
        out->mean_ms = out->fps > 0 ? 1000.0 / out->fps : 0; out->max_ms = 0;
        last = f; since = now;
        return;
    }
    if (A.engine == ENGINE_COCOS) { tl_cocos_perf p; tl_cocos_perf_snapshot(&p); out->fps = p.fps; out->mean_ms = p.mean_ms; out->max_ms = p.max_ms; return; }
    tl_unity_perf p; tl_unity_perf_snapshot(&p); out->fps = p.fps; out->mean_ms = p.mean_ms; out->max_ms = p.max_ms;
}
void husk_unity_touch(int phase, int id, float x, float y)
{
    if (atomic_load(&A.state) != HUSK_UNITY_RUNNING) return;
    if (A.engine == ENGINE_GTA) tl_gta_touch(phase, id, x, y);
    else if (A.engine == ENGINE_UE4) tl_na_touch(phase, id, x, y);
    else if (A.engine == ENGINE_SDL) tl_sdl_touch(phase, id, x, y);
    else if (A.engine == ENGINE_GAMEACTIVITY) tl_ga_touch(phase, id, x, y);
    else if (A.engine == ENGINE_COCOS) tl_cocos_touch(phase, id, x, y); else tl_unity_touch(phase, id, x, y);
}
void husk_unity_set_paused(bool paused)
{
    if (atomic_load(&A.state) != HUSK_UNITY_RUNNING) return;
    if (A.engine == ENGINE_GTA) { tl_gta_set_paused(paused); tl_audio_set_paused(paused); }
    else if (A.engine == ENGINE_UE4) { tl_na_set_paused(paused); tl_audio_set_paused(paused); }
    else if (A.engine == ENGINE_SDL) { tl_sdl_set_paused(paused); tl_audio_set_paused(paused); }
    else if (A.engine == ENGINE_GAMEACTIVITY) { tl_ga_set_paused(paused); tl_audio_set_paused(paused); }
    else if (A.engine == ENGINE_COCOS) { tl_cocos_set_paused(paused); tl_audio_set_paused(paused); } else tl_unity_set_paused(paused);
}

/* ------------------------------------------------------------- package name */

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* One string of an Android binary XML string pool, as UTF-8 into out. */
static bool pool_string(const uint8_t *pool, size_t pool_size, uint32_t index, char *out, size_t n)
{
    uint32_t count = rd32(pool + 8), flags = rd32(pool + 16), strings = rd32(pool + 20);
    if (index >= count || 28 + 4ull * index + 4 > pool_size) return false;
    size_t off = strings + rd32(pool + 28 + 4 * index);
    if (off + 4 > pool_size) return false;
    const uint8_t *p = pool + off;
    if (flags & 0x100) {                                        /* UTF-8: char length, byte length, bytes */
        size_t l = *p++; if (l & 0x80) p++;
        size_t b = *p++; if (b & 0x80) b = ((b & 0x7F) << 8) | *p++;
        if (b >= n) b = n - 1;
        memcpy(out, p, b); out[b] = 0;
    } else {                                                    /* UTF-16 */
        size_t l = rd16(p); p += 2;
        if (l & 0x8000) { l = ((l & 0x7FFF) << 16) | rd16(p); p += 2; }
        size_t k = 0;
        for (size_t i = 0; i < l && k + 1 < n; i++) out[k++] = (char)rd16(p + 2 * i);   /* package names are ASCII */
        out[k] = 0;
    }
    return true;
}

bool husk_unity_package_name(const char *apk, char *out, unsigned long out_len)
{
    tl_zip z;
    char err[160];
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return false;
    bool ok = false;
    const tl_zip_entry *e = tl_zip_find(&z, "AndroidManifest.xml");
    const uint8_t *data; size_t len; bool owned = false;
    if (e && tl_zip_data(&z, e, 8u << 20, &data, &len, &owned, err, sizeof(err)) && len > 8 && rd16(data) == 0x0003) {
        const uint8_t *pool = NULL; size_t pool_size = 0;
        for (size_t off = rd16(data + 2); off + 8 <= len; ) {
            uint16_t type = rd16(data + off); uint32_t size = rd32(data + off + 4);
            if (size < 8 || off + size > len) break;
            if (type == 0x0001) { pool = data + off; pool_size = size; }
            else if (type == 0x0102 && pool) {                  /* the first element is <manifest> */
                const uint8_t *el = data + off;
                uint16_t astart = rd16(el + 24), asize = rd16(el + 26), acount = rd16(el + 28);
                for (unsigned i = 0; i < acount; i++) {
                    const uint8_t *at = el + 16 + astart + (size_t)i * asize;
                    char name[40];
                    if (pool_string(pool, pool_size, rd32(at + 4), name, sizeof(name)) && !strcmp(name, "package")
                        && rd32(at + 8) != 0xFFFFFFFFu && pool_string(pool, pool_size, rd32(at + 8), out, out_len)) { ok = true; break; }
                }
                break;
            }
            off += size;
        }
    }
    if (owned) free((void *)data);
    tl_zip_close(&z);
    return ok;
}
