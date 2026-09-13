/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The app's view of libqemu-aarch64-softmmu.dylib.
 *
 * Deliberately hand-written rather than including QEMU's own headers: ui/console.h
 * pulls in most of the emulator and cannot be compiled by an Xcode target. These
 * declarations must stay in step with src/ios-jit/husk-display.h and
 * src/ios-jit/husk-ios-jit.h, and with system/qemu.symbols in the QEMU tree --
 * a symbol missing from that file links fine and fails at load.
 */
#ifndef HUSK_BRIDGE_H
#define HUSK_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

/* Mirrors HuskFrameInfo in husk-display.h. */
typedef struct HuskFrameInfo {
    const void *pixels;
    int32_t  width;
    int32_t  height;
    int32_t  stride;
    uint32_t bpp;
    uint64_t generation;
    uint64_t sequence;
} HuskFrameInfo;

/* --- QEMU's own public API (system/qemu.symbols) --- */
void qemu_init(int argc, char **argv);
void qemu_main_loop(void);
void qemu_cleanup(void);

/* --- Husk's display/input bridge --- */
void     husk_display_init(void);
bool     husk_display_lock_frame(HuskFrameInfo *out);
void     husk_display_unlock_frame(void);
uint64_t husk_display_sequence(void);
void     husk_display_send_pointer(int32_t x, int32_t y, bool button_down);
bool     husk_display_send_key(const char *qcode_name, bool down);
void     husk_display_request_update(void);

/* --- Husk's JIT substrate --- */
void husk_ios_jit_install_trap_handler(void);
/* Claim the JIT region while StikDebug is still attached, before the guest
   download. Pass the same size QEMU will ask for (tb-size). */
bool husk_ios_jit_prewarm(size_t bytes);
bool husk_ios_jit_is_available(void);
void husk_ios_jit_detach(void);
void husk_ios_jit_log_footprint(const char *tag);
size_t husk_ios_available_memory(void);

/* --- Husk's GL display path --- */
/* Bring the GL display up against a CAMetalLayer. Returns false if EGL, the
   surface, or the console could not be set up, in which case the caller should
   fall back to husk_display_init(). */
/* Must be called BEFORE qemu_init(): sets display_opengl so virtio-gpu-gl can
   realize, since devices are created inside qemu_init(). */
bool     husk_display_gl_early(void);
/* MAIN THREAD: ANGLE sets up a CAMetalLayer here, and CALayer is not thread-safe. */
bool     husk_display_gl_create(void *native_layer, int32_t width, int32_t height);
/* QEMU thread: make the context current and register the listener. */
/* Is GL usable here? Registers nothing. */
bool     husk_display_gl_probe(void);
bool     husk_display_gl_bind(void);
uint64_t husk_display_gl_frames(void);
void     husk_display_set_ui_size(int32_t width, int32_t height);

/* Audio, from husk-audio.c. The format is fixed there and mirrored here so the
 * render callback never has to negotiate one. */
#define HUSK_AUDIO_RATE     48000
#define HUSK_AUDIO_CHANNELS 2
int32_t  husk_audio_pull(int16_t *dst, int32_t frames);
bool     husk_audio_active(void);
uint64_t husk_audio_frames_in(void);
uint64_t husk_audio_underruns(void);

/* Present the guest's scanout from its own MTLTexture, bypassing GL. See the
 * comment in husk-display-gl.h: on this stack the GL texture id that comes with
 * each scanout is not readable from our context, and the pixels are in the
 * native Metal handle instead. `texture` is an id<MTLTexture> and is valid only
 * for the duration of the call. */
typedef void (*husk_metal_present_fn)(void *texture, int32_t flip,
                                      int32_t width, int32_t height);
void     husk_display_gl_set_metal_presenter(husk_metal_present_fn fn);

/* --- Husk's machine snapshots --- */
/* Restore the saved machine, if one exists. Call straight after qemu_init() and
   before qemu_main_loop(). False means nothing to restore, which is the normal
   first-run case rather than a failure. */
bool husk_snapshot_load_at_startup(void);
/* Save the running machine. Asynchronous; the vCPUs stop for the duration. */
void husk_snapshot_save(void (*cb)(bool ok, const char *what));

/* --- Husk's guest memory balloon --- */
/* Ask the guest to shrink to, or grow back to, this much usable RAM. Safe from
   any thread; the request is asynchronous, so treat it as steering rather than
   as an allocation that has already happened. */
void husk_balloon_set_bytes(int64_t target_bytes);

#endif /* HUSK_BRIDGE_H */
