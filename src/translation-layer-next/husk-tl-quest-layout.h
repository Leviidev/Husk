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
    double l = scene.x + fmax(0,cut.left) * scene.w / pixel_w;
    double r = scene.x + scene.w - fmax(0,cut.right) * scene.w / pixel_w;
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

/* A small downward adjustment for the two movement buttons, bounded by the picture.
 * 12/430 is twelve screen points on the user's 430-point-tall landscape surface.
 * Normalizing by scene height keeps the result independent of render resolution. */
static inline double tl_quest_movement_down(tl_quest_rect scene, tl_quest_rect window)
{
    if (!isfinite(scene.y) || !isfinite(scene.h) || !isfinite(window.y) || !isfinite(window.h) ||
        scene.h <= 0 || window.h <= 0) return 0;
    double room = scene.y + scene.h - window.y - window.h;
    return floor(fmax(0, fmin(scene.h * (12.0/430.0), room)));
}
#endif
