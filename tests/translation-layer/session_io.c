/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise the actual guest syscall shims, including errno and close/reopen. */
#include "husk-tl-bionic.h"
#include "husk-tl-session.h"
#include "husk-tl-http.h"
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void tl_log_line(const char *fmt, ...) { (void)fmt; }
static atomic_bool entered, done;
static int mode, sockets[2];
static long (*guest_read)(int, void *, size_t);
static long (*guest_recv)(int, void *, size_t, int);
static int (*guest_poll)(struct pollfd *, unsigned long, int);

static void *run(void *unused)
{
    (void)unused;
    assert(tl_session_enter());
    atomic_store(&entered, true);
    char byte;
    if (mode == 0) assert(guest_read(sockets[0], &byte, 1) <= 0);
    if (mode == 1) assert(guest_recv(sockets[0], &byte, 1, 0) <= 0);
    if (mode == 2) {
        assert(guest_poll(NULL, 0, -1) == -1);
        assert(*tl_guest_errno_ptr() == 125); /* Linux ECANCELED */
    }
    tl_session_leave(); tl_session_worker_done();
    atomic_store(&done, true);
    return NULL;
}
static void *http_wait(void *url)
{
    assert(tl_session_enter());
    tl_http_request request = { .url = url, .method = "GET", .timeout_ms = 60000 };
    tl_http_response response;
    assert(!tl_http_perform(&request, &response));
    tl_http_response_free(&response);
    tl_session_leave(); tl_session_worker_done();
    atomic_store(&done, true);
    return NULL;
}

int main(void)
{
    guest_read = tl_bionic_find("read"); guest_recv = tl_bionic_find("recv"); guest_poll = tl_bionic_find("poll");
    assert(guest_read && guest_recv && guest_poll);
    for (int cycle = 0; cycle < 3; cycle++) for (mode = 0; mode < 3; mode++) {
        assert(tl_session_restart());
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
        // Normal I/O and its nonblocking errors still work before stopping.
        assert(write(sockets[1], "x", 1) == 1);
        char byte; assert(guest_read(sockets[0], &byte, 1) == 1 && byte == 'x');
        assert(guest_recv(sockets[0], &byte, 1, 0x40 /* Linux MSG_DONTWAIT */) == -1);
        assert(*tl_guest_errno_ptr() == 11);
        atomic_store(&entered, false); atomic_store(&done, false);
        assert(tl_session_worker_add());
        pthread_t thread; assert(pthread_create(&thread, NULL, run, NULL) == 0);
        while (!atomic_load(&entered)) usleep(1000);
        usleep(20000); /* let the shim block, with its peer deliberately kept open */
        tl_session_request_stop();
        for (int i = 0; i < 1000 && !atomic_load(&done); i++) usleep(1000);
        assert(atomic_load(&done)); pthread_join(thread, NULL);
        assert(tl_session_drained()); close(sockets[0]); close(sockets[1]);
    }
    assert(tl_session_restart());
    int listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
    struct sockaddr_in address = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    assert(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t len = sizeof(address); assert(getsockname(listener, (struct sockaddr *)&address, &len) == 0);
    assert(listen(listener, 1) == 0);
    char url[100]; snprintf(url, sizeof(url), "http://127.0.0.1:%d/hang", ntohs(address.sin_port));
    atomic_store(&done, false); assert(tl_session_worker_add());
    pthread_t http; assert(pthread_create(&http, NULL, http_wait, url) == 0);
    struct pollfd incoming = { .fd = listener, .events = POLLIN }; assert(poll(&incoming, 1, 5000) == 1);
    int client = accept(listener, NULL, NULL); assert(client >= 0);
    // The server deliberately never responds. Stop must cancel the URLSession task.
    tl_session_request_stop();
    for (int i = 0; i < 2000 && !atomic_load(&done); i++) usleep(1000);
    assert(atomic_load(&done)); pthread_join(http, NULL);
    close(client); close(listener); assert(tl_session_drained()); assert(tl_session_restart());
    puts("PASS: guest read/recv/poll and hanging HTTP stop and reopen; normal I/O and Linux errno preserved");
}
