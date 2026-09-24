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

void husk_tl_free(void *p);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_H */
