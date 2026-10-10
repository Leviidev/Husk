/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Flutter apps on the translation layer: the engine (libflutter.so) and the app's compiled Dart (libapp.so) run natively; the thin
 * Java embedding around them (FlutterActivity, FlutterJNI, the platform channels) is answered here.
 */
#ifndef HUSK_TL_FLUTTER_H
#define HUSK_TL_FLUTTER_H

#include <stdbool.h>
#include "husk-tl-gameactivity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Whether an APK is a Flutter app: it ships libflutter.so for arm64. */
bool tl_flutter_is_app(const char *apk_path);

bool tl_flutter_start(const tl_ga_config *cfg);   /* load the engine and the app; cfg->width/height is the surface in pixels */
bool tl_flutter_run(void);                         /* start the platform thread: the engine, the surface, the Dart entry point */
unsigned long tl_flutter_frames(void);
bool tl_flutter_first_frame(void);                 /* the engine has said its first frame is up (FlutterJNI.onFirstFrame) */

/* The app going to the background (paused) or coming back. */
void tl_flutter_set_paused(bool paused);

/* Input: phase 0 down, 1 move, 2 up, 3 cancel; x, y in surface pixels. */
void tl_flutter_touch(int phase, int id, float x, float y);

/* The device's pixels per logical pixel (the phone's scale): set before tl_flutter_run. */
void tl_flutter_set_pixel_ratio(float ratio);
/* Safe-area insets in surface pixels (notch, home indicator): set before tl_flutter_run, or any time after. */
void tl_flutter_set_insets(int top, int right, int bottom, int left);
/* The view's new size in pixels (a window being resized): any time after tl_flutter_run. */
void tl_flutter_resize(int width, int height);
/* The keyboard: typed text, backspace, Return; and how much of the view it covers (pixels from the bottom). */
void tl_flutter_key_insert(const char *utf8);
void tl_flutter_key_delete(void);
void tl_flutter_key_action(void);
void tl_flutter_set_keyboard_inset(int bottom);
/* Android's back button. */
void tl_flutter_back(void);
/* Called when the app closes itself (back on its first screen). */
void tl_flutter_set_close_handler(void (*fn)(void));

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_FLUTTER_H */
