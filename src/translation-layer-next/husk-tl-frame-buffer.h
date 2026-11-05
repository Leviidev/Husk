/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_FRAME_BUFFER_H
#define HUSK_TL_FRAME_BUFFER_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Capacity belongs to the allocation, not to the dimensions of the most recently displayed
 * texture. Failed growth preserves the previous allocation and the caller's last good frame. */
static inline bool tl_rgba_buffer_reserve(uint8_t **pixels, size_t *capacity, int width, int height)
{
    if (width <= 0 || height <= 0 || (size_t)width > SIZE_MAX / 4 / (size_t)height) return false;
    size_t bytes = (size_t)width * (size_t)height * 4;
    if (*pixels && *capacity >= bytes) return true;
    uint8_t *next = realloc(*pixels, bytes);
    if (!next) return false;
    *pixels = next;
    *capacity = bytes;
    return true;
}
#endif
