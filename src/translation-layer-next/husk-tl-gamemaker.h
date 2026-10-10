/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Drives a GameMaker game's runner (libyoyo.so) the way GameMaker's Android activity does.
 *
 * A GameMaker export is YoYo's runner library and the game in the APK's assets (game.droid, which the runner reads out of
 * the APK itself). The Java side -- com.yoyogames.runner.RunnerJNILib and the activity's DemoRenderer -- loads the library,
 * resolves GL with initGLFuncs, and on a GLSurfaceView's thread calls Startup() once and then Process() every frame, until
 * canFlip() says the frame is ready to swap. Input arrives as TouchEvent/KeyEvent calls. This does the same from C, with
 * the Java the runner calls back into answered in husk-tl-gamemaker.c too.
 */
#ifndef HUSK_TL_GAMEMAKER_H
#define HUSK_TL_GAMEMAKER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tl_gm_config {
    const char *apk_path;
    const char *data_dir;        /* writable app data directory */
    const char *package_name;
    int width, height;           /* surface size in pixels */
    void *metal_layer;           /* CAMetalLayer to present into, or NULL (offscreen/host) */
    const char *angle_egl;
    const char *angle_gles;
    const char *frame_dir;       /* host tests: write frames here instead of presenting */
    int frame_every;
} tl_gm_config;

/* Whether an APK is a GameMaker game: YoYo's runner for arm64. */
bool tl_gm_is_game(const char *apk_path);

bool tl_gm_start(const tl_gm_config *cfg);
bool tl_gm_run(void);
unsigned long tl_gm_frames(void);
bool tl_gm_ended(void);
void tl_gm_set_paused(bool paused);

/* A touch in surface pixels, y down. phase 0 down, 1 move, 2 up, 3 cancel. Safe from any thread. */
void tl_gm_touch(int phase, int id, float x, float y);
/* An Android key code, pressed and released (4 is Back). */
void tl_gm_key(int keycode, bool down);

typedef struct tl_gm_perf { double fps, mean_ms, max_ms; } tl_gm_perf;
void tl_gm_perf_snapshot(tl_gm_perf *out);

/* Which way up the game wants to be, from its options.ini: 1 portrait, 0 landscape, -1 either. */
int tl_gm_orientation(const char *apk_path);

#ifdef __cplusplus
}
#endif

#endif
