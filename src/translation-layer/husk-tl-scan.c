/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * One app, looked at before anything tries to run it: which ABIs it ships,
 * how much Dex it carries, what engine it was made with, and for each arm64
 * library, what the loader will have to do to map it.
 *
 * The answer lands in the UI as a verdict and a sentence, and in the log as
 * the full JSON, so that an app that later fails can be compared with what
 * was predicted for it.
 */
#include "husk-tl.h"
#include "husk-tl-internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* No native library is anywhere near this; a bigger entry is not one. */
#define LIB_LIMIT       ((size_t)1 << 31)

static const char *const kAbis[] = {
    "arm64-v8a", "armeabi-v7a", "armeabi", "x86_64", "x86", "riscv64", "mips64", "mips",
};
#define NABIS (sizeof(kAbis) / sizeof(kAbis[0]))
#define ABI_ARM64 0

/* "lib/<abi>/<name>.so" -> abi index and name; anything else -> -1. */
static int lib_abi(const char *path, const char **file)
{
    if (strncmp(path, "lib/", 4) != 0) {
        return -1;
    }
    const char *abi = path + 4;
    const char *slash = strchr(abi, '/');
    if (!slash || strchr(slash + 1, '/')) {
        return -1;
    }
    size_t flen = strlen(slash + 1);
    if (flen < 4 || strcmp(slash + 1 + flen - 3, ".so") != 0) {
        return -1;
    }
    for (size_t i = 0; i < NABIS; i++) {
        if ((size_t)(slash - abi) == strlen(kAbis[i])
            && strncmp(abi, kAbis[i], (size_t)(slash - abi)) == 0) {
            *file = slash + 1;
            return (int)i;
        }
    }
    return -1;
}

static bool is_dex(const char *name)
{
    if (strncmp(name, "classes", 7) != 0) {
        return false;
    }
    const char *p = name + 7;
    while (*p >= '0' && *p <= '9') {
        p++;
    }
    return strcmp(p, ".dex") == 0;
}

/* Recognised by the libraries an engine always ships. */
typedef struct engine_scan {
    bool il2cpp, unity, mono_unity, flutter, react, dotnet, godot, unreal, cocos, gdx;
} engine_scan;

static void note_engine(engine_scan *s, const char *f)
{
    if (!strcmp(f, "libil2cpp.so")) s->il2cpp = true;
    else if (!strcmp(f, "libunity.so")) s->unity = true;
    else if (!strcmp(f, "libmonobdwgc-2.0.so") || !strcmp(f, "libmono.so")) s->mono_unity = true;
    else if (!strcmp(f, "libflutter.so")) s->flutter = true;
    else if (!strcmp(f, "libreactnativejni.so") || !strcmp(f, "libhermes.so")) s->react = true;
    else if (!strcmp(f, "libmonosgen-2.0.so") || !strcmp(f, "libmonodroid.so")) s->dotnet = true;
    else if (!strcmp(f, "libgodot_android.so")) s->godot = true;
    else if (!strcmp(f, "libUE4.so") || !strcmp(f, "libUnreal.so")) s->unreal = true;
    else if (!strncmp(f, "libcocos", 8)) s->cocos = true;
    else if (!strcmp(f, "libgdx.so")) s->gdx = true;
}

static const char *engine_name(const engine_scan *s)
{
    if (s->il2cpp) return "Unity (IL2CPP)";
    if (s->unity) return s->mono_unity ? "Unity (Mono)" : "Unity";
    if (s->flutter) return "Flutter";
    if (s->react) return "React Native";
    if (s->dotnet) return ".NET / Xamarin";
    if (s->godot) return "Godot";
    if (s->unreal) return "Unreal Engine";
    if (s->cocos) return "Cocos";
    if (s->gdx) return "libGDX";
    return NULL;
}

static const char *lib_status(const tl_elf_report *r)
{
    if (!r->ok || r->relocs_unsupported) return "blocked";
    if (r->pages_conflict || r->svc || r->textrel || r->tpidr_writes) return "work";
    return "ok";
}

static void note(tl_json *j, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void note(tl_json *j, const char *fmt, ...)
{
    char buf[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tl_json_string(j, buf);
}

static void write_notes(tl_json *j, const tl_elf_report *r)
{
    tl_json_key(j, "notes");
    tl_json_begin_array(j);
    if (!r->ok) {
        tl_json_string(j, r->error);
    } else {
        if (r->pages_conflict) {
            note(j, "%u page%s would have to be executable and writable at once: it was "
                    "built for 4 KiB pages, so code and data share 16 KiB ones. Needs write "
                    "emulation in the loader, or a build with 16 KiB alignment.",
                 r->pages_conflict, r->pages_conflict == 1 ? "" : "s");
        } else if (r->max_align < 16384) {
            tl_json_string(j, "Built for 4 KiB pages, but no 16 KiB page mixes code with "
                              "writable data, so it still maps.");
        }
        if (r->svc) {
            note(j, "Makes %u direct Linux system call%s, which iOS would not understand; "
                    "each needs patching at load time.", r->svc, r->svc == 1 ? "" : "s");
        }
        if (r->tpidr_writes) {
            note(j, "Sets the thread register itself in %u place%s.", r->tpidr_writes,
                 r->tpidr_writes == 1 ? "" : "s");
        }
        if (r->textrel) {
            tl_json_string(j, "Patches its own code as it loads (text relocations).");
        }
        if (r->relocs_unsupported) {
            note(j, "%u relocation%s of a type the loader will not handle (first: %u).",
                 r->relocs_unsupported, r->relocs_unsupported == 1 ? "" : "s",
                 r->first_unsupported);
        }
    }
    tl_json_end_array(j);
}

static void write_library(tl_json *j, const char *apk, int abi, const char *file,
                          const tl_zip_entry *e, const tl_elf_report *r)
{
    tl_json_begin_object(j);
    tl_json_key(j, "name");         tl_json_string(j, file);
    tl_json_key(j, "abi");          tl_json_string(j, kAbis[abi]);
    tl_json_key(j, "apk");          tl_json_string(j, apk);
    tl_json_key(j, "bytes");        tl_json_int(j, (long long)e->usize);
    tl_json_key(j, "compressed");   tl_json_bool(j, e->method != 0);
    tl_json_key(j, "status");       tl_json_string(j, lib_status(r));
    write_notes(j, r);
    if (r->ok) {
        tl_json_key(j, "maxAlign");         tl_json_int(j, (long long)r->max_align);
        tl_json_key(j, "pages");            tl_json_int(j, r->pages);
        tl_json_key(j, "execPages");        tl_json_int(j, r->pages_exec);
        tl_json_key(j, "writePages");       tl_json_int(j, r->pages_write);
        tl_json_key(j, "conflictPages");    tl_json_int(j, r->pages_conflict);
        tl_json_key(j, "layout");           tl_json_string(j, r->layout);
        tl_json_key(j, "svc");              tl_json_int(j, r->svc);
        tl_json_key(j, "tpidrReads");       tl_json_int(j, r->tpidr_reads);
        tl_json_key(j, "tpidrWrites");      tl_json_int(j, r->tpidr_writes);
        tl_json_key(j, "tls");              tl_json_bool(j, r->has_tls);
        tl_json_key(j, "textrel");          tl_json_bool(j, r->textrel);
        tl_json_key(j, "relocations");      tl_json_int(j, r->relocs);
        tl_json_key(j, "tlsRelocations");   tl_json_int(j, r->relocs_tls);
        tl_json_key(j, "unsupportedRelocations"); tl_json_int(j, r->relocs_unsupported);
        tl_json_key(j, "packing");          tl_json_string(j, r->packing);
        tl_json_key(j, "imports");          tl_json_int(j, r->imports);
        tl_json_key(j, "soname");           tl_json_string(j, r->soname[0] ? r->soname : NULL);
        tl_json_key(j, "needed");
        tl_json_begin_array(j);
        for (int i = 0; i < r->needed_count; i++) {
            tl_json_string(j, r->needed[i]);
        }
        tl_json_end_array(j);
    }
    tl_json_end_object(j);
}

/* A library another one needs that the app does not carry itself: Husk has
 * to provide it. Kept as a small set of names. */
typedef struct nameset {
    char names[96][64];
    int  count;
} nameset;

static void nameset_add(nameset *s, const char *name)
{
    for (int i = 0; i < s->count; i++) {
        if (!strcmp(s->names[i], name)) return;
    }
    if (s->count < (int)(sizeof(s->names) / sizeof(s->names[0]))) {
        snprintf(s->names[s->count++], sizeof(s->names[0]), "%s", name);
    }
}

static bool nameset_has(const nameset *s, const char *name)
{
    for (int i = 0; i < s->count; i++) {
        if (!strcmp(s->names[i], name)) return true;
    }
    return false;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

char *husk_tl_scan(const char *const *paths, int count)
{
    tl_json j;
    tl_json_init(&j);
    tl_json_begin_object(&j);

    bool abis[NABIS] = { false };
    long long dex_count = 0, dex_bytes = 0;
    bool manifest = false;
    int arm64_libs = 0, arm64_trouble = 0;
    engine_scan eng;
    memset(&eng, 0, sizeof(eng));
    nameset *own = calloc(1, sizeof(*own)), *needed = calloc(1, sizeof(*needed));
    tl_elf_report *rep = malloc(sizeof(*rep));
    char error[256] = "";

    if (!own || !needed || !rep) {
        snprintf(error, sizeof(error), "out of memory");
    }

    /* The libraries first, while the facts about the whole app accumulate;
     * the summary is written after them. */
    tl_json_key(&j, "libraries");
    tl_json_begin_array(&j);
    for (int p = 0; p < count && !error[0]; p++) {
        tl_zip z;
        char zerr[160] = "";
        if (!tl_zip_open(&z, paths[p], zerr, sizeof(zerr))) {
            snprintf(error, sizeof(error), "%s: %s", base_name(paths[p]), zerr);
            break;
        }
        for (size_t i = 0; i < z.count; i++) {
            const tl_zip_entry *e = &z.entries[i];
            const char *file = NULL;
            int abi = lib_abi(e->name, &file);
            if (!strcmp(e->name, "AndroidManifest.xml")) {
                manifest = true;
            } else if (is_dex(e->name)) {
                dex_count++;
                dex_bytes += (long long)e->usize;
            }
            if (abi < 0) {
                continue;
            }
            abis[abi] = true;
            note_engine(&eng, file);
            if (abi != ABI_ARM64) {
                continue;
            }
            nameset_add(own, file);

            const uint8_t *data;
            size_t len;
            bool owned;
            char derr[160] = "";
            if (tl_zip_data(&z, e, LIB_LIMIT, &data, &len, &owned, derr, sizeof(derr))) {
                tl_elf_analyze(data, len, rep);
                if (owned) free((void *)data);
            } else {
                memset(rep, 0, sizeof(*rep));
                rep->packing = "none";
                snprintf(rep->error, sizeof(rep->error), "%s", derr);
            }
            for (int n = 0; n < rep->needed_count; n++) {
                nameset_add(needed, rep->needed[n]);
            }
            arm64_libs++;
            if (strcmp(lib_status(rep), "ok") != 0) {
                arm64_trouble++;
            }
            write_library(&j, base_name(paths[p]), abi, file, e, rep);
        }
        tl_zip_close(&z);
    }
    tl_json_end_array(&j);

    bool any_native = false;
    for (size_t i = 0; i < NABIS; i++) {
        any_native = any_native || abis[i];
    }

    const char *verdict;
    char summary[320];
    if (error[0]) {
        verdict = "unreadable";
        snprintf(summary, sizeof(summary), "Could not be read: %s", error);
    } else if (!manifest && dex_count == 0) {
        verdict = "unreadable";
        snprintf(summary, sizeof(summary), "This is not an app: it has no manifest and no code.");
    } else if (!any_native) {
        verdict = "java";
        snprintf(summary, sizeof(summary), "No native code. Only the Java side of the "
                 "translation layer is needed to run it.");
    } else if (abis[ABI_ARM64] && arm64_trouble == 0) {
        verdict = "native";
        snprintf(summary, sizeof(summary), "arm64 native code, and every library maps "
                 "cleanly onto 16 KiB pages.");
    } else if (abis[ABI_ARM64]) {
        verdict = "nativeWithWork";
        snprintf(summary, sizeof(summary), "arm64 native code, but %d of %d librar%s "
                 "need%s extra work from the loader.", arm64_trouble, arm64_libs,
                 arm64_libs == 1 ? "y" : "ies", arm64_trouble == 1 ? "s" : "");
    } else {
        verdict = "noArm64";
        bool arm32 = abis[1] || abis[2], x86 = abis[3] || abis[4];
        if (arm32 && !x86) {
            snprintf(summary, sizeof(summary), "Native code for 32-bit ARM only. iPhones "
                     "cannot run 32-bit ARM code at all, so this would need a CPU emulator.");
        } else if (x86 && !arm32) {
            snprintf(summary, sizeof(summary), "Native code for x86 only, which would "
                     "need an x86 translator such as FEXCore.");
        } else {
            snprintf(summary, sizeof(summary), "Native code, but none of it for arm64.");
        }
    }

    tl_json_key(&j, "ok");          tl_json_bool(&j, !error[0]);
    tl_json_key(&j, "error");       tl_json_string(&j, error[0] ? error : NULL);
    tl_json_key(&j, "verdict");     tl_json_string(&j, verdict);
    tl_json_key(&j, "summary");     tl_json_string(&j, summary);
    tl_json_key(&j, "engine");      tl_json_string(&j, engine_name(&eng));
    tl_json_key(&j, "hasManifest"); tl_json_bool(&j, manifest);
    tl_json_key(&j, "dexCount");    tl_json_int(&j, dex_count);
    tl_json_key(&j, "dexBytes");    tl_json_int(&j, dex_bytes);
    tl_json_key(&j, "abis");
    tl_json_begin_array(&j);
    for (size_t i = 0; i < NABIS; i++) {
        if (abis[i]) tl_json_string(&j, kAbis[i]);
    }
    tl_json_end_array(&j);
    /* What the app's own libraries link against that it does not ship. */
    tl_json_key(&j, "systemLibraries");
    tl_json_begin_array(&j);
    if (needed && own) {
        for (int i = 0; i < needed->count; i++) {
            if (!nameset_has(own, needed->names[i])) tl_json_string(&j, needed->names[i]);
        }
    }
    tl_json_end_array(&j);
    tl_json_end_object(&j);

    free(own);
    free(needed);
    free(rep);
    return tl_json_finish(&j);
}

void *husk_tl_read_entry(const char *apk, const char *name, size_t limit,
                         size_t *out_len)
{
    *out_len = 0;
    tl_zip z;
    if (!tl_zip_open(&z, apk, NULL, 0)) {
        return NULL;
    }
    void *copy = NULL;
    const tl_zip_entry *e = tl_zip_find(&z, name);
    const uint8_t *data;
    size_t len;
    bool owned;
    if (e && tl_zip_data(&z, e, limit, &data, &len, &owned, NULL, 0)) {
        if (owned) {
            copy = (void *)data;
        } else if ((copy = malloc(len ? len : 1)) != NULL) {
            memcpy(copy, data, len);
        }
        if (copy) {
            *out_len = len;
        }
    }
    tl_zip_close(&z);
    return copy;
}

void husk_tl_free(void *p)
{
    free(p);
}
