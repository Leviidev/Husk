/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Husk -- the entire surface between the iOS app and QEMU.
 *
 * The app links against libqemu-aarch64-softmmu.dylib but must never include a
 * QEMU header: QEMU's console.h drags in most of the emulator's internals and
 * cannot be compiled by an Xcode target. Everything the app needs is these few
 * functions, which live inside the dylib and speak plain C.
 */
#ifndef HUSK_DISPLAY_H
#define HUSK_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * QEMU compiles most of itself into static libraries with -fvisibility=hidden,
 * so only the handful of symbols it deliberately publishes (qemu_init,
 * qemu_main_loop, bql_lock_impl, the plugin API) end up exported from the dylib.
 * An explicit attribute wins over the command-line default, so everything the
 * app needs to reach must be marked.
 */
#define HUSK_EXPORT __attribute__((visibility("default")))

typedef struct HuskFrameInfo {
    const void *pixels;
    int32_t  width;
    int32_t  height;
    int32_t  stride;      /* bytes per row */
    uint32_t bpp;         /* bits per pixel; 32 in every case we care about */
    uint64_t generation;  /* bumps whenever the surface is replaced (resize) */
    uint64_t sequence;    /* bumps on every guest draw */
} HuskFrameInfo;

/*
 * Register Husk's DisplayChangeListener. Must be called on the QEMU thread
 * after qemu_init() and before qemu_main_loop().
 */
HUSK_EXPORT void husk_display_init(void);

/*
 * Borrow the current frame. Returns false when the guest has not produced a
 * surface yet. On true, `out` is valid until husk_display_unlock_frame().
 *
 * Holds a lock that blocks surface replacement, so unlock promptly -- upload the
 * pixels to a texture and get out.
 */
HUSK_EXPORT bool husk_display_lock_frame(HuskFrameInfo *out);
HUSK_EXPORT void husk_display_unlock_frame(void);

/* Cheap poll: has anything been drawn since this sequence number? */
HUSK_EXPORT uint64_t husk_display_sequence(void);

/* Ask the guest to modeset to this size. See the comment in the .c. */
HUSK_EXPORT void husk_display_set_ui_size(int32_t width, int32_t height);

/* The guest's real resolution, which may not be what we asked for. */
HUSK_EXPORT void husk_display_guest_size(int32_t *width, int32_t *height);

/*
 * Absolute pointer input, in guest pixels. Takes the BQL internally, so it is
 * safe to call from the UI thread.
 */
HUSK_EXPORT void husk_display_send_pointer(int32_t x, int32_t y, bool button_down);

/* Ask the guest to redraw. Safe from any thread. */
/*
 * Send one key transition to the guest. `qcode_name` is a QEMU QKeyCode name --
 * "a", "ret", "shift", "left", "f1" and so on. Returns false if the name is not
 * a key QEMU knows. Takes the BQL internally, so it is safe from the UI thread.
 */
HUSK_EXPORT bool husk_display_send_key(const char *qcode_name, bool down);

HUSK_EXPORT void husk_display_request_update(void);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_DISPLAY_H */
