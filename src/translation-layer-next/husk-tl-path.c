/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-path.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#endif

void tl_path_normalize_slashes(const char *src, char *dst, size_t cap)
{
    if (!dst || !cap) return;
    if (!src) { dst[0] = 0; return; }
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < cap; i++) {
        char c = (src[i] == '\\') ? '/' : src[i];
        if (c == '/' && j > 0 && dst[j - 1] == '/') continue;
        dst[j++] = c;
    }
    dst[j] = 0;
}

const char *tl_resolve_data_path(const char *path, const char *host_root, char *out, size_t cap)
{
    if (!path || !host_root) return NULL;
    char norm[1024];
    if (strstr(path, "//") || strchr(path, '\\')) {
        tl_path_normalize_slashes(path, norm, sizeof(norm));
        path = norm;
    }
    static const char *const roots[] = { "/data/user/0/", "/data/data/" };
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        size_t n = strlen(roots[i]);
        if (strncmp(path, roots[i], n) || !path[n] || path[n] == '/') continue;
        const char *rest = strchr(path + n, '/');
        snprintf(out, cap, "%s%s", host_root, rest ? rest : "");
        return out;
    }
    return NULL;
}

unsigned tl_android_data_ancestor_mode(const char *path)
{
    if (!path || (path[0] != '/' && path[0] != '\\')) return 0;
    char norm[64];
    if (strstr(path, "//") || strchr(path, '\\')) {
        tl_path_normalize_slashes(path, norm, sizeof(norm));
        path = norm;
    }
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') n--;
    /* Android's modes: system-owned, traversable by an app but not listable. */
    static const struct { const char *name; unsigned mode; } dirs[] = {
        { "/data", 0771 }, { "/data/data", 0771 }, { "/data/user", 0711 }, { "/data/user/0", 0771 },
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
        if (strlen(dirs[i].name) == n && !strncmp(path, dirs[i].name, n)) return dirs[i].mode;
    return 0;
}

int tl_android_data_ancestor_access(const char *path, int mode)
{
    if (!tl_android_data_ancestor_mode(path)) return -1;
    /* R_OK 4 and W_OK 2 (the same on bionic and Darwin): only search (X_OK) is granted to "other". */
    return (mode & 6) ? EACCES : 0;
}

static char *native_cwd(void)
{
#ifdef _WIN32
    char *p = _getcwd(NULL, 0);
    if (p) for (char *c = p; *c; c++) if (*c == '\\') *c = '/';
    return p;
#else
    return getcwd(NULL, 0);
#endif
}

static char *canonical_root(const char *root)
{
#ifdef _WIN32
    char *p = _fullpath(NULL, root, 0);
    if (p) for (char *c = p; *c; c++) if (*c == '\\') *c = '/';
    return p;
#else
    return realpath(root, NULL);
#endif
}

/* iOS exposes the same container through /var and /private/var. */
static const char *without_private(const char *p)
{
    return !strncmp(p, "/private/var/", 13) ? p + 8 : p;
}

char *tl_guest_getcwd(char *out, size_t size, const char *host_root, const char *guest_root)
{
    int saved_errno = errno;
    if (out && !size) { errno = EINVAL; return NULL; }
    char *cwd = native_cwd();
    if (!cwd) return NULL;
    char *canonical = host_root && *host_root ? canonical_root(host_root) : NULL;
    const char *prefix = "", *suffix = cwd;
    if (canonical && guest_root && *guest_root) {
        const char *root = without_private(canonical), *actual = without_private(cwd);
        size_t n = strlen(root);
        while (n > 1 && root[n - 1] == '/') n--;
        /* A sibling with a common prefix is not this app's directory. Only
         * replace a longer physical prefix; avoid changing short host paths. */
        if (n > strlen(guest_root) && !strncmp(actual, root, n) &&
            (actual[n] == 0 || actual[n] == '/')) {
            prefix = guest_root;
            suffix = actual + n;
        }
    }
    size_t a = strlen(prefix), b = strlen(suffix), needed = a + b + 1;
    int failure = 0;
    if (size && needed > size) failure = ERANGE;
    else if (!out && !(out = malloc(size ? size : needed))) failure = ENOMEM;
    if (!failure) {
        memcpy(out, prefix, a);
        memcpy(out + a, suffix, b + 1);
    }
    free(canonical);
    free(cwd);
    errno = failure ? failure : saved_errno;
    return failure ? NULL : out;
}

bool tl_path_case_resolve(char *path)
{
    if (!path || !path[0]) return false;
    if (access(path, 0) == 0) return true;
#ifndef _WIN32
    size_t len = strlen(path);
    size_t i = 0;
    while (i < len) {
        while (path[i] == '/') i++;
        if (i >= len) break;
        size_t start = i;
        while (path[i] && path[i] != '/') i++;
        size_t comp_len = i - start;
        if (comp_len == 0) continue;
        if (comp_len == 1 && path[start] == '.') continue;
        if (comp_len == 2 && path[start] == '.' && path[start + 1] == '.') continue;

        char saved = path[i];
        path[i] = '\0';
        if (access(path, 0) != 0) {
            char parent[1024];
            if (start <= 1) {
                snprintf(parent, sizeof(parent), "/");
            } else {
                size_t p_len = start - 1;
                while (p_len > 1 && path[p_len - 1] == '/') p_len--;
                snprintf(parent, sizeof(parent), "%.*s", (int)p_len, path);
            }
            DIR *d = opendir(parent);
            bool found = false;
            if (d) {
                struct dirent *de;
                while ((de = readdir(d))) {
                    if (strcasecmp(de->d_name, path + start) == 0) {
                        size_t de_len = strlen(de->d_name);
                        if (de_len == comp_len) {
                            memcpy(path + start, de->d_name, comp_len);
                        } else {
                            path[i] = saved;
                            memmove(path + start + de_len, path + i, strlen(path + i) + 1);
                            memcpy(path + start, de->d_name, de_len);
                            i = start + de_len;
                            len = strlen(path);
                            saved = path[i];
                            path[i] = '\0';
                        }
                        found = true;
                        break;
                    }
                }
                closedir(d);
            }
            if (!found) {
                path[i] = saved;
                return false;
            }
        }
        path[i] = saved;
    }
    return access(path, 0) == 0;
#else
    return false;
#endif
}
