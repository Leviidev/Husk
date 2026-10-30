/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Two threads exchange one Binder transaction in-process (Husk 3.3 /dev/binder). */
#include "husk-tl-binder.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

void tl_log_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

#define BINDER_WRITE_READ      0xC0306201UL
#define BINDER_SET_CONTEXT_MGR 0x40046207UL
#define BINDER_VERSION         0xC0046209UL
#define BC_TRANSACTION         0x40406300U
#define BC_REPLY               0x40406301U
#define BC_ENTER_LOOPER        0x00006312U
#define BR_TRANSACTION         0x80407202U
#define BR_REPLY               0x80407203U
#define BR_TRANSACTION_COMPLETE 0x00007206U

typedef struct {
    uint64_t write_size, write_consumed, write_buffer;
    uint64_t read_size, read_consumed, read_buffer;
} wr_t;

typedef struct {
    uint64_t handle, cookie;
    uint32_t code, flags;
    int32_t sender_pid;
    uint32_t sender_euid;
    uint64_t data_size, offsets_size, buffer, offsets;
} txn_t;

static int wr_ioctl(int fd, const void *w, size_t wn, void *r, size_t rn, size_t *got)
{
    uint8_t rbuf[4096];
    wr_t wr = { .write_size = wn, .write_buffer = (uint64_t)(uintptr_t)w,
                .read_size = rn ? rn : sizeof(rbuf), .read_buffer = (uint64_t)(uintptr_t)(r ? r : rbuf) };
    if (husk_binder_ioctl(fd, BINDER_WRITE_READ, &wr) != 0) return -1;
    if (got) *got = (size_t)wr.read_consumed;
    if (r && wr.read_consumed) memcpy(r, (void *)(uintptr_t)wr.read_buffer, (size_t)wr.read_consumed);
    return 0;
}

static int g_srv, g_ok;

static void *server(void *arg)
{
    (void)arg;
    int32_t dummy = 0;
    if (husk_binder_ioctl(g_srv, BINDER_SET_CONTEXT_MGR, &dummy) != 0) return NULL;
    husk_binder_mmap(g_srv, 1 << 16);
    uint32_t enter = BC_ENTER_LOOPER;
    uint8_t in[512];
    size_t n = 0;
    if (wr_ioctl(g_srv, &enter, 4, in, sizeof(in), &n) != 0) return NULL;
    size_t i = 0;
    while (i + 4 <= n) {
        uint32_t cmd;
        memcpy(&cmd, in + i, 4);
        i += 4;
        if (cmd == BR_TRANSACTION_COMPLETE) continue;
        if (cmd == BR_TRANSACTION && i + sizeof(txn_t) <= n) {
            txn_t t;
            memcpy(&t, in + i, sizeof(t));
            const char *msg = (const char *)(uintptr_t)t.buffer;
            if (t.data_size < 5 || memcmp(msg, "ping", 4) != 0) return NULL;
            char pong[] = "pong";
            txn_t reply = t;
            reply.handle = 0;
            reply.data_size = 5;
            reply.offsets_size = 0;
            reply.buffer = (uint64_t)(uintptr_t)pong;
            reply.offsets = 0;
            uint8_t w[4 + sizeof(txn_t)];
            uint32_t bc = BC_REPLY;
            memcpy(w, &bc, 4);
            memcpy(w + 4, &reply, sizeof(reply));
            wr_ioctl(g_srv, w, sizeof(w), NULL, 0, NULL);
            g_ok = 1;
            return NULL;
        }
        if (cmd == BR_TRANSACTION) i += sizeof(txn_t);
    }
    return NULL;
}

int main(void)
{
    g_srv = husk_binder_open();
    int cli = husk_binder_open();
    if (g_srv < 0 || cli < 0) { fprintf(stderr, "open failed\n"); return 1; }
    int32_t ver = 0;
    if (husk_binder_ioctl(cli, BINDER_VERSION, &ver) != 0 || ver != 8) {
        fprintf(stderr, "BINDER_VERSION %d\n", ver);
        return 1;
    }
    husk_binder_mmap(cli, 1 << 16);
    pthread_t th;
    pthread_create(&th, NULL, server, NULL);
    usleep(50000);
    char ping[] = "ping";
    txn_t t = { .handle = 0, .code = 1, .data_size = 5, .buffer = (uint64_t)(uintptr_t)ping };
    uint8_t w[4 + sizeof(txn_t)], in[512];
    uint32_t bc = BC_TRANSACTION;
    memcpy(w, &bc, 4);
    memcpy(w + 4, &t, sizeof(t));
    size_t n = 0;
    if (wr_ioctl(cli, w, sizeof(w), in, sizeof(in), &n) != 0) { fprintf(stderr, "client write failed\n"); return 1; }
    int saw_reply = 0;
    for (int round = 0; round < 4 && !saw_reply; round++) {
        size_t i = 0;
        while (i + 4 <= n) {
            uint32_t cmd;
            memcpy(&cmd, in + i, 4);
            i += 4;
            if (cmd == BR_TRANSACTION_COMPLETE) continue;
            if (cmd == BR_REPLY && i + sizeof(txn_t) <= n) {
                txn_t r;
                memcpy(&r, in + i, sizeof(r));
                if (r.data_size >= 5 && !memcmp((void *)(uintptr_t)r.buffer, "pong", 4)) saw_reply = 1;
                break;
            }
            if (cmd == BR_REPLY || cmd == BR_TRANSACTION) i += sizeof(txn_t);
        }
        if (!saw_reply) wr_ioctl(cli, NULL, 0, in, sizeof(in), &n);
    }
    pthread_join(th, NULL);
    husk_binder_close(cli);
    husk_binder_close(g_srv);
    close(cli);
    close(g_srv);
    if (!saw_reply || !g_ok) { fprintf(stderr, "no ping/pong\n"); return 1; }
    printf("binder: ping/pong ok\n");
    return 0;
}
