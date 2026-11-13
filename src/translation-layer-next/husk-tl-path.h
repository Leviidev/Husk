/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_PATH_H
#define HUSK_TL_PATH_H
#include <stdbool.h>
#include <stddef.h>

/* Android app-data aliases used by the file shims; NULL means not an alias. */
const char *tl_resolve_data_path(const char *path, const char *host_root, char *out, size_t cap);

/* Collapses consecutive slashes in a path (e.g. "//data//data" -> "/data/data"). */
void tl_path_normalize_slashes(const char *src, char *dst, size_t cap);

/* /data, /data/data, /data/user and /data/user/0 (trailing slashes allowed): directories on
 * Android, absent on iOS. Games that resolve names case-insensitively walk the path from "/"
 * with lstat and give up at the first missing component. Returns Android's permission bits
 * (without S_IFDIR), or 0 if the path is not one of them. */
unsigned tl_android_data_ancestor_mode(const char *path);

/* access() for those directories as an app sees them: 0 for F_OK/X_OK, EACCES when R_OK or
 * W_OK is asked; -1 if the path is not one of them. */
int tl_android_data_ancestor_access(const char *path, int mode);

/* getcwd with a reversible Android name for directories within this app's data.
 * The host directory and files stay in place. Allocation, ERANGE and EINVAL
 * follow getcwd; errno is unchanged on success. Other directories stay as-is. */
char *tl_guest_getcwd(char *out, size_t size, const char *host_root, const char *guest_root);

/* Case-insensitive path resolution for APFS/Darwin:
 * If path exists, returns true. Otherwise traverses existing components and matches
 * missing components case-insensitively using readdir/strcasecmp, updating path in-place. */
bool tl_path_case_resolve(char *path);
/* Resolve an already-existing parent directory in a slash-normalized path and
 * retain a missing final leaf. Does not create anything. Rejects dot traversal, an existing leaf,
 * missing/non-directory parents and insufficient capacity; failure leaves
 * the input unchanged. */
bool tl_path_case_resolve_parent(char *path, size_t cap);
/* Bump the directory-component resolution generation after successful filesystem mutations. */
void tl_path_cache_invalidate(void);
#endif
