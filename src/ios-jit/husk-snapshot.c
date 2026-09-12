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
 * Both calls must run under the BQL on the main loop, and the caller is a plain
 * thread, so they hop through a bottom half exactly as the balloon does. Saving
 * also has to stop the CPUs first: a snapshot taken while vCPUs are running
 * captures a machine mid-instruction and restores into nonsense.
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
    bool was_running;
    bool ok;

    bql_lock();
    /*
     * Stop first. save_snapshot() on a running machine captures vCPUs
     * mid-instruction, and what comes back is not a machine that can resume.
     */
    was_running = runstate_is_running();
    if (was_running) {
        vm_stop(RUN_STATE_SAVE_VM);
    }

    ok = save_snapshot(HUSK_SNAPSHOT_NAME, true, NULL, false, NULL, &err);

    if (was_running) {
        vm_start();
    }
    bql_unlock();

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
    bool ok;

    /*
     * Called straight after qemu_init() and before the main loop runs, so the
     * BQL is already held by this thread and the machine is stopped -- which is
     * exactly the state load_snapshot wants. No bottom half here.
     */
    ok = load_snapshot(HUSK_SNAPSHOT_NAME, NULL, false, NULL, &err);
    if (!ok) {
        /*
         * Not an error worth shouting about: the common case is simply that no
         * snapshot exists yet, on the very first run.
         */
        fprintf(stderr, "[husk-snap] no snapshot restored (%s); booting normally\n",
                err ? error_get_pretty(err) : "none found");
        error_free(err);
        return false;
    }
    fprintf(stderr, "[husk-snap] restored '%s' -- Android is already booted\n",
            HUSK_SNAPSHOT_NAME);
    return true;
}
