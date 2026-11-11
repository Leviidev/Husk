/* SPDX-License-Identifier: GPL-2.0-or-later */
#define TL_SESSION_IMPLEMENTATION
#include "husk-tl-session.h"
#include <assert.h>
#include <stdio.h>
#ifndef _WIN32
#include <pthread.h>
static void *race_creation(void *unused)
{
    (void)unused;
    for (unsigned i = 0; i < 10000; i++) {
        bool tracked;
        if (tl_guest_thread_reserve(&tracked) && tracked) tl_guest_thread_release((void *)1);
    }
    return NULL;
}
#endif
int main(void)
{
    bool tracked;
    assert(tl_guest_thread_reserve(&tracked) && !tracked); /* boot is independent */
    for (int session = 0; session < 10000; session++) {
        assert(tl_guest_session_begin());
        assert(!tl_guest_session_begin());
        assert(tl_guest_thread_reserve(&tracked) && tracked);
        assert(tl_guest_thread_reserve(&tracked) && tracked);
        assert(tl_guest_session_threads() == 2);
        tl_guest_session_close();
        assert(!tl_guest_thread_reserve(&tracked) && !tracked);
        assert(!tl_guest_session_finish());
        tl_guest_thread_release((void *)1);
        assert(!tl_guest_session_finish());
        tl_guest_thread_release((void *)1);
        assert(tl_guest_session_finish());
    }
#ifndef _WIN32
    for (int session = 0; session < 100; session++) {
        assert(tl_guest_session_begin());
        pthread_t threads[4];
        for (int i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, race_creation, NULL));
        tl_guest_session_close();
        for (int i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
        assert(tl_guest_session_threads() == 0);
        assert(tl_guest_session_finish());
    }
#endif
    puts("PASS: creation/drain barrier prevents session reset with live or pending guest threads");
    return 0;
}
