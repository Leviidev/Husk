/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_TRUTIEN_TEXT_H
#define HUSK_TL_TRUTIEN_TEXT_H
#include "husk-tl-ld.h"
/* Bounded, read-only test of the game's exported color-tag recognizer after
 * its static constructors. Does not change game strings, fonts or registry. */
void tl_trutien_text_probe(tl_lib *lib);
#endif
