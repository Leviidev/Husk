/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Flutter's StandardMessageCodec: the binary encoding of null, booleans, numbers, strings, typed arrays, lists and maps that
 * method channels and Pigeon-generated plugin APIs send between Dart and the platform.
 */
#ifndef HUSK_TL_FLUTTER_CODEC_H
#define HUSK_TL_FLUTTER_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { SV_NULL, SV_BOOL, SV_INT, SV_DOUBLE, SV_STRING, SV_BYTES, SV_LIST, SV_MAP, SV_OTHER };

typedef struct sv {
    int type;
    int64_t i;                      /* bool, int */
    double d;
    char *s;                        /* string (UTF-8, NUL-terminated) */
    uint8_t *bytes; size_t nbytes;  /* Uint8List and the other typed lists, raw */
    uint8_t subtype;                /* the wire type of SV_BYTES / SV_OTHER */
    struct sv *items; size_t n;     /* list: n items; map: n keys then n values (items[0..n) keys, items[n..2n) values) */
} sv;

sv *sv_null(void);
sv *sv_bool(bool b);
sv *sv_int(int64_t i);
sv *sv_double(double d);
sv *sv_string(const char *s);
sv *sv_list(size_t n);              /* n null items, to be filled with sv_set */
sv *sv_map(size_t n);               /* n null keys and values */
void sv_set(sv *list, size_t i, sv *v);              /* takes ownership */
void sv_map_set(sv *map, size_t i, sv *key, sv *val);
void sv_free(sv *v);
sv *sv_copy(const sv *v);

/* Lookups, NULL when absent or of another type. */
const sv *sv_at(const sv *list, size_t i);
const sv *sv_get(const sv *map, const char *key);
const char *sv_str(const sv *v);

/* Wire format. sv_decode returns NULL on malformed input; *used is how many bytes the value took. */
sv *sv_decode(const uint8_t *d, size_t len, size_t *used);
void sv_encode(const sv *v, uint8_t **out, size_t *len, size_t *cap);

/* A whole message of one value, and the method-call / envelope shapes on top of it. */
sv *sv_decode_message(const uint8_t *d, size_t len);
uint8_t *sv_encode_message(const sv *v, size_t *len);                 /* malloc'd */
uint8_t *sv_encode_success(const sv *result, size_t *len);           /* method channels: 0, value */
uint8_t *sv_encode_error(const char *code, const char *msg, size_t *len);

#endif /* HUSK_TL_FLUTTER_CODEC_H */
