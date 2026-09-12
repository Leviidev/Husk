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
void     husk_display_request_update(void);

/* --- Husk's JIT substrate --- */
void husk_ios_jit_install_trap_handler(void);
bool husk_ios_jit_is_available(void);
void husk_ios_jit_detach(void);
void husk_ios_jit_log_footprint(const char *tag);
size_t husk_ios_available_memory(void);

#endif /* HUSK_BRIDGE_H */
