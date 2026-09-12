/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Husk display-bridge probe.
 *
 * Drives the real patched QEMU exactly the way the iOS app does -- qemu_init and
 * qemu_main_loop on a secondary thread, husk_display_init between them -- and
 * reports whether the DisplayChangeListener actually receives frames from the
 * guest.
 *
 * The point is to test the bridge somewhere it can be observed. The same
 * husk-display.c runs on the phone, so a surface appearing here with sane
 * dimensions, stride and format means the only untested part left on device is
 * the JIT allocation and the Metal blit.
 *
 * Build: see scripts/run_display_probe.sh
 */
#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct HuskFrameInfo {
    const void *pixels;
    int32_t  width, height, stride;
    uint32_t bpp;
    uint64_t generation, sequence;
} HuskFrameInfo;

static void     (*p_qemu_init)(int, char **);
static void     (*p_qemu_main_loop)(void);
static void     (*p_husk_display_init)(void);
static bool     (*p_husk_display_lock_frame)(HuskFrameInfo *);
static void     (*p_husk_display_unlock_frame)(void);
static uint64_t (*p_husk_display_sequence)(void);
static void     (*p_husk_display_request_update)(void);
static void     (*p_husk_display_send_pointer)(int32_t, int32_t, bool);

static char **g_argv;
static int    g_argc;

static void *qemu_thread(void *unused)
{
    (void)unused;
    printf("[probe] qemu_init()\n");
    fflush(stdout);
    p_qemu_init(g_argc, g_argv);

    printf("[probe] husk_display_init()\n");
    fflush(stdout);
    p_husk_display_init();

    printf("[probe] qemu_main_loop()\n");
    fflush(stdout);
    p_qemu_main_loop();
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <libqemu.dylib> <seconds> [qemu args...]\n", argv[0]);
        return 2;
    }
    const char *libpath = argv[1];
    int seconds = atoi(argv[2]);

    void *h = dlopen(libpath, RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "[probe] dlopen failed: %s\n", dlerror()); return 1; }

#define SYM(v, n) do { \
        *(void **)(&v) = dlsym(h, n); \
        if (!v) { fprintf(stderr, "[probe] missing symbol %s\n", n); return 1; } \
    } while (0)
    SYM(p_qemu_init, "qemu_init");
    SYM(p_qemu_main_loop, "qemu_main_loop");
    SYM(p_husk_display_init, "husk_display_init");
    SYM(p_husk_display_lock_frame, "husk_display_lock_frame");
    SYM(p_husk_display_unlock_frame, "husk_display_unlock_frame");
    SYM(p_husk_display_sequence, "husk_display_sequence");
    SYM(p_husk_display_request_update, "husk_display_request_update");
    SYM(p_husk_display_send_pointer, "husk_display_send_pointer");
#undef SYM
    printf("[probe] all husk symbols resolved from the dylib\n");

    /* QEMU's argv: argv[0] plus everything after <seconds>. */
    g_argc = argc - 2;
    g_argv = calloc(g_argc + 1, sizeof(char *));
    g_argv[0] = strdup("qemu-system-aarch64");
    for (int i = 3; i < argc; i++) {
        g_argv[i - 2] = strdup(argv[i]);
    }

    pthread_t t;
    pthread_create(&t, NULL, qemu_thread, NULL);

    bool saw_surface = false;
    uint64_t last_seq = 0, max_seq = 0;
    int32_t w = 0, h2 = 0, stride = 0;
    uint32_t bpp = 0;

    for (int i = 0; i < seconds * 10; i++) {
        usleep(100 * 1000);
        if (!p_husk_display_init) break;

        HuskFrameInfo info;
        memset(&info, 0, sizeof(info));
        if (p_husk_display_lock_frame(&info)) {
            if (!saw_surface) {
                printf("[probe] FIRST SURFACE at t=%.1fs: %dx%d stride=%d bpp=%u gen=%llu\n",
                       i / 10.0, info.width, info.height, info.stride, info.bpp,
                       (unsigned long long)info.generation);
                fflush(stdout);
            }
            saw_surface = true;
            w = info.width; h2 = info.height; stride = info.stride; bpp = info.bpp;
            if (info.sequence > max_seq) max_seq = info.sequence;
            p_husk_display_unlock_frame();
        }
        p_husk_display_request_update();

        /* Exercise the input path once the guest is up. */
        if (saw_surface && i == seconds * 5) {
            printf("[probe] sending a pointer event to (%d,%d)\n", w / 2, h2 / 2);
            p_husk_display_send_pointer(w / 2, h2 / 2, true);
            p_husk_display_send_pointer(w / 2, h2 / 2, false);
            printf("[probe] pointer event returned without deadlock\n");
            fflush(stdout);
        }
        last_seq = max_seq;
    }

    printf("\n========== RESULT ==========\n");
    printf("surface seen : %s\n", saw_surface ? "YES" : "NO");
    if (saw_surface) {
        printf("dimensions   : %dx%d\n", w, h2);
        printf("stride       : %d bytes (%d bytes/pixel)\n", stride, w ? stride / w : 0);
        printf("bpp          : %u\n", bpp);
        printf("draw events  : %llu\n", (unsigned long long)last_seq);
    }
    printf("============================\n");
    return saw_surface ? 0 : 1;
}
