/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#include "husk-tl-fallback-alloc.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

#define FB_SLOTS 512

typedef struct { uintptr_t base; size_t mapped, requested; } fb_slot;

static fb_slot g_slots[FB_SLOTS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
/* Lets free() of an ordinary pointer skip the lock while no block is live. */
static atomic_size_t g_live;

static size_t page_size(void)
{
    long p = sysconf(_SC_PAGESIZE);
    return p > 0 ? (size_t)p : 16384;
}

void *tl_fb_alloc(size_t n)
{
    if (n == 0) n = 1;
    size_t pg = page_size();
    if (n > SIZE_MAX - pg) { errno = ENOMEM; return NULL; }
    size_t mapped = (n + pg - 1) & ~(pg - 1);

    pthread_mutex_lock(&g_lock);
    int free_slot = -1;
    for (int i = 0; i < FB_SLOTS; i++)
        if (!g_slots[i].base) { free_slot = i; break; }
    if (free_slot < 0) {
        pthread_mutex_unlock(&g_lock);
        errno = ENOMEM;
        return NULL;
    }
    void *m = mmap(NULL, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (m == MAP_FAILED) {
        int e = errno;
        pthread_mutex_unlock(&g_lock);
        errno = e;
        return NULL;
    }
    g_slots[free_slot].base = (uintptr_t)m;
    g_slots[free_slot].mapped = mapped;
    g_slots[free_slot].requested = n;
    atomic_fetch_add_explicit(&g_live, 1, memory_order_release);
    pthread_mutex_unlock(&g_lock);
    return m;
}

static int find_locked(const void *p)
{
    uintptr_t a = (uintptr_t)p;
    for (int i = 0; i < FB_SLOTS; i++)
        if (g_slots[i].base == a) return i;
    return -1;
}

bool tl_fb_owns(const void *p)
{
    if (!p || atomic_load_explicit(&g_live, memory_order_acquire) == 0) return false;
    pthread_mutex_lock(&g_lock);
    bool r = find_locked(p) >= 0;
    pthread_mutex_unlock(&g_lock);
    return r;
}

size_t tl_fb_size(const void *p)
{
    if (!p || atomic_load_explicit(&g_live, memory_order_acquire) == 0) return 0;
    pthread_mutex_lock(&g_lock);
    int i = find_locked(p);
    size_t r = i >= 0 ? g_slots[i].requested : 0;
    pthread_mutex_unlock(&g_lock);
    return r;
}

bool tl_fb_release(void *p)
{
    if (!p || atomic_load_explicit(&g_live, memory_order_acquire) == 0) return false;
    pthread_mutex_lock(&g_lock);
    int i = find_locked(p);
    if (i < 0) { pthread_mutex_unlock(&g_lock); return false; }
    void *base = (void *)g_slots[i].base;
    size_t mapped = g_slots[i].mapped;
    g_slots[i].base = 0; g_slots[i].mapped = 0; g_slots[i].requested = 0;
    atomic_fetch_sub_explicit(&g_live, 1, memory_order_release);
    pthread_mutex_unlock(&g_lock);
    munmap(base, mapped);
    return true;
}

size_t tl_fb_live(void)
{
    return atomic_load_explicit(&g_live, memory_order_acquire);
}
