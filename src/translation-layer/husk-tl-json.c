/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char kOutOfMemory[] = "{\"ok\":false,\"error\":\"out of memory\"}";

void tl_json_init(tl_json *j)
{
    memset(j, 0, sizeof(*j));
}

static void put(tl_json *j, const char *s, size_t n)
{
    if (j->failed) {
        return;
    }
    if (j->len + n + 1 > j->cap) {
        size_t cap = j->cap ? j->cap : 1024;
        while (cap < j->len + n + 1) {
            cap *= 2;
        }
        char *grown = realloc(j->buf, cap);
        if (!grown) {
            j->failed = true;
            return;
        }
        j->buf = grown;
        j->cap = cap;
    }
    memcpy(j->buf + j->len, s, n);
    j->len += n;
    j->buf[j->len] = '\0';
}

static void puts_(tl_json *j, const char *s)
{
    put(j, s, strlen(s));
}

/* Comma before a value, unless it follows a key or opens its container. */
static void before_value(tl_json *j)
{
    if (j->after_key) {
        j->after_key = false;
        return;
    }
    if (j->depth > 0 && j->need_comma[j->depth]) {
        put(j, ",", 1);
    }
    if (j->depth > 0) {
        j->need_comma[j->depth] = true;
    }
}

static void open_(tl_json *j, char c)
{
    before_value(j);
    put(j, &c, 1);
    if (j->depth + 1 >= (int)(sizeof(j->need_comma) / sizeof(j->need_comma[0]))) {
        j->failed = true;
        return;
    }
    j->depth++;
    j->need_comma[j->depth] = false;
}

static void close_(tl_json *j, char c)
{
    put(j, &c, 1);
    if (j->depth > 0) {
        j->depth--;
    }
}

void tl_json_begin_object(tl_json *j) { open_(j, '{'); }
void tl_json_end_object(tl_json *j)   { close_(j, '}'); }
void tl_json_begin_array(tl_json *j)  { open_(j, '['); }
void tl_json_end_array(tl_json *j)    { close_(j, ']'); }

/* Length of the valid UTF-8 sequence at `s`, or 0 if it is not one. */
static size_t utf8_len(const unsigned char *s)
{
    unsigned char c = s[0];
    size_t n;
    unsigned min;
    unsigned cp;
    if (c < 0x80) {
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        n = 2; cp = c & 0x1F; min = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        n = 3; cp = c & 0x0F; min = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        n = 4; cp = c & 0x07; min = 0x10000;
    } else {
        return 0;
    }
    for (size_t i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            return 0;
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return 0;
    }
    return n;
}

/*
 * Escaped and quoted. Names come out of files someone else built, and Swift's
 * decoder rejects the whole document over one invalid UTF-8 byte, so anything
 * that is not valid UTF-8 becomes '?'.
 */
static void quoted(tl_json *j, const char *s)
{
    put(j, "\"", 1);
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            char esc[2] = { '\\', (char)c };
            put(j, esc, 2);
            p++;
        } else if (c < 0x20) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\u%04x", c);
            puts_(j, esc);
            p++;
        } else {
            size_t n = utf8_len(p);
            if (n == 0) {
                put(j, "?", 1);
                p++;
            } else {
                put(j, (const char *)p, n);
                p += n;
            }
        }
    }
    put(j, "\"", 1);
}

void tl_json_key(tl_json *j, const char *key)
{
    if (j->depth > 0 && j->need_comma[j->depth]) {
        put(j, ",", 1);
    }
    if (j->depth > 0) {
        j->need_comma[j->depth] = true;
    }
    quoted(j, key);
    put(j, ":", 1);
    j->after_key = true;
}

void tl_json_string(tl_json *j, const char *s)
{
    before_value(j);
    if (s) {
        quoted(j, s);
    } else {
        puts_(j, "null");
    }
}

void tl_json_int(tl_json *j, long long v)
{
    char num[32];
    before_value(j);
    snprintf(num, sizeof(num), "%lld", v);
    puts_(j, num);
}

void tl_json_bool(tl_json *j, bool v)
{
    before_value(j);
    puts_(j, v ? "true" : "false");
}

char *tl_json_finish(tl_json *j)
{
    if (j->failed || !j->buf) {
        free(j->buf);
        j->buf = NULL;
        char *fallback = malloc(sizeof(kOutOfMemory));
        if (fallback) {
            memcpy(fallback, kOutOfMemory, sizeof(kOutOfMemory));
        }
        return fallback;
    }
    char *out = j->buf;
    j->buf = NULL;
    return out;
}
