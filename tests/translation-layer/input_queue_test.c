/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "input_queue_under_test.h"

static int32_t bits_of(float v)
{
    union { float f; int32_t i; } u;
    u.f = v;
    return u.i;
}

static float float_of(int32_t b)
{
    union { float f; int32_t i; } u;
    u.i = b;
    return u.f;
}

int main(void)
{
    /* Trailing MOVE events coalesce per pointer without crossing DOWN/UP boundaries. */
    int32_t q[4096];
    int qn = 0;
    tl_input_queue_push(q, &qn, 4096, 0, 0, bits_of(10.0f), bits_of(20.0f));
    tl_input_queue_push(q, &qn, 4096, 1, 0, bits_of(11.0f), bits_of(21.0f));
    tl_input_queue_push(q, &qn, 4096, 1, 1, bits_of(300.0f), bits_of(400.0f));
    tl_input_queue_push(q, &qn, 4096, 1, 0, bits_of(19.5f), bits_of(29.25f));
    tl_input_queue_push(q, &qn, 4096, 1, 1, bits_of(345.0f), bits_of(412.5f));
    assert(qn == 3 * 4);
    assert(q[0] == 0 && q[1] == 0);
    assert(q[4] == 1 && q[5] == 0 && float_of(q[6]) == 19.5f && float_of(q[7]) == 29.25f);
    assert(q[8] == 1 && q[9] == 1 && float_of(q[10]) == 345.0f && float_of(q[11]) == 412.5f);

    tl_input_queue_push(q, &qn, 4096, 2, 0, bits_of(19.5f), bits_of(29.25f));
    tl_input_queue_push(q, &qn, 4096, 1, 1, bits_of(360.0f), bits_of(420.0f));
    assert(qn == 5 * 4);
    assert(q[12] == 2 && q[13] == 0);
    assert(q[16] == 1 && q[17] == 1 && float_of(q[18]) == 360.0f);

    /* Fast tap (DOWN+UP on pointer 0) must defer pointer 0's UP without stalling pointer 1's MOVE. */
    int64_t down_ns[16] = {0};
    uint64_t down_frame[16] = {0};
    uint8_t keep[1024] = {0};
    int32_t out[64] = {0};
    int64_t defer_until = 0;

    qn = 0;
    tl_input_queue_push(q, &qn, 4096, 0, 0, bits_of(50.0f), bits_of(50.0f));
    tl_input_queue_push(q, &qn, 4096, 2, 0, bits_of(50.0f), bits_of(50.0f));
    tl_input_queue_push(q, &qn, 4096, 1, 1, bits_of(610.0f), bits_of(310.0f));
    tl_input_queue_push(q, &qn, 4096, 1, 1, bits_of(642.5f), bits_of(318.25f));
    int total = qn / 4;
    assert(total == 3);

    int take = tl_input_queue_plan(q, total, 1000000000ll, 42, down_ns, down_frame, keep, &defer_until);
    assert(take == 2);
    assert(keep[0] == 0 && keep[1] == 1 && keep[2] == 0);
    assert(defer_until == 1016000000ll);
    tl_input_queue_commit(q, &qn, keep, total, out);
    assert(qn == 4 && q[0] == 2 && q[1] == 0);
    assert(out[0] == 0 && out[1] == 0);
    assert(out[4] == 1 && out[5] == 1 && float_of(out[6]) == 642.5f && float_of(out[7]) == 318.25f);

    /* While pointer 0's UP sits deferred, only waiting on UP returns not-ready; arriving MOVE(1) wakes immediately. */
    int64_t rem = 0;
    assert(!tl_input_queue_has_ready(q, qn / 4, 1004000000ll, 42, down_ns, down_frame, &rem));
    assert(rem == 12000000ll);

    tl_input_queue_push(q, &qn, 4096, 1, 1, bits_of(670.0f), bits_of(325.0f));
    assert(tl_input_queue_has_ready(q, qn / 4, 1004000000ll, 42, down_ns, down_frame, &rem));
    assert(rem == 0);

    /* Same-pointer subsequent DOWN(0) stays ordered behind deferred UP(0). */
    tl_input_queue_push(q, &qn, 4096, 0, 0, bits_of(52.0f), bits_of(52.0f));
    take = tl_input_queue_plan(q, qn / 4, 1004000000ll, 42, down_ns, down_frame, keep, &defer_until);
    assert(take == 1);
    tl_input_queue_commit(q, &qn, keep, qn / 4, out);
    assert(out[0] == 1 && out[1] == 1 && float_of(out[2]) == 670.0f);
    assert(qn == 8 && q[0] == 2 && q[1] == 0 && q[4] == 0 && q[5] == 0);

    /* Once a frame renders after down_frame[0], deferred UP(0) and its trailing DOWN(0) flush together in order. */
    assert(tl_input_queue_has_ready(q, qn / 4, 1005000000ll, 43, down_ns, down_frame, &rem));
    take = tl_input_queue_plan(q, qn / 4, 1005000000ll, 43, down_ns, down_frame, keep, &defer_until);
    assert(take == 2 && defer_until == 0);
    tl_input_queue_commit(q, &qn, keep, qn / 4, out);
    assert(qn == 0);
    assert(out[0] == 2 && out[1] == 0 && out[4] == 0 && out[5] == 0);

    puts("PASS: input queue coalesces trailing MOVE bursts per pointer and never stalls independent pointer moves behind another pointer's deferred fast-tap release");
    return 0;
}

