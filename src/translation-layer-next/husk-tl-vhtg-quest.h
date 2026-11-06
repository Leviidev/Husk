/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_VHTG_QUEST_H
#define HUSK_TL_VHTG_QUEST_H
#include <stdbool.h>
/* True only for the inspected VHTG package/library. Unknown binaries are left untouched. */
/* Insets and dimensions must both use native framebuffer pixels, not Cocos visible units. */
bool tl_vhtg_quest_start(const char *apk, const char *package, bool requested, const int insets[4],
                         int pixel_width, int pixel_height);
bool tl_vhtg_quest_active(void);
void tl_vhtg_quest_insets(int l, int t, int r, int b);
#endif
