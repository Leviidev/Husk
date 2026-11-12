/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>

static _Atomic int control;
static int calls, yields, payload;
static int test_sched_yield(void)
{
    /* Deterministically finish the other initializer after the actual wrapper
     * waits. Reversed states either return too early or reach this hook while
     * the control is already complete, so neither old failure can hang QA. */
    assert(atomic_load(&control) == 1);
    assert(++yields == 1);
    payload = 123;
    atomic_store(&control, 2);
    return 0;
}
#define sched_yield test_sched_yield
#include "pthread_once_under_test.h"
#undef sched_yield

static void initialize(void)
{
    assert(atomic_load(&control) == 1);
    calls++;
    payload = 123;
}

int main(void)
{
    /* A cold control exposes Bionic's underway state during the initializer,
     * and leaves Bionic's completed state for native inline readers. */
    assert(!b_once(&control, initialize));
    assert(calls == 1 && !yields && payload == 123 && atomic_load(&control) == 2);
    for (int i = 0; i < 10000; i++) assert(!b_once(&control, initialize));
    assert(calls == 1 && !yields && atomic_load(&control) == 2);

    /* A completed control supplied by a different Bionic caller must return
     * immediately without rerunning initialization or treating it as busy. */
    atomic_store(&control, 2); payload = 456;
    assert(!b_once(&control, initialize));
    assert(calls == 1 && !yields && payload == 456);

    /* State 1 belongs to another initializer, so callers cannot observe its
     * unfinished payload before publication of state 2. */
    atomic_store(&control, 1); payload = 0;
    assert(!b_once(&control, initialize));
    assert(calls == 1 && yields == 1 && payload == 123 && atomic_load(&control) == 2);
    assert(!b_once(&control, initialize));
    assert(calls == 1 && yields == 1);
    puts("PASS: production pthread_once matches Bionic's 0/1/2 ABI, runs once, accepts completed controls and waits for underway initialization");
    return 0;
}
