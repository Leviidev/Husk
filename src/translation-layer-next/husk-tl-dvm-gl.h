/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Helpers the generated GLES natives (husk-tl-dvm-gles20.c) use: GL functions through ANGLE, and Java arrays/buffers as pointers. */
#ifndef HUSK_TL_DVM_GL_H
#define HUSK_TL_DVM_GL_H

#include "husk-tl-dvm-internal.h"

typedef struct { const char *name, *sig; dvm_native_fn fn; } gl_native;
extern const gl_native k_gles20[];
extern const gl_native k_gles30[], k_gles31[], k_gles32[];

void *gl_fn(const char *name);
const char *gl_str(jobj *s);                    /* UTF-8; NULL for null */
void *gl_array(jobj *arr, int32_t offset);      /* the elements from offset, or NULL */
void *gl_buffer(jobj *buffer);                  /* a java.nio buffer's elements from its position, or NULL */

#endif
