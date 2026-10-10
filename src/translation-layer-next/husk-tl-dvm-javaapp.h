/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * A Java app (no engine library of its own to drive, or one driven from Java, such as libGDX) run on Husk's Dalvik runtime with
 * Husk's Java framework: the launcher activity is created and its lifecycle run by husk.AppRunner, a GLSurfaceView's renderer draws
 * on Husk's GL thread into the app's CAMetalLayer through ANGLE, and touches are queued here for the app's main thread.
 * The runtime must already be started (tl_dvm_start + tl_dvm_load_natives) with husk-framework.dex on the boot class path.
 */
#ifndef HUSK_TL_DVM_JAVAAPP_H
#define HUSK_TL_DVM_JAVAAPP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct tl_javaapp_config {
    const char *apk_path;
    const char *data_dir;            /* the app's private folder */
    const char *package_name;        /* NULL: the manifest's */
    int width, height;               /* the screen, in pixels */
    float density;                   /* pixels per dp */
    void *metal_layer;
    const char *angle_egl, *angle_gles;
    const char *frame_dir;           /* host tests: frames written here */
    int frame_every;
    void (*vibrate)(int ms);
    void (*open_url)(const char *url);
} tl_javaapp_config;

bool tl_javaapp_manifest(const char *apk, char *pkg, size_t pn, char *activity, size_t an);
bool tl_javaapp_start(const tl_javaapp_config *cfg);
void tl_javaapp_touch(int phase, int id, float x, float y);    /* surface pixels; phase 0 down, 1 move, 2 up, 3 cancel */
void tl_javaapp_key(int android_keycode, bool down);
void tl_javaapp_back(void);
void tl_javaapp_set_paused(bool paused);
unsigned long tl_javaapp_frames(void);
bool tl_javaapp_ended(void);

#endif
