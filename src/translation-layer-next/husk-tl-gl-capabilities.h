/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_GL_CAPABILITIES_H
#define HUSK_TL_GL_CAPABILITIES_H
#include <string.h>

/* Read the unwrapped current-context version, never the requested minimum or
 * a guest-visible compatibility string. GL_VERSION is valid in ES 1/2 too;
 * querying GL_MAJOR_VERSION on an ES 2 context would change its error state. */
static inline int tl_gl_es_version_major(const char *version)
{
    if (!version || strncmp(version, "OpenGL ES", 9)) return 0;
    const char *at = version + 9;
    if (!strncmp(at, "-CM", 3) || !strncmp(at, "-CL", 3)) at += 3;
    if (*at++ != ' ') return 0;
    if (at[0] < '1' || at[0] > '9' || at[1] != '.') return 0;
    return at[0] - '0';
}
#endif
