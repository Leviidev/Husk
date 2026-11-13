/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>

static uint64_t g_mock_ns = 1000000000000ull; /* 1000.0s monotonic base */
static int g_yields = 0;
static int g_backoffs = 0;
static struct timespec g_last_backoff = { 0, 0 };

static int test_clock_gettime(clockid_t clk, struct timespec *ts)
{
    (void)clk;
    ts->tv_sec = (time_t)(g_mock_ns / 1000000000ull);
    ts->tv_nsec = (long)(g_mock_ns % 1000000000ull);
    return 0;
}

static int test_nanosleep(const struct timespec *req, struct timespec *rem)
{
    (void)rem;
    g_backoffs++;
    g_last_backoff = *req;
    g_mock_ns += (uint64_t)req->tv_sec * 1000000000ull + (uint64_t)req->tv_nsec;
    return 0;
}

static int test_sched_yield(void)
{
    g_yields++;
    return 0;
}

#define clock_gettime test_clock_gettime
#define nanosleep test_nanosleep
#define sched_yield test_sched_yield
/* Extracted from src/translation-layer-next/husk-tl-bionic-pthread.c by tests/run_portable.py. */
#include "bionic_wait_yield_under_test.h"
#undef clock_gettime
#undef nanosleep
#undef sched_yield

int main(void)
{
    struct timespec rel = { 0, 0 };

    /* 1. cond_rel_timeout rejects NULL and normalizes past / out-of-range / huge deadlines. */
    assert(cond_rel_timeout(CLOCK_MONOTONIC, NULL, &rel) == EINVAL);

    g_mock_ns = 1000ull * 1000000000ull + 500000000ull; /* 1000.5s */
    struct timespec past = { 999, 900000000L };
    assert(cond_rel_timeout(CLOCK_MONOTONIC, &past, &rel) == 0);
    assert(rel.tv_sec == 0 && rel.tv_nsec == 0);

    struct timespec unnormalized = { 1000, 1500000000L }; /* 1001.5s => +1.0s */
    assert(cond_rel_timeout(CLOCK_MONOTONIC, &unnormalized, &rel) == 0);
    assert(rel.tv_sec == 1 && rel.tv_nsec == 0);

    struct timespec negative_ns = { 1002, -250000000L }; /* 1001.75s => +1.25s */
    assert(cond_rel_timeout(CLOCK_MONOTONIC, &negative_ns, &rel) == 0);
    assert(rel.tv_sec == 1 && rel.tv_nsec == 250000000L);

    struct timespec far_future = { (time_t)1000 + 86400L * 1000L, 123456L };
    assert(cond_rel_timeout(CLOCK_MONOTONIC, &far_future, &rel) == 0);
    assert(rel.tv_sec == 86400 * 365 && rel.tv_nsec == 0);

    /* 2. tl_bionic_sched_yield yields normally until tight spin streak hits 8, then backs off. */
    g_yields = 0;
    g_backoffs = 0;
    for (int i = 0; i < 8; i++) {
        g_mock_ns += 1000ull; /* 1 us tight spin */
        assert(tl_bionic_sched_yield() == 0);
    }
    assert(g_yields == 8 && g_backoffs == 0);

    /* 9th tight spin (<500us intervals for 8 consecutive transitions) triggers 200us backoff & resets streak. */
    g_mock_ns += 1000ull;
    assert(tl_bionic_sched_yield() == 0);
    assert(g_yields == 9 && g_backoffs == 1);
    assert(g_last_backoff.tv_sec == 0 && g_last_backoff.tv_nsec == 200000L);

    /* A gap >= 500us resets streak without spurious backoff. */
    g_mock_ns += 600000ull;
    for (int i = 0; i < 8; i++) {
        g_mock_ns += 1000ull;
        assert(tl_bionic_sched_yield() == 0);
    }
    assert(g_backoffs == 1);

    g_mock_ns += 1000ull;
    assert(tl_bionic_sched_yield() == 0);
    assert(g_backoffs == 2);

    puts("PASS: cond_rel_timeout normalization and tl_bionic_sched_yield spin backoff verified");
    return 0;
}

