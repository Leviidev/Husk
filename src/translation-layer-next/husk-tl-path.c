/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-path.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static atomic_uint_fast64_t g_path_cache_gen = 1;

void tl_path_cache_invalidate(void)
{
    atomic_fetch_add_explicit(&g_path_cache_gen, 1, memory_order_release);
}

#ifndef _WIN32
#define CASE_CACHE_SIZE 512u
#define CASE_CACHE_PROBE 4u

typedef struct {
    uint64_t gen;
    uint64_t dev;
    uint64_t ino;
    uint64_t mtime_sec;
    uint64_t mtime_nsec;
    uint64_t stamp;
    uint32_t nlink;
    uint32_t hash;
    uint16_t comp_len;
    uint16_t actual_len;
    uint8_t state; /* 0 = empty, 1 = negative miss, 2 = positive hit */
    char query_ci[256];
    char actual[256];
} case_cache_entry;

static case_cache_entry g_case_cache[CASE_CACHE_SIZE];
static atomic_flag g_case_cache_lock = ATOMIC_FLAG_INIT;
static uint64_t g_case_cache_clock = 0;

static uint32_t case_comp_hash(const char *s, size_t n, char *ci_out)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        ci_out[i] = (char)c;
        h ^= c;
        h *= 16777619u;
    }
    ci_out[n] = '\0';
    return h;
}

static uint32_t case_bucket_index(uint64_t dev, uint64_t ino, uint32_t comp_hash)
{
    uint64_t x = dev * 0x9e3779b97f4a7c15ULL ^ ino * 0xbf58476d1ce4e5b9ULL ^ comp_hash;
    x ^= x >> 30;
    return (uint32_t)(x & (CASE_CACHE_SIZE - 1u));
}

static void stat_dir_stamp(const struct stat *st, uint64_t *dev, uint64_t *ino,
                           uint32_t *nlink, uint64_t *ms, uint64_t *mn)
{
    *dev = (uint64_t)st->st_dev;
    *ino = (uint64_t)st->st_ino;
    *nlink = (uint32_t)st->st_nlink;
#if defined(__APPLE__)
    *ms = (uint64_t)st->st_mtimespec.tv_sec;
    *mn = (uint64_t)st->st_mtimespec.tv_nsec;
#elif defined(__linux__)
    *ms = (uint64_t)st->st_mtim.tv_sec;
    *mn = (uint64_t)st->st_mtim.tv_nsec;
#else
    *ms = (uint64_t)st->st_mtime;
    *mn = 0;
#endif
}

static int case_cache_lookup(uint64_t gen, const struct stat *pst, const char *comp, size_t comp_len,
                             char *actual_out, size_t *actual_len_out)
{
    if (comp_len == 0 || comp_len >= 256) return 0;
    char ci[256];
    uint32_t h = case_comp_hash(comp, comp_len, ci);
    uint64_t dev, ino, ms, mn;
    uint32_t nlink;
    stat_dir_stamp(pst, &dev, &ino, &nlink, &ms, &mn);
    uint32_t bucket = case_bucket_index(dev, ino, h);

    while (atomic_flag_test_and_set_explicit(&g_case_cache_lock, memory_order_acquire)) {}
    int hit = 0;
    for (uint32_t s = 0; s < CASE_CACHE_PROBE; s++) {
        case_cache_entry *e = &g_case_cache[(bucket + s) & (CASE_CACHE_SIZE - 1u)];
        if (e->state != 0 && e->gen == gen && e->hash == h && e->comp_len == (uint16_t)comp_len &&
            e->dev == dev && e->ino == ino && e->nlink == nlink &&
            e->mtime_sec == ms && e->mtime_nsec == mn &&
            memcmp(e->query_ci, ci, comp_len) == 0) {
            e->stamp = ++g_case_cache_clock;
            if (e->state == 1) {
                hit = -1;
            } else {
                memcpy(actual_out, e->actual, e->actual_len + 1u);
                *actual_len_out = e->actual_len;
                hit = 1;
            }
            break;
        }
    }
    atomic_flag_clear_explicit(&g_case_cache_lock, memory_order_release);
    return hit;
}

static void case_cache_store(uint64_t gen, const struct stat *pst, const char *comp, size_t comp_len,
                             bool found, const char *actual, size_t actual_len)
{
    if (comp_len == 0 || comp_len >= 256) return;
    if (found && (actual_len == 0 || actual_len >= 256)) return;
    if (atomic_load_explicit(&g_path_cache_gen, memory_order_acquire) != gen) return;

    char ci[256];
    uint32_t h = case_comp_hash(comp, comp_len, ci);
    uint64_t dev, ino, ms, mn;
    uint32_t nlink;
    stat_dir_stamp(pst, &dev, &ino, &nlink, &ms, &mn);
    uint32_t bucket = case_bucket_index(dev, ino, h);

    while (atomic_flag_test_and_set_explicit(&g_case_cache_lock, memory_order_acquire)) {}
    case_cache_entry *victim = &g_case_cache[bucket];
    for (uint32_t s = 0; s < CASE_CACHE_PROBE; s++) {
        case_cache_entry *e = &g_case_cache[(bucket + s) & (CASE_CACHE_SIZE - 1u)];
        if (e->state == 0 || e->gen != gen) { victim = e; break; }
        if (e->hash == h && e->comp_len == (uint16_t)comp_len && e->dev == dev && e->ino == ino &&
            memcmp(e->query_ci, ci, comp_len) == 0) {
            victim = e;
            break;
        }
        if (e->stamp < victim->stamp) victim = e;
    }
    victim->gen = gen;
    victim->dev = dev;
    victim->ino = ino;
    victim->mtime_sec = ms;
    victim->mtime_nsec = mn;
    victim->nlink = nlink;
    victim->hash = h;
    victim->comp_len = (uint16_t)comp_len;
    victim->stamp = ++g_case_cache_clock;
    memcpy(victim->query_ci, ci, comp_len + 1u);
    if (found) {
        victim->state = 2;
        victim->actual_len = (uint16_t)actual_len;
        memcpy(victim->actual, actual, actual_len + 1u);
    } else {
        victim->state = 1;
        victim->actual_len = 0;
        victim->actual[0] = '\0';
    }
    atomic_flag_clear_explicit(&g_case_cache_lock, memory_order_release);
}
#endif

static bool path_case_resolve_bounded(char *path, size_t cap)
{
    if (!path || !path[0]) return false;
    if (cap && strnlen(path, cap) == cap) return false;
    if (access(path, 0) == 0) return true;
#ifndef _WIN32
    size_t len = strlen(path);
    /* Walk right-to-left to stop at the deepest already-existing ancestor in
     * 1-2 access() probes rather than scanning from '/' down. */
    size_t probe = len;
    while (probe > 0 && path[probe - 1] == '/') probe--;
    if (probe == 0) return false;
    size_t i = 0;
    while (probe > 0) {
        while (probe > 0 && path[probe - 1] != '/') probe--;
        while (probe > 0 && path[probe - 1] == '/') probe--;
        if (probe == 0) break;
        char saved = path[probe];
        path[probe] = '\0';
        int exists = access(path, 0);
        path[probe] = saved;
        if (exists == 0) {
            i = probe;
            break;
        }
    }

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
            if (start == 0) {
                snprintf(parent, sizeof(parent), ".");
            } else if (start == 1) {
                snprintf(parent, sizeof(parent), "/");
            } else {
                size_t p_len = start - 1;
                while (p_len > 1 && path[p_len - 1] == '/') p_len--;
                if (p_len >= sizeof(parent)) { path[i] = saved; return false; }
                snprintf(parent, sizeof(parent), "%.*s", (int)p_len, path);
            }

            struct stat pst;
            if (stat(parent, &pst) != 0 || !S_ISDIR(pst.st_mode)) {
                path[i] = saved;
                return false;
            }
            uint64_t gen = atomic_load_explicit(&g_path_cache_gen, memory_order_acquire);
            char actual[256];
            size_t actual_len = 0;
            int cached = case_cache_lookup(gen, &pst, path + start, comp_len, actual, &actual_len);
            if (cached < 0) {
                path[i] = saved;
                return false;
            }
            char resolved_buf[1024];
            const char *resolved_name = actual;
            bool found = (cached > 0);

            if (!found) {
                DIR *d = opendir(parent);
                if (d) {
                    struct dirent *de;
                    while ((de = readdir(d))) {
                        if (strcasecmp(de->d_name, path + start) == 0) {
                            actual_len = strlen(de->d_name);
                            if (actual_len < sizeof(resolved_buf)) {
                                memcpy(resolved_buf, de->d_name, actual_len + 1u);
                                resolved_name = resolved_buf;
                                found = true;
                            }
                            break;
                        }
                    }
                    closedir(d);
                }
                if (comp_len < 256) {
                    case_cache_store(gen, &pst, path + start, comp_len, found,
                                     found ? resolved_name : NULL, actual_len);
                }
            }
            if (!found) {
                path[i] = saved;
                return false;
            }
            if (actual_len == comp_len) {
                memcpy(path + start, resolved_name, comp_len);
            } else {
                if (cap && (actual_len > cap - 1 || len - comp_len > cap - 1 - actual_len)) {
                    path[i] = saved;
                    return false;
                }
                path[i] = saved;
                memmove(path + start + actual_len, path + i, strlen(path + i) + 1);
                memcpy(path + start, resolved_name, actual_len);
                i = start + actual_len;
                len = strlen(path);
                saved = path[i];
                path[i] = '\0';
            }
        }
        path[i] = saved;
    }
    return access(path, 0) == 0;
#else
    return false;
#endif
}

bool tl_path_case_resolve(char *path) { return path_case_resolve_bounded(path, 0); }

bool tl_path_case_resolve_parent(char *path, size_t cap)
{
    if (!path || !cap) return false;
    size_t len = strnlen(path, cap);
    if (!len || len == cap || len >= 1024 || path[len - 1] == '/' || strchr(path, '\\')) return false;
    /* This helper is for one missing leaf, not a path that traverses out of
     * the caller's selected root. Keep the legacy resolver's rules separate. */
    for (size_t i = 0; i < len;) {
        while (path[i] == '/') i++;
        size_t start = i;
        while (path[i] && path[i] != '/') i++;
        size_t n = i - start;
        if ((n == 1 && path[start] == '.') ||
            (n == 2 && path[start] == '.' && path[start + 1] == '.')) return false;
    }
    const char *slash = strrchr(path, '/');
    if (!slash || !slash[1] || access(path, 0) == 0) return false;
    char parent[1024], leaf[1024];
    size_t parent_len = (size_t)(slash - path);
    if (!parent_len) parent_len = 1; /* /new.log has the root as its parent. */
    memcpy(parent, path, parent_len); parent[parent_len] = 0;
    snprintf(leaf, sizeof(leaf), "%s", slash + 1);
    if (!path_case_resolve_bounded(parent, sizeof(parent))) return false;
    struct stat st;
    if (stat(parent, &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    size_t a = strlen(parent), b = strlen(leaf);
    bool root = a == 1 && parent[0] == '/';
    if (a >= cap || b > cap - a - 1 || (!root && b == cap - a - 1)) return false;
    char candidate[1024];
    int n = snprintf(candidate, sizeof(candidate), "%s%s%s", parent, root ? "" : "/", leaf);
    if (n < 0 || (size_t)n >= sizeof(candidate) || (size_t)n >= cap) return false;
    /* A differently-cased existing file is handled by the caller's normal
     * existing-file lookup; do not manufacture an alternate spelling. */
    char existing[1024]; memcpy(existing, candidate, (size_t)n + 1);
    if (path_case_resolve_bounded(existing, sizeof(existing))) return false;
    memcpy(path, candidate, (size_t)n + 1);
    return true;
}
