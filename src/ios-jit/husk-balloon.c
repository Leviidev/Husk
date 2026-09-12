/*
 * Husk: host control of the guest's memory balloon.
 *
 * iOS gives this process a hard jetsam ceiling -- about 3.4 GB on a 12 GB
 * phone -- and a QEMU guest's RAM is the worst kind of memory to spend it on:
 * dirty and anonymous. Worse, QEMU's resident size is a high-water mark. Once
 * the guest touches a page it stays resident even after the guest frees it, so
 * a guest sized to fit the ceiling at its peak wastes that memory for the rest
 * of the session, and a guest sized any larger is killed.
 *
 * A balloon turns that fixed reservation into a pool. The guest is given more
 * RAM than the process could permanently afford; when the host runs short, the
 * balloon inflates, the guest hands pages back, and QEMU releases them to the
 * OS. When the pressure passes, the balloon deflates and the guest has them
 * again. The ceiling stops being a wall and becomes something to steer against.
 *
 * qmp_balloon() must run under the BQL, and the caller here is a plain thread
 * sampling os_proc_available_memory(). Hopping through a bottom half on the
 * main AIO context puts the call where QEMU expects it.
 */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "block/aio.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-machine.h"

#include "husk-balloon.h"

static void husk_balloon_apply(void *opaque)
{
    int64_t target = (int64_t)(intptr_t)opaque;
    Error *err = NULL;

    /* Already under the BQL: a main-context bottom half runs with it held, and
     * taking it again aborts on assertion failed: (!bql_locked()). */
    qmp_balloon(target, &err);

    if (err) {
        fprintf(stderr, "[husk-balloon] qmp_balloon(%lld) failed: %s\n",
                (long long)target, error_get_pretty(err));
        error_free(err);
    }
}

void husk_balloon_set_bytes(int64_t target_bytes)
{
    if (target_bytes <= 0) {
        return;
    }
    /*
     * Safe to call from any thread; the bottom half runs on the main loop.
     * The target is passed by value rather than by pointer so there is nothing
     * to free and no lifetime to reason about.
     */
    aio_bh_schedule_oneshot(qemu_get_aio_context(), husk_balloon_apply,
                            (void *)(intptr_t)target_bytes);
}
