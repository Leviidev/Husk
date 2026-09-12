/*
 * Husk: save and restore the whole machine, so Android boots once ever.
 *
 * Booting Android under emulation costs about ten minutes: dex2oat compiles the
 * system, a hundred services start, and every instruction of it is translated.
 * None of that work is interesting and none of it changes between runs, so the
 * answer is not to make booting faster but to stop doing it. Save the machine
 * once it is up, and every later launch restores RAM and device state from disk
 * -- seconds, not minutes, and dex2oat never runs again.
 *
 * The state lands inside vdb (userdata), which is the writable qcow2. vda holds
 * the system image and is effectively read-only; putting snapshots there would
 * grow the file we distribute.
 *
 * Both calls must run under the BQL on the main loop. Saving is asked for from
 * a plain thread, so it hops through a bottom half exactly as the balloon does;
 * the bottom half then already holds the BQL and must not take it again.
 * Loading is different: it runs straight after qemu_init(), which returns with
 * the BQL held by this very thread, so it needs no bottom half at all.
 *
 * Neither call stops the CPUs by hand. save_snapshot() and the load sequence
 * below do it at the points where it is correct to do it, and a stop issued
 * outside those points is not merely redundant -- it is recorded in the state
 * that gets written out, and the machine comes back paused.
 */
#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/error-report.h"
#include "block/aio.h"
#include "qapi/error.h"
#include "migration/snapshot.h"
#include "system/runstate.h"

#include "husk-snapshot.h"

#define HUSK_SNAPSHOT_NAME "husk-booted"

/*
 * No explicit device list.
 *
 * The first attempt named "vdb" and QEMU answered "No block device node 'vdb'":
 * that is the -drive id, while the devices argument wants block *node* names,
 * which are auto-generated here. Letting QEMU choose picks every snapshot-capable
 * disk by itself, which is what plain savevm does anyway.
 */

static husk_snapshot_cb husk_cb;

static void husk_report(bool ok, const char *what, Error *err)
{
    fprintf(stderr, "[husk-snap] %s %s%s%s\n", what, ok ? "OK" : "FAILED",
            err ? ": " : "", err ? error_get_pretty(err) : "");
    if (husk_cb) {
        husk_cb(ok, what);
    }
}

static void husk_save_bh(void *opaque)
{
    Error *err = NULL;
    bool ok;

    /*
     * No bql_lock() here. A bottom half on the main AIO context already runs
     * with the BQL held, and taking it a second time aborts the process:
     *
     *   ERROR: cpus.c:556:bql_lock_impl: assertion failed: (!bql_locked())
     *
     * No vm_stop()/vm_start() either, tempting as it looks. save_snapshot()
     * records the run state it found on entry, calls global_state_store() and
     * vm_stop(RUN_STATE_SAVE_VM) itself, and restores that state on the way
     * out. Stopping the machine first means the state it records -- and writes
     * into the snapshot -- is "stopped", so the restored machine comes back
     * paused and the screen never moves again.
     */
    ok = save_snapshot(HUSK_SNAPSHOT_NAME, true, NULL, false, NULL, &err);

    husk_report(ok, "save", err);
    error_free(err);
}

void husk_snapshot_save(husk_snapshot_cb cb)
{
    husk_cb = cb;
    aio_bh_schedule_oneshot(qemu_get_aio_context(), husk_save_bh, NULL);
}

bool husk_snapshot_load_at_startup(void)
{
    Error *err = NULL;
    RunState saved;
    bool ok;

    /*
     * Called straight after qemu_init(), which returns with the BQL held by
     * this thread -- upstream's main() unlocks it on the next line -- so there
     * is no bottom half here and no lock to take.
     *
     * The machine is however already *running* by this point: qemu_init() ends
     * in qmp_x_exit_preconfig(), which calls qmp_cont() unless -S was passed,
     * and vCPU threads are executing guest code. Loading a snapshot underneath
     * running vCPUs is not something load_snapshot() defends against, so stop
     * them first and resume afterwards -- the same order -loadvm uses.
     */
    saved = runstate_get();
    vm_stop(RUN_STATE_RESTORE_VM);

    ok = load_snapshot(HUSK_SNAPSHOT_NAME, NULL, false, NULL, &err);
    if (!ok) {
        /*
         * Not an error worth shouting about: the common case is simply that no
         * snapshot exists yet, on the very first run. Put the machine back the
         * way it was and let it boot.
         */
        fprintf(stderr, "[husk-snap] no snapshot restored (%s); booting normally\n",
                err ? error_get_pretty(err) : "none found");
        error_free(err);
        vm_resume(saved);
        return false;
    }

    fprintf(stderr, "[husk-snap] restored '%s' -- Android is already booted\n",
            HUSK_SNAPSHOT_NAME);
    /*
     * Resume running whatever the machine was doing before, rather than
     * whatever state the snapshot happened to be taken in.
     */
    load_snapshot_resume(RUN_STATE_RUNNING);
    return true;
}
