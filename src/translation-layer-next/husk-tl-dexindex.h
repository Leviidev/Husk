/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * A light index over an APK's DEX files: which classes it defines, and which methods
 * and fields each declares.
 *
 * The JNI layer needs Android's answer to "does this class exist" and "does it have
 * this method". Unity asks both for optional components -- ARCore, a camera wrapper,
 * a plugin that may not be bundled -- and treats a thrown NoClassDefFoundError as
 * "not available". An index that said yes to everything would have it call into
 * things that are not there. The full interpreter's loader (husk-tl-dex.c) builds a
 * linked class model for running code; this only reads names, and reads them lazily.
 */
#ifndef HUSK_TL_DEXINDEX_H
#define HUSK_TL_DEXINDEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Index every classes*.dex in the APK. Returns the number of classes, or -1. */
int tl_dexidx_open(const char *apk_path);

/* `name` in JNI form: "com/unity3d/player/UnityPlayer". */
bool tl_dexidx_has_class(const char *name);

/* The superclass in JNI form, or NULL (Object, or not defined here). */
const char *tl_dexidx_super(const char *name, char *buf, size_t n);

/* Whether `cls` declares a method/field with this name and JNI signature ("" sig matches any). */
bool tl_dexidx_declares_method(const char *cls, const char *name, const char *sig, bool *is_static);
bool tl_dexidx_declares_field(const char *cls, const char *name, const char *sig, bool *is_static);

/* The declared signature of a field by name, or whether any method has this name, ignoring signatures. */
bool tl_dexidx_field_sig(const char *cls, const char *name, char *out, size_t n);
bool tl_dexidx_method_named(const char *cls, const char *name);
/* The signature of the first method of that name (for a method whose signature changes between versions of a library). */
bool tl_dexidx_method_sig(const char *cls, const char *name, char *out, size_t n, bool *is_static);
bool tl_dexidx_find_method_lenient(const char *cls, const char *name, const char *want, char *out, size_t n, bool *is_static);

/* Calls `fn` with every string constant in the APK's DEX files, until it returns false. */
void tl_dexidx_each_string(bool (*fn)(const char *s, void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_DEXINDEX_H */
