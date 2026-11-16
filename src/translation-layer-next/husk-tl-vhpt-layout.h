/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Pure HUD alignment math for inspected VHPT (com.tech3cgame.vhpt1).
 * Matches VHTG's outer-edge Character box and below-left alignment while keeping
 * the Quest list panel (Nhiệm vụ) completely unchanged and avoiding notch/Dynamic
 * Island hardware pills and rounded screen corners across supported iPhones. */
#ifndef HUSK_TL_VHPT_LAYOUT_H
#define HUSK_TL_VHPT_LAYOUT_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Verified ELF/layout offsets for VHPTMobile-release-20261105.apk libMyGame.so */
#define VHPT_LIB_SIZE           15798568u
#define VHPT_STR_ANCHOR_OFF     0x1acdc9u
#define VHPT_STR_TOPLEFT_OFF    0x19edefu
#define VHPT_STR_CHARINFO_OFF   0x174474u
#define VHPT_BSS_SAFE_L_OFF     0xf195c0u
#define VHPT_BSS_SAFE_T_OFF     0xf195c4u
#define VHPT_BSS_SAFE_R_OFF     0xf195c8u
#define VHPT_BSS_SAFE_B_OFF     0xf195ccu
#define VHPT_BSS_SAFE_GEN_OFF   0xf195d0u
#define VHPT_BSS_GAMESCENE_OFF  0xf19ca0u

/* cocos2d::Node offsets in VHPT libMyGame.so */
#define VHPT_NODE_SCALEX_OFF    0x44u
#define VHPT_NODE_POSX_OFF      0x50u
#define VHPT_NODE_POSY_OFF      0x54u
#define VHPT_NODE_CHILDREN_VEC  0x198u
#define VHPT_NODE_VTBL_SETPOS   0x98u
#define VHPT_INI_ANCHOR_STR_OFF 0x60u

typedef struct tl_vhpt_panel_slot {
    uint32_t scene_off;
    uint32_t ini_off;
    uint32_t root_off;
    const char *name;
    bool keep_unchanged;
} tl_vhpt_panel_slot;

static inline const tl_vhpt_panel_slot *tl_vhpt_panel_slots(size_t *count)
{
    static const tl_vhpt_panel_slot k_slots[] = {
        { 0x3b0u, 0x300u, 0x328u, "charinfo",   false }, /* Character box */
        { 0x3b8u, 0x300u, 0x328u, "petinfo",    false }, /* Pet box directly below Character */
        { 0x558u, 0x300u, 0x348u, "buff",       false }, /* Buff bar below Character/Pet */
        { 0x550u, 0x300u, 0x330u, "quickequip", false }, /* Quick equip popup/button */
        { 0x4b0u, 0x2f8u, 0x348u, "notify",     false }, /* Left notify buttons */
        { 0x548u, 0x300u, 0x308u, "quickmenu",  false }, /* QuickMenu rail/buttons */
        { 0x498u, 0x2f8u, 0x340u, "chat",       false }, /* Bottom-left chat bar */
        { 0x5a8u, 0x300u, 0x3f8u, "shopself",   false }, /* PlayerMenu ShopSelf (BottomLeft) */
        { 0x5f0u, 0x308u, 0x368u, "questlist",  true  }, /* Quest list (Nhiệm vụ): MUST stay unchanged */
    };
    if (count) *count = sizeof(k_slots) / sizeof(k_slots[0]);
    return k_slots;
}

static inline bool tl_vhpt_is_left_anchor(const char *anchor)
{
    if (!anchor || !anchor[0]) return true;
    return !strstr(anchor, "Right") && !strstr(anchor, "Center");
}

/* Outer-edge left origin in surface pixels matching VHTG (~7-10 pt bezel clearance
 * so curved OLED display corners do not clip portrait rings or leftmost icons). */
static inline float tl_vhpt_outer_left_px(float safe_l, float vis_h)
{
    if (safe_l <= 0.5f) return 0.0f;
    float corner_pad = vis_h * 0.020f;
    if (corner_pad < 6.0f) corner_pad = 6.0f;
    if (corner_pad > safe_l) corner_pad = safe_l;
    return floorf(corner_pad + 0.5f);
}

/* Hardware notch / Dynamic Island vertical band on the left bezel in bottom-up
 * Cocos surface pixels. Dynamic Island occupies a narrower center band (~32% of
 * height) than a wide notch (~54% of height). */
static inline void tl_vhpt_cutout_band_px(float safe_l, float vis_h, float *y0, float *y1)
{
    if (safe_l <= 0.5f || vis_h <= 0.0f) { *y0 = 0.0f; *y1 = 0.0f; return; }
    float half_h = (safe_l / vis_h >= 0.132f) ? (vis_h * 0.165f) : (vis_h * 0.270f);
    *y0 = vis_h * 0.5f - half_h;
    *y1 = vis_h * 0.5f + half_h;
}

/* Compute aligned m_root X in surface pixels. Never moves questlist (Nhiệm vụ),
 * never moves Right/Center-anchored panels, and avoids placing a centered-Y panel
 * directly behind the physical notch/Dynamic Island pill. */
static inline float tl_vhpt_panel_target_x(bool keep_unchanged, const char *anchor,
    float cur_x, float safe_l, float vis_h, bool has_y_span, float min_y, float max_y)
{
    if (keep_unchanged || safe_l <= 0.5f || !tl_vhpt_is_left_anchor(anchor)) return cur_x;
    if (fabsf(cur_x - safe_l) > 2.0f) return cur_x;
    if (has_y_span && max_y > min_y) {
        float cy0 = 0.0f, cy1 = 0.0f;
        tl_vhpt_cutout_band_px(safe_l, vis_h, &cy0, &cy1);
        float ov = fminf(max_y, cy1) - fmaxf(min_y, cy0);
        if (ov > (max_y - min_y) * 0.35f) return safe_l;
    }
    return tl_vhpt_outer_left_px(safe_l, vis_h);
}

#endif
