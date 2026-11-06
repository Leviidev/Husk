/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Regression: an earlier fallback allocator kept a header in the 16 bytes BEFORE
 * each block and free() read it for every pointer. A pointer that starts a
 * mapping has an unmapped page in front of it, so free() crashed (SIGSEGV at
 * p-16) in every app. Ownership must come from the registry alone.
 */
#define _GNU_SOURCE
#include "husk-tl-fallback-alloc.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < 2000; i++) {
        size_t n = 1 + (size_t)(i % 97) * 513;
        unsigned char *p = tl_fb_alloc(n);
        assert(p);
        p[0] = 0x5a; p[n - 1] = 0xa5;
        assert(tl_fb_owns(p) && tl_fb_size(p) == n);
        assert(tl_fb_release(p));
    }
    return NULL;
}

int main(void)
{
    long pg = sysconf(_SC_PAGESIZE);
    assert(pg > 0);

    /* The crash: a page-aligned foreign pointer whose preceding page is unmapped. */
    unsigned char *two = mmap(NULL, 2 * (size_t)pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(two != MAP_FAILED);
    assert(munmap(two, (size_t)pg) == 0);
    unsigned char *foreign = two + pg;
    assert(!tl_fb_owns(foreign) && tl_fb_size(foreign) == 0 && !tl_fb_release(foreign));
    /* ...also with a live block present, so the registry path is exercised. */
    unsigned char *a = tl_fb_alloc(24u << 20);
    assert(a && ((uintptr_t)a % (uintptr_t)pg) == 0 && tl_fb_live() == 1);
    assert(!tl_fb_owns(foreign) && !tl_fb_release(foreign));
    assert(munmap(foreign, (size_t)pg) == 0);
    puts("PASS: foreign page-aligned pointer is never probed or released");

    /* Zero-filled, usable end to end, exact-pointer ownership only. */
    assert(a[0] == 0 && a[(24u << 20) - 1] == 0);
    memset(a, 0x77, 24u << 20);
    assert(tl_fb_owns(a) && tl_fb_size(a) == (24u << 20));
    assert(!tl_fb_owns(a + 1) && !tl_fb_owns(a + pg) && !tl_fb_release(a + 1));
    assert(tl_fb_live() == 1);
    assert(tl_fb_release(a) && tl_fb_live() == 0);
    assert(!tl_fb_release(a) && !tl_fb_owns(a));          /* double free is refused, not a crash */
    puts("PASS: 24 MiB block lifecycle, exact-pointer ownership, double release refused");

    /* Ordinary host pointers and NULL. */
    void *h = malloc(64); int s = 0;
    assert(h && !tl_fb_owns(h) && !tl_fb_release(h) && !tl_fb_owns(&s) && !tl_fb_release(NULL) && !tl_fb_owns(NULL));
    free(h);
    unsigned char *z = tl_fb_alloc(0);
    assert(z && tl_fb_size(z) == 1 && tl_fb_release(z));
    errno = 0;
    assert(!tl_fb_alloc(SIZE_MAX) && errno == ENOMEM);
    puts("PASS: host pointers, NULL, zero size and overflow");

    /* Registry exhaustion fails cleanly and recovers once blocks are released. */
    enum { MAX = 4096 };
    static void *blocks[MAX]; int n = 0;
    for (; n < MAX; n++) { blocks[n] = tl_fb_alloc(1); if (!blocks[n]) break; }
    assert(n > 0 && n < MAX && errno == ENOMEM && tl_fb_live() == (size_t)n);
    for (int i = 0; i < n; i++) assert(tl_fb_release(blocks[i]));
    assert(tl_fb_live() == 0);
    void *again = tl_fb_alloc(1);
    assert(again && tl_fb_release(again));
    puts("PASS: registry exhaustion and recovery");

    pthread_t t[4];
    for (int i = 0; i < 4; i++) assert(pthread_create(&t[i], NULL, worker, NULL) == 0);
    for (int i = 0; i < 4; i++) assert(pthread_join(t[i], NULL) == 0);
    assert(tl_fb_live() == 0);
    puts("PASS: concurrent alloc/release");
    return 0;
}
