/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_TRUTIEN_TEXT_H
#define HUSK_TL_TRUTIEN_TEXT_H
#include "husk-tl-ld.h"
/* Bounded, read-only test of the game's exported color-tag recognizer after
 * its static constructors. Does not change game strings, fonts or registry. */
void tl_trutien_text_probe(tl_lib *lib);
void tl_trutien_text_set_package(const char *package_name);
/* Called by the precompiled native entry filters. Only saved text/count
 * argument registers change; stack arguments and the original string do not. */
void tl_trutien_font_filter(uint64_t *regs);
void tl_trutien_extent_filter(uint64_t *regs);
void tl_trutien_hud_filter(uint64_t *regs);
void tl_trutien_text_enter(uint64_t *regs, uint64_t *saved_link, void (*filter)(uint64_t *regs));
uintptr_t tl_trutien_text_leave(void);
#endif
