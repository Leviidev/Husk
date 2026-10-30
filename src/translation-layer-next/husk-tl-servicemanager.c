/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-servicemanager.h"
#include "husk-tl-binder.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tl_log_line(const char *fmt, ...);

#define SM_GET    1
#define SM_CHECK  2
#define SM_ADD    3
#define SM_LIST   4

#define BINDER_TYPE_HANDLE 0x73682a85U

enum { MAX_SVC = 128 };

typedef struct {
    char name[128];
    uint32_t handle;
    bool used;
} svc;

static svc g_svc[MAX_SVC];
static pthread_mutex_t g_lk = PTHREAD_MUTEX_INITIALIZER;
static bool g_on;

static int32_t rd_i32(const uint8_t *d, size_t n, size_t *p)
{
    if (*p + 4 > n) return 0;
    int32_t v;
    memcpy(&v, d + *p, 4);
    *p += 4;
    return v;
}

static void skip_token(const uint8_t *d, size_t n, size_t *p)
{
    rd_i32(d, n, p); /* strict mode */
    if (*p + 4 <= n) {
        int32_t x;
        memcpy(&x, d + *p, 4);
        if (x < 0 || x > 512) *p += 4; /* work source uid */
    }
    int32_t len = rd_i32(d, n, p);
    if (len < 0) return;
    size_t bytes = ((size_t)(len + 1) * 2 + 3) & ~(size_t)3;
    if (*p + bytes <= n) *p += bytes;
}

static bool rd_str16(const uint8_t *d, size_t n, size_t *p, char *out, size_t cap)
{
    int32_t len = rd_i32(d, n, p);
    if (len < 0) { if (cap) out[0] = 0; return false; }
    if (*p + (size_t)len * 2 > n) { if (cap) out[0] = 0; return false; }
    size_t i;
    for (i = 0; i < (size_t)len && i + 1 < cap; i++) {
        uint16_t c;
        memcpy(&c, d + *p + i * 2, 2);
        out[i] = (char)(c < 128 ? c : '?');
    }
    if (cap) out[i] = 0;
    *p += ((size_t)(len + 1) * 2 + 3) & ~(size_t)3;
    return true;
}

static uint32_t rd_handle(const uint8_t *d, size_t n, const uint64_t *offs, size_t n_off)
{
    if (!offs || !n_off) return 0;
    size_t at = (size_t)offs[0];
    if (at + 24 > n) return 0;
    uint32_t type, handle = 0;
    memcpy(&type, d + at, 4);
    memcpy(&handle, d + at + 8, 4);
    if (type == BINDER_TYPE_HANDLE) return handle;
    return handle;
}

static void wr_i32(uint8_t **buf, size_t *len, size_t *cap, int32_t v)
{
    if (*len + 4 > *cap) {
        size_t nc = *cap ? *cap * 2 : 64;
        while (nc < *len + 4) nc *= 2;
        uint8_t *q = realloc(*buf, nc);
        if (!q) return;
        *buf = q;
        *cap = nc;
    }
    memcpy(*buf + *len, &v, 4);
    *len += 4;
}

static void wr_handle(husk_binder_reply *r, uint32_t handle)
{
    size_t cap = 64, len = r->data_size;
    uint8_t *buf = r->data;
    if (!buf) { buf = malloc(cap); cap = 64; len = 0; }
    wr_i32(&buf, &len, &cap, 0); /* no exception */
    if (!handle) {
        /* null binder: BINDER_TYPE_BINDER with a zero pointer */
        uint32_t type = 0x73622a85U, flags = 0;
        uint64_t z = 0;
        size_t at = len, need = at + 24;
        if (need > cap) {
            cap = need + 32;
            uint8_t *q = realloc(buf, cap);
            if (!q) { r->data = buf; r->data_size = len; return; }
            buf = q;
        }
        memcpy(buf + at, &type, 4);
        memcpy(buf + at + 4, &flags, 4);
        memcpy(buf + at + 8, &z, 8);
        memcpy(buf + at + 16, &z, 8);
        r->data = buf;
        r->data_size = at + 24;
        r->offsets = malloc(sizeof(uint64_t));
        if (r->offsets) { r->offsets[0] = at; r->n_offsets = 1; }
        return;
    }
    size_t at = len;
    /* flat_binder_object: type, flags, handle, pad, cookie */
    uint32_t type = BINDER_TYPE_HANDLE, flags = 0, pad = 0;
    uint64_t cookie = 0;
    size_t need = at + 24;
    if (need > cap) {
        cap = need + 32;
        uint8_t *q = realloc(buf, cap);
        if (!q) { r->data = buf; r->data_size = len; return; }
        buf = q;
    }
    memcpy(buf + at, &type, 4);
    memcpy(buf + at + 4, &flags, 4);
    memcpy(buf + at + 8, &handle, 4);
    memcpy(buf + at + 12, &pad, 4);
    memcpy(buf + at + 16, &cookie, 8);
    len = at + 24;
    r->data = buf;
    r->data_size = len;
    r->offsets = malloc(sizeof(uint64_t));
    if (r->offsets) {
        r->offsets[0] = at;
        r->n_offsets = 1;
    }
}

static void wr_ok(husk_binder_reply *r)
{
    size_t cap = 8, len = 0;
    uint8_t *buf = malloc(cap);
    wr_i32(&buf, &len, &cap, 0);
    r->data = buf;
    r->data_size = len;
}

bool husk_sm_add(const char *name, uint32_t handle)
{
    if (!name || !name[0] || !handle) return false;
    pthread_mutex_lock(&g_lk);
    svc *slot = NULL;
    for (int i = 0; i < MAX_SVC; i++) {
        if (g_svc[i].used && !strcmp(g_svc[i].name, name)) { g_svc[i].handle = handle; pthread_mutex_unlock(&g_lk); return true; }
        if (!g_svc[i].used && !slot) slot = &g_svc[i];
    }
    if (!slot) { pthread_mutex_unlock(&g_lk); return false; }
    snprintf(slot->name, sizeof(slot->name), "%s", name);
    slot->handle = handle;
    slot->used = true;
    pthread_mutex_unlock(&g_lk);
    tl_log_line("sm: add %s -> handle %u", name, handle);
    return true;
}

uint32_t husk_sm_get(const char *name)
{
    if (!name) return 0;
    pthread_mutex_lock(&g_lk);
    uint32_t h = 0;
    for (int i = 0; i < MAX_SVC; i++) if (g_svc[i].used && !strcmp(g_svc[i].name, name)) { h = g_svc[i].handle; break; }
    pthread_mutex_unlock(&g_lk);
    return h;
}

int husk_sm_list(char *buf, int cap)
{
    if (!buf || cap <= 0) return 0;
    buf[0] = 0;
    int n = 0, used = 0;
    pthread_mutex_lock(&g_lk);
    for (int i = 0; i < MAX_SVC; i++) if (g_svc[i].used) {
        int w = snprintf(buf + used, (size_t)(cap - used), "%s%s", used ? "\n" : "", g_svc[i].name);
        if (w < 0 || used + w >= cap) break;
        used += w;
        n++;
    }
    pthread_mutex_unlock(&g_lk);
    return n;
}

static void sm_on_txn(uint32_t code, uint32_t flags, const uint8_t *data, size_t data_size,
                      const uint64_t *offsets, size_t n_offsets, husk_binder_reply *reply, void *cookie)
{
    (void)flags;
    (void)cookie;
    size_t p = 0;
    skip_token(data, data_size, &p);
    if (code == SM_ADD) {
        char name[128];
        rd_str16(data, data_size, &p, name, sizeof(name));
        uint32_t h = rd_handle(data, data_size, offsets, n_offsets);
        if (h) husk_sm_add(name, h);
        wr_ok(reply);
        return;
    }
    if (code == SM_GET || code == SM_CHECK) {
        char name[128];
        rd_str16(data, data_size, &p, name, sizeof(name));
        wr_handle(reply, husk_sm_get(name));
        return;
    }
    if (code == SM_LIST) {
        wr_ok(reply);
        return;
    }
    wr_ok(reply);
}

void husk_sm_start(void)
{
    if (g_on) return;
    g_on = true;
    husk_binder_set_local(0, sm_on_txn, NULL);
    tl_log_line("sm: context manager ready");
}
