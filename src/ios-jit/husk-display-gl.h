#ifndef HUSK_DISPLAY_GL_H
#define HUSK_DISPLAY_GL_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Bring up the GL display path against a CAMetalLayer. Must be called on the
 * QEMU thread after qemu_init(), and instead of husk_display_init() -- the two
 * register competing DisplayChangeListeners for the same console.
 */
/*
 * Must be called BEFORE qemu_init(). Brings up EGL and sets display_opengl, so
 * virtio-gpu-gl is allowed to realize -- devices are created inside qemu_init(),
 * before any DisplayChangeListener can exist.
 */
bool husk_display_gl_early(void);

bool husk_display_gl_init(void *native_layer, int width, int height);

/* Frames presented, for the perf counter. */
uint64_t husk_display_gl_frames(void);

#endif
