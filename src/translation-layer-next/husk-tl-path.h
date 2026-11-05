/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_PATH_H
#define HUSK_TL_PATH_H
#include <stddef.h>

/* Android app-data aliases used by the file shims; NULL means not an alias. */
const char *tl_resolve_data_path(const char *path, const char *host_root, char *out, size_t cap);

/* getcwd with a reversible Android name for directories within this app's data.
 * The host directory and files stay in place. Allocation, ERANGE and EINVAL
 * follow getcwd; errno is unchanged on success. Other directories stay as-is. */
char *tl_guest_getcwd(char *out, size_t size, const char *host_root, const char *guest_root);
#endif
