/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Drives a game built on SDL3 the way SDL's Java shell (SDLActivity) does.
 *
 * An SDL game is a native library with a `SDL_main`, behind an activity that extends org.libsdl.app.SDLActivity. That class loads SDL3 and the game's
 * library, tells SDL about the screen and the surface through native calls on itself, and runs SDL_main on a thread of its own through nativeRunMain;
 * SDL calls back into the activity for the few things only Java can answer (the context, the surface, the touch devices, locale). This does the same
 * from C, against the Java world husk-tl-jni-hle.c and this file provide.
 */
#ifndef HUSK_TL_SDL_H
#define HUSK_TL_SDL_H

#include <stdbool.h>

#include "husk-tl-gameactivity.h"          /* tl_ga_config: the same description of the game and its surface */

#ifdef __cplusplus
extern "C" {
#endif

/* The game's activity class, a subclass of SDLActivity, as its manifest names it (JNI form: "com/vectorunit/cobalt/MainActivity"). */
bool tl_sdl_start(const tl_ga_config *cfg, const char *activity_class);

/* Another APK of the same app (a split, an asset pack): its libraries and assets are found along with the main one's. Call before tl_sdl_start. */
bool tl_sdl_add_package(const char *apk_path);
/* Launch arguments for the game (what SDLActivity.getArguments() returns), space-separated. Before tl_sdl_start. */
void tl_sdl_set_arguments(const char *args);

/* The surface and its lifecycle, then SDL_main on its own thread: the game takes it from there. */
bool tl_sdl_run(void);

/* A touch in surface pixels, y down. phase 0 down, 1 move, 2 up, 3 cancel. Safe from any thread. */
void tl_sdl_touch(int phase, int id, float x, float y);

void tl_sdl_set_paused(bool paused);

/* The keyboard: the handler is told (on the game's thread) 1 = show, 2 = hide. Text typed goes in with tl_sdl_commit_text, Backspace and Enter as Android key codes. */
/* True if the manifest asks for a portrait screen. */
bool tl_sdl_manifest_portrait(const char *apk);
int tl_manifest_orientation(const char *apk);
bool tl_manifest_is_game(const char *apk);   /* 1 portrait, 0 landscape, -1 unspecified */
void tl_sdl_mouse(int phase, float x, float y);
void tl_sdl_set_keyboard_handler(void (*handler)(int action));
void tl_sdl_commit_text(const char *utf8);
void tl_sdl_key(int android_keycode, bool down);

/* The screen's safe-area insets (a notch, rounded corners) in surface pixels, as the activity would report them from its display cutout. */
void tl_sdl_set_safe_insets(int left, int top, int right, int bottom);
unsigned long tl_sdl_frames(void);

#ifdef __cplusplus
}
#endif

#endif
