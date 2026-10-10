/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Text input for Flutter apps: the flutter/textinput channel, as Android's TextInputPlugin answers it.
 *
 * Dart owns the text field; the embedding owns the keyboard and an editing state (the text, the selection, the composing range)
 * that both sides keep in step. Dart says which field is focused (setClient), shows and hides the keyboard, and pushes its state
 * when the field changes it (setEditingState). The keyboard's edits are applied here and sent back as updateEditingState; Return
 * is the field's action (performAction). Offsets are UTF-16 code units, as in Java and Dart, so the text is kept as UTF-16.
 *
 * Everything here runs on the platform thread, except the keyboard's edits, which arrive on the main thread and are queued.
 */
#include "husk-tl-flutter-text.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    int client;                 /* the focused field's id; 0 when there is none */
    uint16_t *text; size_t len, cap;
    long sel_base, sel_ext;
    char action[48];            /* the field's inputAction, e.g. TextInputAction.done */
    int kind;                   /* tl_flutter_kb_kind */
    bool multiline, obscure;
} T;

static void (*g_keyboard)(int show, int kind);

void tl_flutter_text_set_keyboard_handler(void (*fn)(int show, int kind)) { g_keyboard = fn; }

/* ------------------------------------------------------------------ a little JSON */

/* The value after "key": somewhere after `from`, or NULL. Good enough for the flat objects this channel carries. */
static const char *json_find(const char *from, const char *end, const char *key)
{
    char pat[64];
    int n = snprintf(pat, sizeof(pat), "\"%s\"", key);
    for (const char *p = from; p && p + n < end; p++) {
        p = memmem(p, (size_t)(end - p), pat, (size_t)n);
        if (!p) return NULL;
        const char *q = p + n;
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
        if (q < end && *q == ':') {
            q++;
            while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
            return q;
        }
    }
    return NULL;
}

static long json_long(const char *v, const char *end, long dflt)
{
    if (!v || v >= end) return dflt;
    if (*v == '-' || (*v >= '0' && *v <= '9')) return strtol(v, NULL, 10);
    return dflt;
}

static bool json_bool(const char *v, const char *end)
{
    return v && end - v >= 4 && !memcmp(v, "true", 4);
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void push16(uint16_t **buf, size_t *len, size_t *cap, uint16_t u)
{
    if (*len == *cap) { *cap = *cap ? *cap * 2 : 64; *buf = realloc(*buf, *cap * sizeof(uint16_t)); }
    (*buf)[(*len)++] = u;
}

/* Append UTF-8 bytes to a UTF-16 buffer. */
static void utf8_to_16(const unsigned char *s, size_t n, uint16_t **buf, size_t *len, size_t *cap)
{
    for (size_t i = 0; i < n; ) {
        unsigned c = s[i];
        uint32_t cp; int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { i++; continue; }
        i++;
        for (int k = 0; k < extra && i < n; k++, i++) cp = (cp << 6) | (s[i] & 0x3F);
        if (cp >= 0x10000) {
            cp -= 0x10000;
            push16(buf, len, cap, (uint16_t)(0xD800 | (cp >> 10)));
            push16(buf, len, cap, (uint16_t)(0xDC00 | (cp & 0x3FF)));
        } else {
            push16(buf, len, cap, (uint16_t)cp);
        }
    }
}

/* A JSON string (at its opening quote) into UTF-16. */
static void json_string16(const char *v, const char *end, uint16_t **buf, size_t *len, size_t *cap)
{
    if (!v || v >= end || *v != '"') return;
    const char *p = v + 1;
    while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) {
            char e = p[1];
            p += 2;
            switch (e) {
            case 'n': push16(buf, len, cap, '\n'); break;
            case 't': push16(buf, len, cap, '\t'); break;
            case 'r': push16(buf, len, cap, '\r'); break;
            case 'b': push16(buf, len, cap, '\b'); break;
            case 'f': push16(buf, len, cap, '\f'); break;
            case 'u':
                if (p + 4 <= end) {
                    int h0 = hexval(p[0]), h1 = hexval(p[1]), h2 = hexval(p[2]), h3 = hexval(p[3]);
                    if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) push16(buf, len, cap, (uint16_t)(h0 << 12 | h1 << 8 | h2 << 4 | h3));
                    p += 4;
                }
                break;
            default: push16(buf, len, cap, (uint16_t)e); break;
            }
            continue;
        }
        const char *q = p;
        while (q < end && *q != '"' && *q != '\\') q++;
        utf8_to_16((const unsigned char *)p, (size_t)(q - p), buf, len, cap);
        p = q;
    }
}

/* A JSON string (at its opening quote) into a short C string. */
static void json_cstr(const char *v, const char *end, char *out, size_t n)
{
    out[0] = 0;
    if (!v || v >= end || *v != '"') return;
    const char *q = memchr(v + 1, '"', (size_t)(end - v - 1));
    if (!q) return;
    snprintf(out, n, "%.*s", (int)(q - v - 1), v + 1);
}

/* The editing state as JSON: the text escaped, then the offsets. */
static char *state_json(void)
{
    size_t cap = T.len * 6 + 256, n = 0;
    char *out = malloc(cap);
    n += (size_t)snprintf(out + n, cap - n, "{\"text\":\"");
    for (size_t i = 0; i < T.len; i++) {
        uint32_t cp = T.text[i];
        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < T.len && T.text[i + 1] >= 0xDC00 && T.text[i + 1] < 0xE000) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (T.text[i + 1] - 0xDC00);
            i++;
        }
        if (cp == '"' || cp == '\\') { out[n++] = '\\'; out[n++] = (char)cp; }
        else if (cp == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else if (cp < 0x20) n += (size_t)snprintf(out + n, cap - n, "\\u%04x", cp);
        else if (cp < 0x80) out[n++] = (char)cp;
        else if (cp < 0x800) { out[n++] = (char)(0xC0 | cp >> 6); out[n++] = (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out[n++] = (char)(0xE0 | cp >> 12); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
        else { out[n++] = (char)(0xF0 | cp >> 18); out[n++] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[n++] = (char)(0x80 | (cp & 0x3F)); }
    }
    snprintf(out + n, cap - n, "\",\"selectionBase\":%ld,\"selectionExtent\":%ld,\"selectionAffinity\":\"TextAffinity.downstream\","
             "\"selectionIsDirectional\":false,\"composingBase\":-1,\"composingExtent\":-1}", T.sel_base, T.sel_ext);
    return out;
}

/* ------------------------------------------------------------------ from Dart */

static int keyboard_kind(const char *name)
{
    if (strstr(name, "number") || strstr(name, "phone")) return TL_FLUTTER_KB_NUMBER;
    if (strstr(name, "emailAddress")) return TL_FLUTTER_KB_EMAIL;
    if (strstr(name, "url")) return TL_FLUTTER_KB_URL;
    return TL_FLUTTER_KB_TEXT;
}

const char *tl_flutter_text_message(const char *method, const uint8_t *data, size_t len)
{
    const char *d = (const char *)data, *end = d + len;
    if (!strcmp(method, "TextInput.setClient")) {
        /* args: [id, configuration] */
        const char *args = json_find(d, end, "args");
        if (args && *args == '[') T.client = (int)strtol(args + 1, NULL, 10);
        json_cstr(json_find(d, end, "inputAction"), end, T.action, sizeof(T.action));
        char type[64] = "";
        json_cstr(json_find(d, end, "name"), end, type, sizeof(type));
        T.kind = keyboard_kind(type);
        T.multiline = strstr(type, "multiline") != NULL;
        T.obscure = json_bool(json_find(d, end, "obscureText"), end);
        return "[null]";
    }
    if (!strcmp(method, "TextInput.clearClient")) {
        T.client = 0;
        if (g_keyboard) g_keyboard(0, 0);
        return "[null]";
    }
    if (!strcmp(method, "TextInput.show")) {
        if (g_keyboard) g_keyboard(1, T.kind | (T.multiline ? TL_FLUTTER_KB_MULTILINE : 0) | (T.obscure ? TL_FLUTTER_KB_SECURE : 0));
        return "[null]";
    }
    if (!strcmp(method, "TextInput.hide")) {
        if (g_keyboard) g_keyboard(0, 0);
        return "[null]";
    }
    if (!strcmp(method, "TextInput.setEditingState")) {
        T.len = 0;
        json_string16(json_find(d, end, "text"), end, &T.text, &T.len, &T.cap);
        T.sel_base = json_long(json_find(d, end, "selectionBase"), end, (long)T.len);
        T.sel_ext = json_long(json_find(d, end, "selectionExtent"), end, T.sel_base);
        return "[null]";
    }
    return "[null]";       /* sizes, styles, marked-text rects, autofill: nothing to do */
}

/* ------------------------------------------------------------------ from the keyboard */

enum { OP_INSERT, OP_DELETE, OP_ACTION };
typedef struct op { int kind; char *text; struct op *next; } op;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static op *g_head, *g_tail;

static void enqueue(int kind, const char *text)
{
    op *o = calloc(1, sizeof(*o));
    o->kind = kind;
    o->text = text ? strdup(text) : NULL;
    pthread_mutex_lock(&g_lock);
    if (g_tail) g_tail->next = o; else g_head = o;
    g_tail = o;
    pthread_mutex_unlock(&g_lock);
}

void tl_flutter_text_insert(const char *utf8) { if (utf8) enqueue(OP_INSERT, utf8); }
void tl_flutter_text_delete(void) { enqueue(OP_DELETE, NULL); }
void tl_flutter_text_action(void) { enqueue(OP_ACTION, NULL); }
bool tl_flutter_text_multiline(void) { return T.multiline; }

static void replace_selection(const uint16_t *with, size_t n)
{
    long a = T.sel_base < T.sel_ext ? T.sel_base : T.sel_ext;
    long b = T.sel_base < T.sel_ext ? T.sel_ext : T.sel_base;
    if (a < 0 || a > (long)T.len) a = (long)T.len;
    if (b < a) b = a;
    if (b > (long)T.len) b = (long)T.len;
    size_t tail = T.len - (size_t)b;
    size_t need = (size_t)a + n + tail;
    if (need > T.cap) { T.cap = need + 64; T.text = realloc(T.text, T.cap * sizeof(uint16_t)); }
    memmove(T.text + a + n, T.text + b, tail * sizeof(uint16_t));
    if (n) memcpy(T.text + a, with, n * sizeof(uint16_t));
    T.len = need;
    T.sel_base = T.sel_ext = a + (long)n;
}

bool tl_flutter_text_drain(void (*send)(const char *channel, const char *json))
{
    pthread_mutex_lock(&g_lock);
    op *list = g_head;
    g_head = g_tail = NULL;
    pthread_mutex_unlock(&g_lock);
    if (!list) return false;
    bool changed = false;
    for (op *o = list, *next; o; o = next) {
        next = o->next;
        if (T.client) {
            if (o->kind == OP_INSERT) {
                uint16_t *u = NULL; size_t n = 0, cap = 0;
                utf8_to_16((const unsigned char *)o->text, strlen(o->text), &u, &n, &cap);
                replace_selection(u, n);
                free(u);
                changed = true;
            } else if (o->kind == OP_DELETE) {
                if (T.sel_base != T.sel_ext) {
                    replace_selection(NULL, 0);
                } else if (T.sel_base > 0 && T.sel_base <= (long)T.len) {
                    /* one character back: both halves of a surrogate pair */
                    long from = T.sel_base - 1;
                    if (from > 0 && T.text[from] >= 0xDC00 && T.text[from] < 0xE000 && T.text[from - 1] >= 0xD800 && T.text[from - 1] < 0xDC00) from--;
                    T.sel_base = from;
                    replace_selection(NULL, 0);
                }
                changed = true;
            } else if (o->kind == OP_ACTION) {
                if (changed) {
                    char *st = state_json();
                    char *msg = malloc(strlen(st) + 96);
                    sprintf(msg, "{\"method\":\"TextInputClient.updateEditingState\",\"args\":[%d,%s]}", T.client, st);
                    send("flutter/textinput", msg);
                    free(msg); free(st);
                    changed = false;
                }
                char msg[160];
                snprintf(msg, sizeof(msg), "{\"method\":\"TextInputClient.performAction\",\"args\":[%d,\"%s\"]}",
                         T.client, T.action[0] ? T.action : "TextInputAction.done");
                send("flutter/textinput", msg);
            }
        }
        free(o->text);
        free(o);
    }
    if (changed && T.client) {
        char *st = state_json();
        char *msg = malloc(strlen(st) + 96);
        sprintf(msg, "{\"method\":\"TextInputClient.updateEditingState\",\"args\":[%d,%s]}", T.client, st);
        send("flutter/textinput", msg);
        free(msg); free(st);
    }
    return true;
}
