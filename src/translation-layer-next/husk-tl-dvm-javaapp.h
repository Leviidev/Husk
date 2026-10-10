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
    const char *framework_res;       /* the platform's resources (framework-res.apk): its resources.arsc, layouts, drawables */
    /* the host's side of the views: its keyboard for a focused text field (input type and IME options as EditorInfo has them),
       the clipboard (get returns a malloc'd string or NULL), the share sheet, the screen orientation the activity asks for */
    void (*show_keyboard)(int show, int input_type, int ime_options);
    void (*set_clipboard)(const char *utf8);
    char *(*get_clipboard)(void);
    void (*share)(const char *utf8);
    void (*set_orientation)(int android_orientation);
    int insets[4];                   /* left, top, right, bottom: the screen's notch and home indicator, in pixels */
    bool night_mode;                 /* the phone is in dark mode: the app's configuration says UI_MODE_NIGHT_YES */
} tl_javaapp_config;

bool tl_javaapp_manifest(const char *apk, char *pkg, size_t pn, char *activity, size_t an, char *application, size_t apn);
bool tl_javaapp_start(const tl_javaapp_config *cfg);
void tl_javaapp_touch(int phase, int id, float x, float y);    /* surface pixels; phase 0 down, 1 move, 2 up, 3 cancel */
void tl_javaapp_key(int android_keycode, bool down);
void tl_javaapp_back(void);
void tl_javaapp_set_paused(bool paused);
/* The host keyboard's typing, to the focused text field: text, one delete before the cursor, the editor action (return), the
   keyboard closed by the host. */
void tl_javaapp_text(const char *utf8);
void tl_javaapp_text_delete(void);
void tl_javaapp_text_action(void);
void tl_javaapp_keyboard_closed(void);
/* The screen's insets changed (rotation), or the keyboard now covers ime_bottom pixels of it. */
void tl_javaapp_set_insets(int left, int top, int right, int bottom, int ime_bottom);
unsigned long tl_javaapp_frames(void);
bool tl_javaapp_ended(void);

#endif
