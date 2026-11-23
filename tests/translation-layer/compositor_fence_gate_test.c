#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

static struct {
    atomic_bool ended, paused, host_inactive, render_requested;
} A;

static struct {
    int lk;
    int cv;
} E2;

static int64_t g_fake_now_ns = 1000000000ll;
static bool g_ui_pending = false;
static int g_sleep_calls = 0;
static int64_t g_last_sleep_ns = 0;
static int g_cond_wait_calls = 0;
static int64_t g_last_cond_wait_ns = 0;
static bool g_wake_inactive_on_wait = false;

static int64_t now_ns(void) { return g_fake_now_ns; }
static bool tl_ui_frame_pending(void) { return g_ui_pending; }

static void test_mutex_lock(int *m) { (void)m; }
static void test_mutex_unlock(int *m) { (void)m; }

static int test_clock_gettime(int clk, struct timespec *ts)
{
    (void)clk;
    ts->tv_sec = (time_t)(g_fake_now_ns / 1000000000ll);
    ts->tv_nsec = (long)(g_fake_now_ns % 1000000000ll);
    return 0;
}

static int test_nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)rem;
    int64_t ns = (int64_t)req->tv_sec * 1000000000ll + (int64_t)req->tv_nsec;
    g_sleep_calls++;
    g_last_sleep_ns = ns;
    g_fake_now_ns += ns;
    return 0;
}

static int test_cond_timedwait(int *cv, int *lk, const struct timespec *ts)
{
    (void)cv; (void)lk;
    g_cond_wait_calls++;
    int64_t target = (int64_t)ts->tv_sec * 1000000000ll + (int64_t)ts->tv_nsec;
    if (target > g_fake_now_ns) {
        g_last_cond_wait_ns = target - g_fake_now_ns;
        g_fake_now_ns = target;
    } else {
        g_last_cond_wait_ns = 0;
    }
    if (g_wake_inactive_on_wait) atomic_store(&A.host_inactive, true);
    return 0;
}

#define pthread_mutex_lock(m) test_mutex_lock(m)
#define pthread_mutex_unlock(m) test_mutex_unlock(m)
#define pthread_cond_timedwait(cv, lk, ts) test_cond_timedwait((cv), (lk), (ts))
#define clock_gettime(c, ts) test_clock_gettime((c), (ts))
#define nanosleep(req, rem) test_nanosleep((req), (rem))

#include "compositor_fence_gate_under_test.h"

#define MAX_FENCES 16
static uintptr_t g_next_fence = 1;
static bool g_signaled[MAX_FENCES];
static bool g_deleted[MAX_FENCES];
static int g_client_wait_calls = 0;
static uint32_t g_last_wait_flags = 0xFFFFFFFFu;
static uint64_t g_last_wait_timeout = UINT64_MAX;
static int g_delete_calls = 0;
static int g_fence_sync_calls = 0;

static void reset_test_state(void)
{
    atomic_store(&A.ended, false);
    atomic_store(&A.paused, false);
    atomic_store(&A.host_inactive, false);
    atomic_store(&A.render_requested, false);
    g_fake_now_ns = 1000000000ll;
    g_ui_pending = false;
    g_sleep_calls = 0;
    g_last_sleep_ns = 0;
    g_cond_wait_calls = 0;
    g_last_cond_wait_ns = 0;
    g_wake_inactive_on_wait = false;
    g_next_fence = 1;
    for (int i = 0; i < MAX_FENCES; i++) {
        g_signaled[i] = false;
        g_deleted[i] = false;
    }
    g_client_wait_calls = 0;
    g_last_wait_flags = 0xFFFFFFFFu;
    g_last_wait_timeout = UINT64_MAX;
    g_delete_calls = 0;
    g_fence_sync_calls = 0;
}

static void *stub_fence_sync(uint32_t condition, uint32_t flags)
{
    assert(condition == 0x9117u);
    assert(flags == 0u);
    g_fence_sync_calls++;
    uintptr_t id = g_next_fence++;
    assert(id < MAX_FENCES);
    g_signaled[id] = false;
    g_deleted[id] = false;
    return (void *)id;
}

static uint32_t stub_client_wait_sync(void *fence, uint32_t flags, uint64_t timeout_ns)
{
    uintptr_t id = (uintptr_t)fence;
    assert(id > 0 && id < g_next_fence && !g_deleted[id]);
    assert(flags == 0u);
    assert(timeout_ns == 0u);
    g_client_wait_calls++;
    g_last_wait_flags = flags;
    g_last_wait_timeout = timeout_ns;
    return g_signaled[id] ? 0x911Au /* GL_ALREADY_SIGNALED */ : 0x911Bu /* GL_TIMEOUT_EXPIRED */;
}

static void stub_delete_sync(void *fence)
{
    uintptr_t id = (uintptr_t)fence;
    assert(id > 0 && id < g_next_fence && !g_deleted[id]);
    g_deleted[id] = true;
    g_delete_calls++;
}

int main(void)
{
    /* 1. Double-buffered mode (!single_flight): blocks only N-2, never N-1,
     * polls strictly with flags=0, timeout=0, and preserves unsignaled fences. */
    reset_test_state();
    void *fences[2] = { NULL, NULL };
    int idx = 0;
    int64_t defer_since = 0;

    assert(compositor_gpu_ready(fences, idx, false, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    compositor_gpu_arm(fences, &idx, stub_fence_sync, stub_delete_sync); /* slot 0 = id 1, idx -> 1 */
    compositor_gpu_arm(fences, &idx, stub_fence_sync, stub_delete_sync); /* slot 1 = id 2, idx -> 0 */
    assert(idx == 0 && (uintptr_t)fences[0] == 1 && (uintptr_t)fences[1] == 2);

    /* N-2 (id 1) not signaled yet -> must defer before touching FBO 0 */
    assert(!compositor_gpu_ready(fences, idx, false, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    assert(g_client_wait_calls == 1 && g_last_wait_flags == 0u && g_last_wait_timeout == 0u);
    assert(defer_since == g_fake_now_ns);
    assert((uintptr_t)fences[0] == 1 && !g_deleted[1]);

    /* N-2 signals while N-1 (id 2) is still busy -> ready immediately! */
    g_signaled[1] = true;
    g_fake_now_ns += 4000000ll;
    assert(compositor_gpu_ready(fences, idx, false, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    assert(fences[0] == NULL && g_deleted[1] && defer_since == 0);
    assert((uintptr_t)fences[1] == 2 && !g_deleted[2]);

    /* 2. Single-flight mode (capture / thermal >= 2 / notification blur):
     * even when N-2 is empty, N-1 unsignaled must defer without blocking ANGLE. */
    compositor_gpu_arm(fences, &idx, stub_fence_sync, stub_delete_sync); /* slot 0 = id 3, idx -> 1 */
    g_signaled[2] = true; /* N-2 = slot 1 (id 2) signaled, N-1 = slot 0 (id 3) still running */
    assert(!compositor_gpu_ready(fences, idx, true, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    assert(fences[1] == NULL && g_deleted[2]);
    assert((uintptr_t)fences[0] == 3 && !g_deleted[3]);
    assert(defer_since == g_fake_now_ns);

    /* Watchdog clears N-1-only stall at 120 ms without waiting for 180 ms */
    g_fake_now_ns += 119000000ll;
    assert(!compositor_gpu_ready(fences, idx, true, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    assert((uintptr_t)fences[0] == 3);
    g_fake_now_ns += 1000000ll;
    assert(compositor_gpu_ready(fences, idx, true, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    assert(fences[0] == NULL && g_deleted[3] && defer_since == 0);

    /* 3. N-2 stall uses longer 180 ms watchdog before releasing both wedged slots */
    reset_test_state();
    defer_since = 0; idx = 0;
    fences[0] = fences[1] = NULL;
    compositor_gpu_arm(fences, &idx, stub_fence_sync, stub_delete_sync); /* id 1 */
    compositor_gpu_arm(fences, &idx, stub_fence_sync, stub_delete_sync); /* id 2 */
    assert(!compositor_gpu_ready(fences, idx, true, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    g_fake_now_ns += 150000000ll;
    assert(!compositor_gpu_ready(fences, idx, true, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    assert(!g_deleted[1] && !g_deleted[2]);
    g_fake_now_ns += 30000000ll;
    assert(compositor_gpu_ready(fences, idx, true, g_fake_now_ns, &defer_since, stub_client_wait_sync, stub_delete_sync));
    assert(fences[0] == NULL && fences[1] == NULL && g_deleted[1] && g_deleted[2] && defer_since == 0);

    /* 4. compositor_gpu_wait polls non-blocking and exits immediately on host_inactive */
    reset_test_state();
    fences[0] = fences[1] = NULL; idx = 0;
    compositor_gpu_arm(fences, &idx, stub_fence_sync, stub_delete_sync); /* slot 0 = id 1, idx = 1 */
    atomic_store(&A.host_inactive, true);
    compositor_gpu_wait(fences, idx, true, stub_client_wait_sync, stub_delete_sync);
    assert(g_sleep_calls == 0);
    assert(fences[0] == NULL && g_deleted[1]);

    /* 5. wait_capture_cadence maintains 0.5 ms slack and breaks when host_inactive rises */
    reset_test_state();
    int64_t last_swap = g_fake_now_ns;
    g_fake_now_ns += 4000000ll;
    wait_capture_cadence(60, last_swap);
    assert(g_cond_wait_calls == 1);
    assert(g_last_cond_wait_ns == (1000000000ll / 60) - 500000ll - 4000000ll);

    atomic_store(&A.host_inactive, true);
    wait_capture_cadence(60, g_fake_now_ns);
    assert(g_cond_wait_calls == 1); /* immediate skip while Control Center / overlay inactive */

    compositor_gpu_arm(fences, &idx, stub_fence_sync, stub_delete_sync);
    compositor_gpu_release(fences, stub_delete_sync);
    assert(fences[0] == NULL && fences[1] == NULL);

    puts("PASS: non-blocking GPU fence gate defers FBO0 before ShareGroup lock contention and aborts cleanly on host_inactive");
    return 0;
}
