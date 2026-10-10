/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Android's libc (bionic) and friends, implemented over Darwin.
 *
 * Guest code binds against names, and the two ABIs agree on many of them, so the
 * symbol table has three kinds of entry:
 *
 *   DIRECT  Darwin's function used as it is: same signature, same layouts, same
 *           meaning. strlen, memcpy, sin.
 *   WRAP    ours, because something differs: a structure's layout (stat, dirent),
 *           a constant's value (O_CREAT, errno numbers, signal numbers), the size
 *           of a type the guest embeds in its own structures (pthread_mutex_t is 40
 *           bytes here and 64 there), or a calling convention (variadics).
 *   DATA    an object the guest reads or writes: __sF, _ctype_, environ.
 *
 * Nothing falls back to "whatever Darwin happens to export under that name". That
 * would hand Android code Darwin's C++ ABI and Darwin's idea of every flag.
 */
#ifndef HUSK_TL_BIONIC_H
#define HUSK_TL_BIONIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tl_bionic_entry {
    const char *name;
    void *addr;
} tl_bionic_entry;

#define TL_DIRECT(fn)       { #fn, (void *)(fn) }
#define TL_WRAP(name, impl) { name, (void *)(impl) }
#define TL_DATA(name, ptr)  { name, (void *)(ptr) }
#define TL_END              { NULL, NULL }

/* The tables, one per file. Each ends with TL_END. */
extern const tl_bionic_entry tl_tab_core[];
extern const tl_bionic_entry tl_tab_str[];
extern const tl_bionic_entry tl_tab_io[];
extern const tl_bionic_entry tl_tab_net[];
extern const tl_bionic_entry tl_tab_io2[];
extern const tl_bionic_entry tl_tab_str2[];
extern const tl_bionic_entry tl_tab_pthread[];
extern const tl_bionic_entry tl_tab_ndk[];
extern const tl_bionic_entry tl_tab_egl[];
extern const tl_bionic_entry tl_tab_cxx[];
extern const tl_bionic_entry tl_tab_opensles[];
extern const tl_bionic_entry tl_tab_sys[];
extern const tl_bionic_entry tl_tab_ldbl[];
void *tl_egl_resolve(const char *name);   /* husk-tl-egl.c: GLES by name, through ANGLE */
void *tl_vk_resolve(const char *name);    /* husk-tl-vulkan.m: Vulkan by name, through MoltenVK */
bool tl_vk_available(void);                /* a MoltenVK was configured, so libvulkan.so can be opened */

extern void (*tl_guest_exit_hook)(int status);   /* set by an app host: exit() from guest code calls it instead of exiting */

void *tl_bionic_find(const char *name);
bool  tl_bionic_is_system_lib(const char *soname);

/* ----------------------------------------------------------------- errno */

/*
 * The guest's errno is its own per-thread integer, holding Linux numbers. Darwin's
 * errno holds Darwin numbers, and the two diverge past 34 (EAGAIN is 11 there and
 * 35 here). A wrapper clears Darwin's errno, calls, and publishes whatever the call
 * set -- translated -- only when it set something, so success leaves the guest's
 * errno alone.
 */
int  tl_errno_to_guest(int darwin_errno);
int  tl_errno_from_guest(int guest_errno);
int *tl_guest_errno_ptr(void);
void tl_set_guest_errno(int guest_errno);

#define TL_ERRNO_BEGIN()  (errno = 0)
#define TL_ERRNO_END()    do { if (errno) tl_set_guest_errno(tl_errno_to_guest(errno)); } while (0)

/* Signal numbers: Linux <-> Darwin. 0 and negative on no equivalent. */
int tl_signal_to_darwin(int linux_sig);
int tl_signal_from_darwin(int darwin_sig);

/* Android system properties, for __system_property_get and Java's Build. */
const char *tl_sysprop(const char *name);

/* The log. */
void tl_log_line(const char *fmt, ...);

/* A one-line note, once per distinct text, for things that are stubbed. */
void tl_note_once(const char *what);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_BIONIC_H */
