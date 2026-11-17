/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned g_epoch = 1;
static bool g_retiring = false;
static unsigned tl_guest_session_epoch(void) { return g_epoch; }
static bool tl_guest_thread_is_retiring(void) { return g_retiring; }

static const uint8_t k_sys_code[64] = {0};
static const uint8_t k_app_code[64] = {0};
static bool tl_ld_is_app_code(const void *addr)
{
    uintptr_t p = (uintptr_t)addr;
    return p >= (uintptr_t)k_app_code && p < (uintptr_t)(k_app_code + sizeof(k_app_code));
}

#include "pthread_keys_under_test.h"

static void dummy_sys_dtor(void *p) { (void)p; }

int main(void)
{
    int sys_key = 0, app_key1 = 0, app_key2 = 0, next_key = 0;

    /* Session 1: persistent system library (e.g. libcrypto.so) and App 1 create keys */
    g_epoch = 1;
    assert(key_create_from(&sys_key, dummy_sys_dtor, k_sys_code) == 0 && sys_key == 1);
    assert(key_create_from(&app_key1, NULL, k_app_code) == 0 && app_key1 == 2);
    assert(b_setspecific(sys_key, (void *)(uintptr_t)0x1111) == 0);
    assert(b_setspecific(app_key1, (void *)(uintptr_t)0x2222) == 0);
    assert(b_getspecific(sys_key) == (void *)(uintptr_t)0x1111);
    assert(b_getspecific(app_key1) == (void *)(uintptr_t)0x2222);

    /* Switch App 1 -> App 2: tl_pthread_reset_session(1) drops App 1 keys but MUST keep system keys */
    tl_pthread_reset_session(1);
    g_epoch = 2;

    assert(b_getspecific(sys_key) == (void *)(uintptr_t)0x1111);
    assert(b_getspecific(app_key1) == NULL);

    /* Session 2 (App 2): creating an app key MUST reclaim app slot 2, NEVER collide with system slot 1 */
    assert(key_create_from(&app_key2, NULL, k_app_code) == 0);
    assert(app_key2 == app_key1 && app_key2 != sys_key);
    assert(b_setspecific(app_key2, (void *)(uintptr_t)0x3333) == 0);
    assert(b_getspecific(sys_key) == (void *)(uintptr_t)0x1111);
    assert(b_getspecific(app_key2) == (void *)(uintptr_t)0x3333);

    /* Direct b_key_create from host test code treats host test binary as non-app (system) */
    assert(b_key_create(&next_key, NULL) == 0 && next_key == 3);
    assert(b_key_delete(next_key) == 0);

    puts("PASS: persistent system pthread keys (e.g. BoringSSL/libc++) survive game-switch reset while app keys reclaim cleanly without slot aliasing");
    return 0;
}
