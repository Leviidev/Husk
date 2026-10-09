/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * What the app calls to run a Unity game through the native runtime.
 *
 * Starting is asynchronous: the engine takes seconds to load, and the UI must stay up
 * meanwhile. The progress is in the log (husk_tl_attempt_log) and in husk_unity_state.
 * An engine cannot be unloaded, so a game that has been started stays loaded for the life
 * of the process; leaving its screen pauses it.
 */
#ifndef HUSK_TL_UNITY_APP_H
#define HUSK_TL_UNITY_APP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { HUSK_UNITY_IDLE = 0, HUSK_UNITY_STARTING = 1, HUSK_UNITY_RUNNING = 2, HUSK_UNITY_FAILED = 3, HUSK_UNITY_ENDED = 4 };

/*
 * Start the game in `apk`. `metal_layer` is the CAMetalLayer it draws into, `width`/`height` its
 * size in pixels. `angle_dylib` is the bundled libANGLE-shared.dylib; `ca_bundle` a PEM file of root
 * certificates. Returns false if a game is already started or the arguments are unusable.
 */
bool husk_unity_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                       const char *angle_dylib, const char *ca_bundle);

/* The same for a cocos2d-x game (Geometry Dash): a landscape surface, with sound. Status, touch and pause go through the calls below. */
/* Load Geode (the Geometry Dash mod loader) into the next cocos2d-x game: its release zip and its Android launcher's APK.
 * NULL turns it off. Call before husk_cocos_launch. */
void husk_cocos_set_geode(const char *release_zip, const char *launcher_apk);
bool husk_cocos_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                       const char *angle_dylib, const char *ca_bundle);

/* The same for a game built on Google's GameActivity (Minecraft): a landscape surface, with sound and multi-touch. */
bool husk_gameactivity_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                              const char *angle_dylib, const char *ca_bundle);

/*
 * The same for a game built on SDL3 (Beach Buggy Racing 2): landscape, sound, multi-touch. Its libraries may be in a split, and its data in an asset pack, so every
 * other APK of the app is given first with husk_native_add_package. The game's activity is the launcher activity of the manifest.
 */
bool husk_sdl_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                     const char *angle_dylib, const char *ca_bundle);
/*
 * The same for a game built on Unreal Engine 4 (Minecraft Dungeons): landscape, sound, touch and controllers. Its renderer is Vulkan, on MoltenVK: give the path
 * of libMoltenVK.dylib with husk_ue4_set_vulkan first. Its data is an OBB the APK carries inside itself, mapped in place.
 */
bool husk_ue4_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                     const char *angle_dylib, const char *ca_bundle);
/* A Godot game (libgodot_android.so: Godot 3 and 4, with GLES2/GLES3 or the Compatibility renderer): sound, multi-touch. */
bool husk_godot_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                       const char *angle_dylib, const char *ca_bundle);
/* Rockstar's GTA San Andreas (libGame.so behind the oswrapper): landscape, sound, touch and controllers. Its data is inside the APK. */
bool husk_gta_launch(const char *apk, const char *data_dir, void *metal_layer, int width, int height,
                     const char *angle_dylib, const char *ca_bundle);
void husk_ue4_set_vulkan(const char *dylib);
/* Another APK of the app about to be launched (a split, an asset pack). Before the launch call; at most three. */
void husk_native_add_package(const char *apk);
/* The folder games see as Android's shared storage (/sdcard outside their own Android/data), or NULL for each game's own. */
void husk_native_set_shared_storage(const char *dir);
/* The screen's safe-area insets in pixels, for a game that keeps its controls out of a notch (SDL games ask). Before the launch call, or any time. */
void husk_sdl_set_safe_insets(int left, int top, int right, int bottom);
/* Whether an SDL game's manifest asks for a portrait screen (the others are landscape). */
int husk_sdl_apk_is_portrait(const char *apk);
/* Soft keyboard for an SDL game: the handler is told 1 = show, 2 = hide; typed text goes in with husk_sdl_commit_text, Backspace (67) and Enter (66) with husk_sdl_key. */
void husk_sdl_set_keyboard_handler(void (*handler)(int action));
void husk_sdl_commit_text(const char *utf8);
void husk_sdl_key(int keycode, int down);
/* Soft keyboard for a cocos2d-x game. The handler is told (on the game's GL thread) 0 = toggle, 1 = show, 2 = hide. */
void husk_cocos_set_keyboard_handler(void (*handler)(int action));
/* Soft keyboard for a GameActivity game (Minecraft): 1 = show, 2 = hide; the field's text is kept on the C side. */
void husk_ga_set_keyboard_handler(void (*handler)(int action));
void husk_ga_insert_text(const char *utf8);
void husk_ga_delete_backward(void);
void husk_ga_editor_action(void);
void husk_ga_text(char *out, unsigned long cap);
void husk_cocos_insert_text(const char *utf8);
void husk_cocos_delete_backward(void);
void husk_cocos_key_down(int keycode);
void husk_cocos_request_text(void (*cb)(const char *utf8));    /* what the game's text field holds now; cb runs on the GL thread */

/* A link the game wants opened (terms of use, social buttons). The handler runs on the game's GL thread. */
void husk_cocos_set_open_url_handler(void (*handler)(const char *url));

/*
 * Game controllers (GameController on iOS, so Bluetooth ones too). Report a controller when it connects and its state as often as it
 * changes; what changed is sent to whichever game is running, as the Xbox-style controller Android games expect. `buttons` is a mask of
 * 1 << TL_PAD_* (A, B, X, Y, L1, R1, left and right stick click, Start, Select, Mode, then D-pad up, down, left, right) -- see
 * husk-tl-gamepad.h, or HuskGamepad.swift for the same list. Sticks are -1..1 with y up, as iOS reports them; triggers 0..1.
 */
void husk_gamepad_connect(int slot, const char *name);
void husk_gamepad_disconnect(int slot);
void husk_gamepad_update(int slot, unsigned buttons, float lx, float ly, float rx, float ry, float lt, float rt);

/* The APK of the game started this session, or NULL. An engine cannot be loaded twice, nor two games at once. */
const char *husk_native_loaded_apk(void);

/* Whether the running game has no touch controls of its own and wants the on-screen controller (a PC game through DXVK). */
bool husk_native_wants_controller(void);

int  husk_unity_state(void);
unsigned long husk_unity_frames(void);
typedef struct husk_unity_perf { double fps, mean_ms, max_ms; } husk_unity_perf;
void husk_unity_perf_snapshot(husk_unity_perf *out);          /* since the last call */
void husk_unity_touch(int phase, int id, float x, float y);   /* phase 0 down, 1 move, 2 up, 3 cancel */
void husk_unity_set_paused(bool paused);

/* The app's package name from its manifest into `out`; false if it cannot be read. */
bool husk_unity_package_name(const char *apk, char *out, unsigned long out_len);

#ifdef __cplusplus
}
#endif

#endif
