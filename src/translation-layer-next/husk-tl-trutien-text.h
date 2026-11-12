/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_TRUTIEN_TEXT_H
#define HUSK_TL_TRUTIEN_TEXT_H
#include "husk-tl-ld.h"
#include <stdbool.h>
/* Bounded, read-only test of the game's exported color-tag recognizer after
 * its static constructors. Does not change game strings, fonts or registry. */
void tl_trutien_text_probe(tl_lib *lib);
void tl_trutien_text_set_package(const char *package_name);
bool tl_trutien_text_active(void);
/* Called by the precompiled native entry filters. A counted stack argument is
 * restored on return; original strings and all other arguments are preserved. */
typedef void (*tl_trutien_text_filter)(uint64_t *regs, void *guest_sp);
void tl_trutien_font_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_extent_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_hud_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_hud_counted_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_plain_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_float_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_clipped_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_legacy_extent_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_params_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_hud_context_filter(uint64_t *regs, void *guest_sp);
void tl_trutien_text_enter(uint64_t *regs, uint64_t *saved_link, tl_trutien_text_filter filter, void *guest_sp);
uintptr_t tl_trutien_text_leave(void);
#endif
