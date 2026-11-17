/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_QUEST_LAYOUT_H
#define HUSK_TL_QUEST_LAYOUT_H
#include <math.h>
#include <stdbool.h>

typedef struct { double x, y, w, h; } tl_quest_rect;
typedef struct { double left, top, right, bottom; } tl_quest_cutout;

/* The original window's anchor formula, before the compatibility offset. */
static inline double tl_quest_anchor(double origin, double extent, double anchor,
                                     double size, double pivot, double offset, double scale)
{
    return floor(origin + anchor * extent - pivot * size + offset * scale + 0.5);
}

/* Only the explicitly selected quest window is translated. Full scene bounds never change. */
static inline bool tl_quest_delta(tl_quest_rect scene, tl_quest_rect window, tl_quest_cutout cut,
                                  double pixel_w, double pixel_h, double *dx, double *dy)
{
    *dx = *dy = 0;
    double values[] = {scene.x,scene.y,scene.w,scene.h,window.x,window.y,window.w,window.h,
                       cut.left,cut.top,cut.right,cut.bottom,pixel_w,pixel_h};
    for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); i++) if (!isfinite(values[i])) return false;
    if (scene.w <= 0 || scene.h <= 0 || window.w <= 0 || window.h <= 0 || pixel_w <= 0 || pixel_h <= 0) return false;
    double pad_x = (cut.left > 0 || cut.right > 0) ? ceil(scene.w * (16.0 / 932.0)) : 0;
    double l = scene.x + fmax(0,cut.left) * scene.w / pixel_w + (cut.left > 0 ? pad_x : 0);
    double r = scene.x + scene.w - fmax(0,cut.right) * scene.w / pixel_w - (cut.right > 0 ? pad_x : 0);
    double t = scene.y + fmax(0,cut.top) * scene.h / pixel_h;
    double b = scene.y + scene.h - fmax(0,cut.bottom) * scene.h / pixel_h;
    // A tall quest container must not disable horizontal camera avoidance.
    if (((cut.left > 0 || cut.right > 0) && r-l < window.w) ||
        ((cut.top > 0 || cut.bottom > 0) && b-t < window.h)) return false;
    if (cut.left > 0 && window.x < l) *dx = l-window.x;
    else if (cut.right > 0 && window.x+window.w > r) *dx = r-window.x-window.w;
    if (cut.top > 0 && window.y < t) *dy = t-window.y;
    else if (cut.bottom > 0 && window.y+window.h > b) *dy = b-window.y-window.h;
    *dx = ceil(fabs(*dx)) * (*dx < 0 ? -1 : 1);
    *dy = ceil(fabs(*dy)) * (*dy < 0 ? -1 : 1);
    return true;
}

/* A downward adjustment for the two movement buttons, bounded by the picture.
 * 20/430 is twenty screen points on the user's 430-point-tall landscape surface.
 * Normalizing by scene height keeps the result independent of render resolution. */
static inline double tl_quest_movement_down(tl_quest_rect scene, tl_quest_rect window)
{
    if (!isfinite(scene.y) || !isfinite(scene.h) || !isfinite(window.y) || !isfinite(window.h) ||
        scene.h <= 0 || window.h <= 0) return 0;
    double room = scene.y + scene.h - window.y - window.h;
    return floor(fmax(0, fmin(scene.h * (20.0/430.0), room)));
}

/* Compute (dx, dy) for a movement button/cluster so it sits near the outer bezel
 * (~8-9 pt curved-corner pad when clipped by a cutout) AND clear below the notch /
 * Dynamic Island bottom edge without leaving the scene. */
static inline void tl_quest_movement_adjust(tl_quest_rect scene, tl_quest_rect window,
                                            tl_quest_cutout cut, double pixel_w, double pixel_h,
                                            double *dx, double *dy)
{
    *dx = 0; *dy = 0;
    if (!isfinite(scene.x) || !isfinite(scene.y) || !isfinite(scene.w) || !isfinite(scene.h) ||
        !isfinite(window.x) || !isfinite(window.y) || !isfinite(window.w) || !isfinite(window.h) ||
        !isfinite(cut.left) || !isfinite(cut.right) || !isfinite(pixel_w) || !isfinite(pixel_h) ||
        scene.w <= 0 || scene.h <= 0 || window.w <= 0 || window.h <= 0 || pixel_w <= 0 || pixel_h <= 0) return;
    if (scene.w <= scene.h || (cut.left <= 0 && cut.right <= 0)) return;

    bool is_left = (window.x + window.w * 0.5) < (scene.x + scene.w * 0.5);
    double active_cut_px = is_left ? cut.left : cut.right;
    if (active_cut_px > 0) {
        double outer_pad = floor(fmax(6.0, scene.h * 0.020) + 0.5);
        double cut_scene = active_cut_px * scene.w / pixel_w;
        outer_pad = fmin(outer_pad, fmax(0.0, floor(cut_scene * 0.5)));
        if (is_left) {
            double target_x = scene.x + outer_pad;
            if (window.x > target_x + 0.5 || window.x < scene.x + fmin(outer_pad, 2.0))
                *dx = floor(target_x - window.x + 0.5);
        } else {
            double target_r = scene.x + scene.w - outer_pad;
            double cur_r = window.x + window.w;
            if (cur_r < target_r - 0.5 || cur_r > scene.x + scene.w - fmin(outer_pad, 2.0))
                *dx = floor(target_r - cur_r + 0.5);
        }

        double ratio = active_cut_px / pixel_h;
        double band_half = (ratio >= 0.132) ? 0.165 : 0.255;
        double band_bottom = scene.y + scene.h * (0.5 + band_half);
        double clear_gap = ceil(scene.h * (8.0 / 430.0));
        double min_down = tl_quest_movement_down(scene, window);
        double clear_down = fmax(0.0, ceil((band_bottom + clear_gap) - window.y));
        double desired_down = fmax(min_down, clear_down);
        double room = fmax(0.0, (scene.y + scene.h) - (window.y + window.h));
        *dy = floor(fmin(desired_down, room));
    } else {
        *dy = tl_quest_movement_down(scene, window);
    }
}
#endif
