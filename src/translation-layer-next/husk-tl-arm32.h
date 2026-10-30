/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ARM32 (armeabi / armeabi-v7a) on the same translation layer as arm64.
 *
 * A modern iPhone cannot execute AArch32. The loader maps the ELF32 image into a
 * 32-bit guest address space (256 MiB) and interprets ARM/Thumb. Imports (libc, JNI)
 * are shims with 32-bit pointers turned into handles.
 */
#ifndef HUSK_TL_ARM32_H
#define HUSK_TL_ARM32_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tl_lib tl_lib;
typedef struct jobj jobj;
typedef union jvalue jvalue;
typedef struct dvm_method dvm_method;

bool tl_a32_is_elf(const uint8_t *file, size_t n);
/* Fill L (already zeroed) from an ELF32 ARM image. Relocates against libc/JNI shims. */
bool tl_a32_map(tl_lib *L, const char *name, uint8_t *file, size_t flen);
void *tl_a32_sym(tl_lib *L, const char *name);          /* tagged host pointer, or NULL */
bool tl_a32_is_fn(const void *p);
uint32_t tl_a32_va(const void *p);
bool tl_a32_init(tl_lib *L);                             /* DT_INIT / init_array, translated */
int32_t tl_a32_call(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
int32_t tl_a32_jni_onload(tl_lib *L);
bool tl_a32_jni_native(void *fn, dvm_method *m, jobj *self, const jvalue *params, jvalue *ret);
bool tl_a32_active(void);                                /* a 32-bit library is loaded this session */
void tl_a32_reset(void);                                 /* drop the guest map so another APK can load */
uint32_t tl_a32_alloc(uint32_t n);                       /* guest heap; 0 if the map is full */
uint8_t *tl_a32_host(uint32_t va);
uint32_t tl_a32_intern_host(void *p);                    /* host pointer as a 32-bit handle */
void *tl_a32_hostptr(uint32_t h);
uint32_t tl_a32_env_va(void);
uint32_t tl_a32_vm_va(void);
uint32_t tl_a32_ref(jobj *o);

#ifdef __cplusplus
}
#endif

#endif
