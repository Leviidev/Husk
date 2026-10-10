/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Husk translation layer -- the parts of an iOS-native Android runtime that
 * exist so far. The plan, and why it is shaped this way, is in
 * docs/04-translation-layer.md.
 *
 * Everything under src/translation-layer/ is Husk's own code. None of it is
 * taken from Android Translation Layer (GPL-3.0) or from AOSP (Apache-2.0):
 * neither licence can be combined with QEMU's GPLv2 in one binary, and this
 * code is linked into the same app as QEMU. The doc's licensing section says
 * what that means for the parts of the runtime still to come.
 *
 * The entry points return JSON rather than structs. The only reader is Swift,
 * which decodes JSON in one line, whereas a struct would have to be kept in
 * step on both sides of the bridge by hand.
 */
#ifndef HUSK_TL_H
#define HUSK_TL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * What running one app in-process would take: which ABIs it ships, how much
 * Dex it carries, and for every arm64 native library, how its pages would
 * have to be mapped on a device with 16 KiB pages and W^X memory.
 *
 * `paths` is one app -- a base APK plus any split APKs. A report that could
 * not be made says so in its "error" field; NULL only when memory has run out
 * entirely. Free the result with husk_tl_free().
 */
char *husk_tl_scan(const char *const *paths, int count);

/*
 * One file out of an APK, e.g. "AndroidManifest.xml", inflated if need be.
 * NULL if the entry is absent, damaged, or larger than `limit` bytes. Free
 * with husk_tl_free().
 */
void *husk_tl_read_entry(const char *apk, const char *name, size_t limit,
                         size_t *out_len);

/*
 * Measure what the translation layer depends on that only the device itself
 * can answer: whether the thread register Android code reads its stack guard
 * from is ours to set, whether x18 survives, and whether a library image can
 * sit in executable memory with its writable data beside it.
 *
 * `may_execute` must be false unless MAP_JIT memory is already known to
 * execute in this process; the checks that run generated code are skipped
 * otherwise. Returns a JSON array; free with husk_tl_free().
 */
char *husk_tl_run_checks(bool may_execute);

/*
 * The attempt: load the given APKs' arm64 libraries into JIT memory,
 * relocate them against the shim, run a NativeActivity lifecycle for
 * `seconds`, and report what happened. Nothing is called from more than
 * one attempt at a time; husk_tl_attempt_start refuses while one is live.
 *
 * Returns 0 when the attempt started, -1 when it could not (already
 * running, thread failure). `seconds` of zero or less means no time limit. Progress: husk_tl_attempt_done() turns true
 * when the run is over, husk_tl_attempt_frames() counts frames the guest
 * posted, husk_tl_attempt_log() hands over the run's log (caller frees),
 * husk_tl_attempt_stop() asks for an early end, and husk_tl_attempt_reset()
 * frees everything after a finished run.
 */
int  husk_tl_attempt_start(const char *const *apks, int count, int seconds);
bool husk_tl_attempt_done(int *exit_code);
int  husk_tl_attempt_frames(void);
char *husk_tl_attempt_log(void);
void husk_tl_attempt_stop(void);
void husk_tl_attempt_reset(void);

/*
 * The frame the guest last finished, for the UI to show.
 *
 * Frames are handed over without a copy. husk_tl_frame_acquire() pins the newest
 * finished frame and returns its generation (zero if there is none yet, and the
 * same number as last time if nothing new has been posted); the pixels stay
 * valid and untouched until husk_tl_frame_release(token). The producer never
 * writes a frame that is pinned or is the newest, so a reader can hold one for as
 * long as the display needs it -- a CGImage's whole lifetime -- and the guest
 * keeps drawing meanwhile. Premultiplied RGBA, bytes in that order; width,
 * height and stride are in pixels.
 */
uint64_t husk_tl_frame_acquire(const uint8_t **pixels, int *width, int *height,
                               int *stride_pixels, int *token);
void husk_tl_frame_release(int token);

/* How the last couple of seconds went, from the frame pump's own counters. */
typedef struct husk_tl_perf {
    double fps;        /* frames the guest produced per second */
    double logic_ms;   /* mean per frame: input, deferred tasks, the app's doFrame */
    double render_ms;  /* mean per frame: the app's onDraw, draw calls included */
    double draw_ms;    /* of which, inside the canvas draw calls */
    double draws;      /* mean canvas draw calls per frame */
    double late_pct;   /* percent of frames that missed their deadline */
} husk_tl_perf;
void husk_tl_perf_snapshot(husk_tl_perf *out);

void husk_tl_send_touch(int action, float x, float y);

/*
 * Copy every entry under `prefix` (e.g. "assets/public/") out of an APK into `out_dir`, keeping the paths below the prefix.
 * For web apps, whose pages and scripts are served from the copy. Returns how many files were written, or -1.
 */
int husk_tl_extract(const char *apk, const char *prefix, const char *out_dir);

/* Whether an APK has an entry by this name. */
int husk_tl_has_entry(const char *apk, const char *name);

void husk_tl_free(void *p);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_H */
