/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_VHTG_QUEST_H
#define HUSK_TL_VHTG_QUEST_H
#include <stdbool.h>
/* True only for the inspected VHTG package/library. Unknown binaries are left untouched. */
/* Insets and dimensions must both use native framebuffer pixels, not Cocos visible units. */
bool tl_vhtg_quest_start(const char *apk, const char *package, bool requested, const int insets[4],
                         int pixel_width, int pixel_height);
/* Align Character and below-left HUD panels for inspected VHPT (com.tech3cgame.vhpt1)
 * without altering the Quest panel or zeroing native insets. */
bool tl_vhpt_hud_start(const char *apk, const char *package, bool requested, const int insets[4],
                       int pixel_width, int pixel_height);
bool tl_vhtg_quest_active(void);
int tl_apk_cutout_profile_kind(const char *apk, const char *package);
void tl_vhtg_quest_insets(int l, int t, int r, int b);
void tl_vhtg_quest_reset(void);
#endif
