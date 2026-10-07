/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The Java a particular SDL game adds around SDL, implemented in C.
 *
 * SDL games are mostly one native library, but some put an Android interface around it (a start menu, a dialog for a name, a loading overlay) that the library calls
 * into and waits on. Each is a handful of methods; they are kept here, by package, and installed only for that game.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-dexindex.h"
#include "husk-tl-ld.h"

jobj *tl_assetstream_list(const char *path);
void tl_sdl_display_metrics(tl_jcall *c);
jobj *tl_assetstream_file_tree(void);
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static const char *Str(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }
static void H_void(tl_jcall *c) { (void)c; }
static void H_false(tl_jcall *c) { c->ret = vz(0); }

/* A native of the game's own library, by its JNI name. */
static void *game_native(const char *mangled)
{
    static const char *const libs[] = { "libmain.so", NULL };
    (void)libs;
    void *fn = NULL;
    extern void *tl_sdl_game_symbol(const char *name);
    fn = tl_sdl_game_symbol(mangled);
    if (!fn) tl_log_line("sdl: the game has no native %s", mangled);
    return fn;
}

/* The answer Java gives later, from another thread, once its dialog is dismissed: here a moment after the question, from a thread of its own. */
struct answer { void (*fn)(void *env, void *self, int a); void *self; int a; void (*fn_text)(void *env, void *self, uint8_t ok, void *text); char text[200]; };
static void *deliver(void *arg)
{
    struct answer *r = arg;
    usleep(250000);
    void *env = tl_jni_env();
    if (r->fn) r->fn(env, r->self, r->a);
    else if (r->fn_text) r->fn_text(env, r->self, 1, tl_jni_new_string(r->text));
    free(r);
    return NULL;
}
static void answer_later(struct answer *r)
{
    pthread_t t;
    if (pthread_create(&t, NULL, deliver, r) == 0) pthread_detach(t); else free(r);
}

/* ---------------------------------------------------------------- Brogue CE */

#define BROGUE "org/broguece/game/BrogueActivity"
static void BR_showStartMenu(tl_jcall *c)                              /* (hasSavedGame, ...): New Game */
{
    struct answer *r = calloc(1, sizeof(*r));
    r->fn = game_native("Java_org_broguece_game_BrogueActivity_nativeStartMenuResult");
    r->self = tl_jni_ref(c->self); r->a = 0;
    if (!r->fn) { free(r); return; }
    tl_log_line("sdl: Brogue start menu: new game");
    answer_later(r);
}
static void BR_showTextInput(tl_jcall *c)                              /* (title, default, maxLength, ...): accept the default */
{
    struct answer *r = calloc(1, sizeof(*r));
    r->fn_text = game_native("Java_org_broguece_game_BrogueActivity_nativeTextInputResult");
    r->self = tl_jni_ref(c->self);
    snprintf(r->text, sizeof(r->text), "%s", c->args[1].l ? Str(c->args[1].l) : "");
    if (!r->fn_text) { free(r); return; }
    answer_later(r);
}
static void BR_getInt(tl_jcall *c) { c->ret = vi(c->args[1].i); }       /* a setting not stored: its default */
static const tl_jhle k_brogue[] = {
    { BROGUE, "showStartMenu", "(ZZ)V", BR_showStartMenu },
    { BROGUE, "showTextInputDialog", "(Ljava/lang/String;Ljava/lang/String;IZ)V", BR_showTextInput },
    { BROGUE, "setOverlayVisible", "(Z)V", H_void },
    { BROGUE, "setLoadingVisible", "(Z)V", H_void },
    { BROGUE, "setLoadingProgress", "(I)V", H_void },
    { BROGUE, "setRestoringVisible", "(Z)V", H_void },
    { BROGUE, "getSettingBool", "(Ljava/lang/String;)Z", H_false },
    { BROGUE, "getSettingInt", "(Ljava/lang/String;I)I", BR_getInt },
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------------------------------------- Pekka Kana 2 */

#define PK2 "org/pgnapps/pk2/PK2Activity"
static void PK_listDir(tl_jcall *c)                                    /* (path): the entries of an asset folder */
{
    jvalue r; r.j = 0; r.l = tl_assetstream_list(Str(c->args[0].l));
    c->ret = r;
}
static const tl_jhle k_pk2[] = {
    { PK2, "externalPermitted", "()Z", H_false },                      /* no shared storage: levels come from the app */
    { PK2, "listDir", "(Ljava/lang/String;)[Ljava/lang/String;", PK_listDir },
    { "org/libsdl/app/SDLActivity", "setSurfaceViewFormat", "(I)V", H_void },
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------------------------------------------- LOVE */

/* LOVE (love2d.org) on Android: its GameActivity adds a few screen settings and the message box LOVE shows when the game's Lua fails. */
#define LOVE "org/love2d/android/GameActivity"
static void LV_dpi(tl_jcall *c) { jvalue v; v.j = 0; v.f = 1.0f; c->ret = v; }                 /* the surface is in real pixels */
static void LV_messagebox(tl_jcall *c)                                                          /* (flags, title, message, ...): said in the log, no button pressed */
{
    tl_log_line("love: message box \"%s\": %s", c->args[1].l ? Str(c->args[1].l) : "", c->args[2].l ? Str(c->args[2].l) : "");
    c->ret = vi(0);
}
static void LV_buildFileTree(tl_jcall *c) { jvalue v; v.j = 0; v.l = tl_assetstream_file_tree(); c->ret = v; }
static const tl_jhle k_love[] = {
    { LOVE, "buildFileTree", "()[Ljava/lang/String;", LV_buildFileTree },
    { LOVE, "getMetrics", "()Landroid/util/DisplayMetrics;", tl_sdl_display_metrics },
    { LOVE, "getImmersiveMode", "()Z", H_false },
    { LOVE, "setImmersiveMode", "(Z)V", H_void },
    { LOVE, "getDPIScale", "()F", LV_dpi },
    { LOVE, "messageboxShowMessageBox", "(ILjava/lang/String;Ljava/lang/String;[I[I[Ljava/lang/String;[I)I", LV_messagebox },
    { NULL, NULL, NULL, NULL }
};

/*
 * LuaJIT. Its compiler writes machine code into memory it makes executable, which the phone does not give a guest (only the loader gets executable memory); with the JIT
 * off, LuaJIT runs its interpreter, and that is what a game gets. Each new Lua state is switched off as the standard libraries are opened.
 */
static void (*g_real_openlibs)(void *L);
static int (*g_jit_setmode)(void *L, int idx, int mode);
static void luajit_openlibs(void *L)
{
    if (!g_real_openlibs) g_real_openlibs = tl_ld_sym(NULL, "luaL_openlibs");
    if (!g_jit_setmode) g_jit_setmode = tl_ld_sym(NULL, "luaJIT_setmode");
    if (g_real_openlibs) g_real_openlibs(L);
    if (g_jit_setmode) g_jit_setmode(L, 0, 0 /* LUAJIT_MODE_ENGINE | LUAJIT_MODE_OFF */);
}

/* ------------------------------------------------------------------ install */

void tl_sdl_apps_install(const char *package)
{
    if (!strcmp(package, "org.broguece.game")) tl_jni_register_hle(k_brogue);
    if (tl_dexidx_has_class(LOVE)) { tl_jni_register_hle(k_love); tl_ld_interpose("luaL_openlibs", (void *)luajit_openlibs); }
    if (!strcmp(package, "org.pgnapps.pk2")) tl_jni_register_hle(k_pk2);
}
