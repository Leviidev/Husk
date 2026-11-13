/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * EGL and OpenGL ES for guest code, over ANGLE.
 *
 * Android's EGL is a C API like any other and ANGLE implements it over Metal, so most calls
 * go straight through. What this layer adds:
 *
 *   - the display is made as an ANGLE/Metal display, whatever the guest asked for;
 *   - the guest's window is an ANativeWindow, which becomes a CAMetalLayer on the phone and,
 *     for tests on a Mac, an off-screen buffer whose contents are saved as images;
 *   - a few GLES functions take stack arguments that AAPCS64 (the guest) and Apple's ABI (ANGLE)
 *     lay out differently; those get adapters;
 *   - GLES names are resolved through eglGetProcAddress, which is how Android apps find them.
 */
#ifndef HUSK_TL_EGL_H
#define HUSK_TL_EGL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Load ANGLE. `egl_path` / `gles_path` are the two libraries (gles_path may be NULL when one library
 * holds both, as on the phone). With `frame_dir` set the guest's window is off-screen and every
 * `frame_every`th presented frame is written there as a BMP.
 */
/* Describe the driver as OpenGL ES 3.1 and write the game's "#version 310 es" shaders down to 300 es (see husk-tl-egl-es31.inc) */
void tl_egl_es31_shim(bool on);
void tl_egl_offscreen_windows(bool on);     /* window surfaces are off-screen buffers: the layer belongs to another API (Vulkan) */
bool tl_egl_init(const char *egl_path, const char *gles_path, const char *frame_dir, int frame_every);

/* The address of a GLES/EGL function by name, adapters first, or NULL. */
void *tl_egl_resolve(const char *name);

unsigned long tl_egl_frames_presented(void);

/* Shared GPU frame handoff. The compositor brackets metadata acquisition and
 * draw submission with begin/end, and waits before sampling the selected tex.
 * Its current shared context must remain current throughout these calls. */
void tl_egl_compositor_context(void *context, bool es3);
void tl_egl_compositor_frame_begin(void);
void tl_egl_compositor_frame_wait(unsigned tex);
void tl_egl_compositor_frame_end(unsigned tex);
void tl_egl_compositor_frame_reset(void *context, bool current);
/* Diagnostic sampling only: discard idle/old-session samples on pause/stop. */
void tl_egl_frame_perf_set_active(bool active);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_EGL_H */
