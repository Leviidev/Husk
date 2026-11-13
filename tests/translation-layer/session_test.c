/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _WIN32
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#endif
#define TL_SESSION_IMPLEMENTATION
#include "husk-tl-session.h"
#include <assert.h>
#include <stdio.h>
#ifndef _WIN32
static void *race_creation(void *unused)
{
    (void)unused;
    for (unsigned i = 0; i < 10000; i++) {
        bool tracked;
        if (tl_guest_thread_reserve(&tracked) && tracked) tl_guest_thread_release((void *)1);
    }
    return NULL;
}
static void *close_after_delay(void *unused)
{
    (void)unused;
    usleep(10000);
    tl_guest_session_close();
    return NULL;
}
#endif
int main(void)
{
    bool tracked;
    assert(tl_guest_thread_reserve(&tracked) && !tracked); /* boot is independent */
    for (int session = 0; session < 10000; session++) {
        assert(tl_guest_session_begin());
        assert(!tl_guest_session_closing());
        assert(!tl_guest_session_begin());
        assert(tl_guest_thread_reserve(&tracked) && tracked);
        assert(tl_guest_thread_reserve(&tracked) && tracked);
        assert(tl_guest_session_threads() == 2);
        tl_guest_session_close();
        assert(tl_guest_session_closing());
        assert(tl_guest_session_wait_close(10000000000LL)); /* immediate when closing */
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
    assert(tl_guest_session_begin());
    assert(!tl_guest_session_wait_close(2000000LL)); /* 2 ms open timeout expires cleanly */
    pthread_t closer;
    assert(!pthread_create(&closer, NULL, close_after_delay, NULL));
    assert(tl_guest_session_wait_close(5000000000LL)); /* woken early on close */
    assert(!pthread_join(closer, NULL));
    assert(tl_guest_session_finish());
#endif
    puts("PASS: creation/drain barrier and wake-on-close prevent session reset races and exit hangs");
    return 0;
}
