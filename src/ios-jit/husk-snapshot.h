#ifndef HUSK_SNAPSHOT_H
#define HUSK_SNAPSHOT_H

#include <stdbool.h>

/* Reports completion; `what` is "save". Called off the QEMU main loop. */
typedef void (*husk_snapshot_cb)(bool ok, const char *what);

/*
 * Save the running machine. Asynchronous: the work happens on QEMU's main loop,
 * with the vCPUs stopped for its duration, and the callback fires when it is
 * done. Expect it to take a while -- the guest's whole RAM is written to disk.
 */
void husk_snapshot_save(husk_snapshot_cb cb);

/*
 * Restore, if a snapshot exists. Call immediately after qemu_init() and before
 * qemu_main_loop(). Returns false when there is nothing to restore, which is
 * the normal first-run case and not a failure.
 */
bool husk_snapshot_load_at_startup(void);

#endif
