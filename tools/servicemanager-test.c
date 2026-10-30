/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Native addService / getService against in-process ServiceManager. */
#include "husk-tl-binder.h"
#include "husk-tl-servicemanager.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void tl_log_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void ping_svc(uint32_t code, uint32_t flags, const uint8_t *data, size_t data_size,
                     const uint64_t *offsets, size_t n_offsets, husk_binder_reply *reply, void *cookie)
{
    (void)code; (void)flags; (void)data; (void)offsets; (void)n_offsets; (void)cookie;
    reply->data = malloc(8);
    if (reply->data && data_size >= 4 && !memcmp(data, "ping", 4)) memcpy(reply->data, "pong", 5);
    else if (reply->data) memcpy(reply->data, "????", 5);
    reply->data_size = 5;
}

static void w_i32(uint8_t *b, size_t *n, int32_t v) { memcpy(b + *n, &v, 4); *n += 4; }
static void w_str16(uint8_t *b, size_t *n, const char *s)
{
    int32_t len = (int32_t)strlen(s);
    w_i32(b, n, len);
    for (int32_t i = 0; i <= len; i++) {
        uint16_t c = (uint16_t)(unsigned char)s[i];
        memcpy(b + *n, &c, 2);
        *n += 2;
    }
    while (*n % 4) b[(*n)++] = 0;
}

static size_t token(uint8_t *b)
{
    size_t n = 0;
    w_i32(b, &n, 0x01000000);
    w_i32(b, &n, -1);
    w_str16(b, &n, "android.os.IServiceManager");
    return n;
}

int main(void)
{
    husk_sm_start();
    uint32_t ping = husk_binder_add_local(ping_svc, NULL);
    if (!ping) { fprintf(stderr, "no handle\n"); return 1; }
    if (!husk_sm_add("ping", ping)) { fprintf(stderr, "add failed\n"); return 1; }
    if (husk_sm_get("ping") != ping) { fprintf(stderr, "table lookup failed\n"); return 1; }

    uint8_t data[512];
    size_t n = token(data);
    w_str16(data, &n, "ping");
    husk_binder_reply r = {0};
    if (husk_binder_transact(0, 1 /* GET */, 0, data, n, NULL, 0, &r) != 0) {
        fprintf(stderr, "GET transact failed\n");
        return 1;
    }
    if (r.data_size < 12) { fprintf(stderr, "short GET reply %zu\n", r.data_size); return 1; }
    uint32_t got = 0;
    memcpy(&got, r.data + 12, 4);
    husk_binder_reply_free(&r);
    if (got != ping) { fprintf(stderr, "GET handle %u want %u\n", got, ping); return 1; }

    uint8_t pingd[4] = { 'p', 'i', 'n', 'g' };
    if (husk_binder_transact(got, 1, 0, pingd, 4, NULL, 0, &r) != 0) {
        fprintf(stderr, "ping transact failed\n");
        return 1;
    }
    int ok = r.data_size >= 4 && !memcmp(r.data, "pong", 4);
    husk_binder_reply_free(&r);
    if (!ok) { fprintf(stderr, "no pong\n"); return 1; }

    char list[1024];
    if (husk_sm_list(list, sizeof(list)) < 1 || !strstr(list, "ping")) {
        fprintf(stderr, "list: %s\n", list);
        return 1;
    }
    printf("sm: add/get ping ok\n");
    return 0;
}
