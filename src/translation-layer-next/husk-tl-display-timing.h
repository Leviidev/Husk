/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_DISPLAY_TIMING_H
#define HUSK_TL_DISPLAY_TIMING_H
/* The Android-compatible virtual display and its choreographer currently run
 * at this cadence. Physical host-screen capability is a separate fact. */
enum { TL_JAVA_DISPLAY_HZ = 60 };
#define TL_JAVA_VSYNC_NS 16666667ll
#endif
