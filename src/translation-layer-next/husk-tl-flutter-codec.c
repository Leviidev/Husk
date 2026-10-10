/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Flutter's StandardMessageCodec (see husk-tl-flutter-codec.h). */
#include "husk-tl-flutter-codec.h"

#include <stdlib.h>
#include <string.h>

enum { W_NULL = 0, W_TRUE = 1, W_FALSE = 2, W_INT32 = 3, W_INT64 = 4, W_LARGEINT = 5, W_FLOAT64 = 6, W_STRING = 7, W_UINT8 = 8,
       W_INT32LIST = 9, W_INT64LIST = 10, W_FLOAT64LIST = 11, W_LIST = 12, W_MAP = 13, W_FLOAT32LIST = 14 };

static sv *mk(int type) { sv *v = calloc(1, sizeof(*v)); v->type = type; return v; }
sv *sv_null(void) { return mk(SV_NULL); }
sv *sv_bool(bool b) { sv *v = mk(SV_BOOL); v->i = b; return v; }
sv *sv_int(int64_t i) { sv *v = mk(SV_INT); v->i = i; return v; }
sv *sv_double(double d) { sv *v = mk(SV_DOUBLE); v->d = d; return v; }
sv *sv_string(const char *s) { sv *v = mk(SV_STRING); v->s = strdup(s ? s : ""); return v; }
sv *sv_list(size_t n) { sv *v = mk(SV_LIST); v->n = n; v->items = calloc(n ? n : 1, sizeof(sv)); return v; }
sv *sv_map(size_t n) { sv *v = mk(SV_MAP); v->n = n; v->items = calloc(n ? 2 * n : 1, sizeof(sv)); return v; }

static void clear(sv *v)
{
    if (!v) return;
    free(v->s); free(v->bytes);
    size_t m = v->type == SV_MAP ? 2 * v->n : v->type == SV_LIST ? v->n : 0;
    for (size_t i = 0; i < m; i++) clear(&v->items[i]);
    free(v->items);
    memset(v, 0, sizeof(*v));
}
void sv_free(sv *v) { if (v) { clear(v); free(v); } }

static void deep(sv *dst, const sv *src)
{
    *dst = *src;
    if (src->s) dst->s = strdup(src->s);
    if (src->bytes) { dst->bytes = malloc(src->nbytes ? src->nbytes : 1); memcpy(dst->bytes, src->bytes, src->nbytes); }
    size_t m = src->type == SV_MAP ? 2 * src->n : src->type == SV_LIST ? src->n : 0;
    if (src->items) { dst->items = calloc(m ? m : 1, sizeof(sv)); for (size_t i = 0; i < m; i++) deep(&dst->items[i], &src->items[i]); }
}
sv *sv_copy(const sv *v) { if (!v) return NULL; sv *c = calloc(1, sizeof(*c)); deep(c, v); return c; }

void sv_set(sv *list, size_t i, sv *v) { if (!list || i >= list->n) { sv_free(v); return; } clear(&list->items[i]); list->items[i] = *v; free(v); }
void sv_map_set(sv *map, size_t i, sv *key, sv *val)
{
    if (!map || map->type != SV_MAP || i >= map->n) { sv_free(key); sv_free(val); return; }
    clear(&map->items[i]); clear(&map->items[map->n + i]);
    map->items[i] = *key; map->items[map->n + i] = *val;
    free(key); free(val);
}

const sv *sv_at(const sv *list, size_t i) { return list && list->type == SV_LIST && i < list->n ? &list->items[i] : NULL; }
const sv *sv_get(const sv *map, const char *key)
{
    if (!map || map->type != SV_MAP) return NULL;
    for (size_t i = 0; i < map->n; i++) if (map->items[i].type == SV_STRING && !strcmp(map->items[i].s, key)) return &map->items[map->n + i];
    return NULL;
}
const char *sv_str(const sv *v) { return v && v->type == SV_STRING ? v->s : NULL; }

/* ---------------------------------------------------------------- decode */

typedef struct { const uint8_t *d; size_t len, pos; bool bad; } rd;
static uint8_t r8(rd *r) { if (r->pos >= r->len) { r->bad = true; return 0; } return r->d[r->pos++]; }
static void rn(rd *r, void *out, size_t n) { if (r->pos + n > r->len) { r->bad = true; memset(out, 0, n); return; } memcpy(out, r->d + r->pos, n); r->pos += n; }
static size_t rsize(rd *r)
{
    uint8_t b = r8(r);
    if (b < 254) return b;
    if (b == 254) { uint16_t v; rn(r, &v, 2); return v; }
    uint32_t v; rn(r, &v, 4); return v;
}
static void ralign(rd *r, size_t a) { size_t m = r->pos % a; if (m) r->pos += a - m; if (r->pos > r->len) r->bad = true; }

static void decode_into(rd *r, sv *v, int depth)
{
    memset(v, 0, sizeof(*v));
    if (depth > 64) { r->bad = true; return; }
    uint8_t t = r8(r);
    switch (t) {
    case W_NULL: v->type = SV_NULL; break;
    case W_TRUE: v->type = SV_BOOL; v->i = 1; break;
    case W_FALSE: v->type = SV_BOOL; v->i = 0; break;
    case W_INT32: { int32_t x; rn(r, &x, 4); v->type = SV_INT; v->i = x; break; }
    case W_INT64: { int64_t x; rn(r, &x, 8); v->type = SV_INT; v->i = x; break; }
    case W_FLOAT64: { ralign(r, 8); double x; rn(r, &x, 8); v->type = SV_DOUBLE; v->d = x; break; }
    case W_LARGEINT: case W_STRING: {
        size_t n = rsize(r);
        if (r->pos + n > r->len) { r->bad = true; return; }
        v->type = SV_STRING; v->s = malloc(n + 1); memcpy(v->s, r->d + r->pos, n); v->s[n] = 0; r->pos += n;
        break;
    }
    case W_UINT8: case W_INT32LIST: case W_INT64LIST: case W_FLOAT64LIST: case W_FLOAT32LIST: {
        size_t n = rsize(r), es = t == W_UINT8 ? 1 : (t == W_INT32LIST || t == W_FLOAT32LIST) ? 4 : 8;
        if (es > 1) ralign(r, es);
        if (n > r->len || r->pos + n * es > r->len) { r->bad = true; return; }
        v->type = SV_BYTES; v->subtype = t; v->nbytes = n * es;
        v->bytes = malloc(v->nbytes ? v->nbytes : 1); memcpy(v->bytes, r->d + r->pos, v->nbytes); r->pos += v->nbytes;
        break;
    }
    case W_LIST: {
        size_t n = rsize(r);
        if (n > r->len) { r->bad = true; return; }
        v->type = SV_LIST; v->n = n; v->items = calloc(n ? n : 1, sizeof(sv));
        for (size_t i = 0; i < n && !r->bad; i++) decode_into(r, &v->items[i], depth + 1);
        break;
    }
    case W_MAP: {
        size_t n = rsize(r);
        if (n > r->len) { r->bad = true; return; }
        v->type = SV_MAP; v->n = n; v->items = calloc(n ? 2 * n : 1, sizeof(sv));
        for (size_t i = 0; i < n && !r->bad; i++) { decode_into(r, &v->items[i], depth + 1); decode_into(r, &v->items[n + i], depth + 1); }
        break;
    }
    default:
        /* a type of a plugin's own codec (Pigeon classes are 129 and up): what follows is usually a list of the fields */
        v->type = SV_OTHER; v->subtype = t;
        if (t >= 128) { sv inner; decode_into(r, &inner, depth + 1); v->items = calloc(1, sizeof(sv)); v->items[0] = inner; v->n = 1; }
        else r->bad = true;
        break;
    }
}

sv *sv_decode(const uint8_t *d, size_t len, size_t *used)
{
    rd r = { d, len, 0, false };
    sv *v = calloc(1, sizeof(*v));
    decode_into(&r, v, 0);
    if (r.bad) { sv_free(v); return NULL; }
    if (used) *used = r.pos;
    return v;
}
sv *sv_decode_message(const uint8_t *d, size_t len) { return len ? sv_decode(d, len, NULL) : sv_null(); }

/* ---------------------------------------------------------------- encode */

typedef struct { uint8_t **b; size_t *n, *cap; } wr;
static void wn(wr *w, const void *d, size_t n)
{
    if (*w->n + n > *w->cap) { *w->cap = (*w->n + n) * 2 + 64; *w->b = realloc(*w->b, *w->cap); }
    memcpy(*w->b + *w->n, d, n); *w->n += n;
}
static void w8(wr *w, uint8_t v) { wn(w, &v, 1); }
static void wsize(wr *w, size_t n)
{
    if (n < 254) w8(w, (uint8_t)n);
    else if (n <= 0xffff) { w8(w, 254); uint16_t v = (uint16_t)n; wn(w, &v, 2); }
    else { w8(w, 255); uint32_t v = (uint32_t)n; wn(w, &v, 4); }
}
static void walign(wr *w, size_t a) { while (*w->n % a) w8(w, 0); }

static void encode_v(wr *w, const sv *v)
{
    switch (v->type) {
    case SV_NULL: w8(w, W_NULL); break;
    case SV_BOOL: w8(w, v->i ? W_TRUE : W_FALSE); break;
    case SV_INT:
        if (v->i >= INT32_MIN && v->i <= INT32_MAX) { w8(w, W_INT32); int32_t x = (int32_t)v->i; wn(w, &x, 4); }
        else { w8(w, W_INT64); wn(w, &v->i, 8); }
        break;
    case SV_DOUBLE: w8(w, W_FLOAT64); walign(w, 8); wn(w, &v->d, 8); break;
    case SV_STRING: { w8(w, W_STRING); size_t n = strlen(v->s); wsize(w, n); wn(w, v->s, n); break; }
    case SV_BYTES: {
        uint8_t t = v->subtype ? v->subtype : W_UINT8;
        size_t es = t == W_UINT8 ? 1 : (t == W_INT32LIST || t == W_FLOAT32LIST) ? 4 : 8;
        w8(w, t); wsize(w, v->nbytes / es); if (es > 1) walign(w, es); wn(w, v->bytes, v->nbytes);
        break;
    }
    case SV_LIST: w8(w, W_LIST); wsize(w, v->n); for (size_t i = 0; i < v->n; i++) encode_v(w, &v->items[i]); break;
    case SV_MAP: w8(w, W_MAP); wsize(w, v->n); for (size_t i = 0; i < v->n; i++) { encode_v(w, &v->items[i]); encode_v(w, &v->items[v->n + i]); } break;
    case SV_OTHER: w8(w, v->subtype); if (v->n) encode_v(w, &v->items[0]); break;
    }
}

void sv_encode(const sv *v, uint8_t **out, size_t *len, size_t *cap) { wr w = { out, len, cap }; encode_v(&w, v); }

uint8_t *sv_encode_message(const sv *v, size_t *len)
{
    uint8_t *b = NULL; size_t n = 0, cap = 0;
    sv_encode(v, &b, &n, &cap);
    *len = n;
    return b;
}
uint8_t *sv_encode_success(const sv *result, size_t *len)
{
    uint8_t *b = NULL; size_t n = 0, cap = 0;
    wr w = { &b, &n, &cap };
    w8(&w, 0);
    if (result) encode_v(&w, result); else w8(&w, W_NULL);
    *len = n;
    return b;
}
uint8_t *sv_encode_error(const char *code, const char *msg, size_t *len)
{
    uint8_t *b = NULL; size_t n = 0, cap = 0;
    wr w = { &b, &n, &cap };
    w8(&w, 1);
    sv *c = sv_string(code), *m = sv_string(msg), *d = sv_null();
    encode_v(&w, c); encode_v(&w, m); encode_v(&w, d);
    sv_free(c); sv_free(m); sv_free(d);
    *len = n;
    return b;
}
