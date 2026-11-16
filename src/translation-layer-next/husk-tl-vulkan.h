/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Vulkan for guest code, over MoltenVK.
 *
 * Android's libvulkan.so is the standard Vulkan C API and MoltenVK implements it over Metal, so nearly every
 * call goes straight through. What this layer changes is the part that is Android-specific:
 *
 *   - VK_KHR_android_surface is offered to the guest and, when it makes a surface from its ANativeWindow,
 *     that becomes a VK_EXT_metal_surface on the window's CAMetalLayer;
 *   - extensions MoltenVK lacks are dropped from the instance the guest asks for, so creation does not fail;
 *   - for tests on a Mac the window is an off-screen layer, and presented frames are saved as images.
 */
#ifndef HUSK_TL_VULKAN_H
#define HUSK_TL_VULKAN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Use the MoltenVK at `path`; NULL means the one already linked into the process. With `frame_dir` set, every `frame_every`th presented frame is written there as a BMP. */
void tl_vk_configure(const char *path, const char *frame_dir, int frame_every);

/* The address of a Vulkan function by name (what the guest's dlsym on libvulkan.so, and vkGetInstanceProcAddr, return), or NULL. */
void *tl_vk_resolve(const char *name);

/* True once tl_vk_configure has named a MoltenVK that loads. Without one the guest is told libvulkan.so does not exist, and games fall back to OpenGL ES. */
bool tl_vk_available(void);

unsigned long tl_vk_frames_presented(void);
void tl_vk_reset_session(void);

#ifdef __cplusplus
}
#endif

#endif
