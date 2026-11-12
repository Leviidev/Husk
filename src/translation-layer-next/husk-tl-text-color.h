/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_TEXT_COLOR_H
#define HUSK_TL_TEXT_COLOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static inline bool tl_text_color_hex(uint32_t c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/* The client leaks six-digit RGB controls and four/five-digit fragments.
 * A longer hex run is a six-digit control touching text such as "Ban" or "100";
 * consuming eight would remove real game text. Inspect the full source so a
 * styled run inside a tag stays hidden.
 * Keep ordinary carets, short hexadecimal expressions, and non-ASCII text. */
#define TL_TEXT_COLOR_END(bits) \
static inline size_t tl_text_color_end##bits(const uint##bits##_t *text, size_t pos, size_t len) \
{ \
    if (pos >= len || text[pos] != '^' || (pos && text[pos - 1] == '^')) return pos; \
    size_t start = pos; \
    while (start < len && text[start] == '^') start++; \
    if (start < len && text[start] == ' ') { \
        if (len - start < 7) return pos; \
        for (size_t j = 1; j <= 6; j++) if (!tl_text_color_hex(text[start + j])) return pos; \
        start++; \
    } \
    size_t end = start; \
    while (end < len && end - start < 6 && tl_text_color_hex(text[end])) end++; \
    size_t digits = end - start; \
    return digits >= 4 ? end : pos; \
}
TL_TEXT_COLOR_END(16)
TL_TEXT_COLOR_END(32)
#undef TL_TEXT_COLOR_END

typedef struct {
    const uint16_t *text;
    size_t source_count, count;
    uint16_t *owned;
    /* Source UTF-16 boundary -> visible UTF-16 boundary. NULL means identity. */
    size_t *visible_at;
} tl_text_color_view16;

static inline void tl_text_color_view16_dispose(tl_text_color_view16 *view)
{
    free(view->owned);
    free(view->visible_at);
}

/* Failed allocation leaves an intact, unmodified view of the requested range.
 * Mapping is needed only for APIs returning positions in the original string. */
static inline tl_text_color_view16 tl_text_color_view16_make(const uint16_t *text,
    size_t len, size_t start, size_t end, bool enabled, bool mapping)
{
    if (start > len) start = len;
    if (end > len) end = len;
    if (end < start) end = start;
    tl_text_color_view16 view = { .text = text ? text + start : NULL,
        .source_count = end - start, .count = end - start };
    if (!enabled || !text || start == end) return view;
    bool changed = false;
    for (size_t i = 0; i < end;) {
        size_t next = tl_text_color_end16(text, i, len);
        if (next != i) {
            if (next > start) { changed = true; break; }
            i = next;
        } else i++;
    }
    if (!changed || view.source_count > SIZE_MAX / sizeof(size_t) - 1) return view;
    uint16_t *out = malloc((view.source_count + 1) * sizeof(*out));
    size_t *map = mapping ? malloc((view.source_count + 1) * sizeof(*map)) : NULL;
    if (!out || (mapping && !map)) { free(out); free(map); return view; }
    size_t at = 0;
    if (map) map[0] = 0;
    for (size_t i = 0; i < end;) {
        size_t next = tl_text_color_end16(text, i, len);
        if (next != i) {
            size_t stop = next < end ? next : end;
            for (size_t j = i < start ? start : i; j < stop; j++)
                if (map) map[j + 1 - start] = at;
            i = next;
        } else {
            if (i >= start) {
                out[at++] = text[i];
                if (map) map[i + 1 - start] = at;
            }
            i++;
        }
    }
    out[at] = 0;
    view.text = view.owned = out;
    view.count = at;
    view.visible_at = map;
    return view;
}

static inline size_t tl_text_color_visible16(const tl_text_color_view16 *view, size_t source_boundary)
{
    if (source_boundary > view->source_count) source_boundary = view->source_count;
    return view->visible_at ? view->visible_at[source_boundary] : source_boundary;
}

static inline size_t tl_text_color_source16(const tl_text_color_view16 *view, size_t visible_boundary)
{
    if (visible_boundary > view->count) visible_boundary = view->count;
    if (!view->visible_at) return visible_boundary;
    size_t source = 0;
    while (source < view->source_count && view->visible_at[source + 1] <= visible_boundary) source++;
    return source;
}
#endif
