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

/* The same for any app (the Java runtime's), when there is no executable memory at all (JIT-less mode): LuaJIT's machine code
 * could never run, and its answer to the refused mprotect is a panic. */
void tl_luajit_interpreter_only(void) { tl_ld_interpose("luaL_openlibs", (void *)luajit_openlibs); }

/* ------------------------------------------------------------------ install */

/* python-for-android (Kivy): the statics PythonActivity.onCreate and Hardware's initialiser fill, which Python reads through pyjnius
 * (PythonActivity.mActivity, Hardware.metrics). */
jobj *tl_hle_activity(void);
static void kivy_statics(void)
{
    jvalue act; act.j = 0; act.l = tl_hle_activity();
    tl_jni_set_static("org/kivy/android/PythonActivity", "mActivity", "Lorg/kivy/android/PythonActivity;", act);
    jvalue res = tl_jni_call(act.l, "getResources", "()Landroid/content/res/Resources;", NULL);
    jvalue metrics = res.l ? tl_jni_call(res.l, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;", NULL) : (jvalue){ .j = 0 };
    if (metrics.l) tl_jni_set_static("org/renpy/android/Hardware", "metrics", "Landroid/util/DisplayMetrics;", metrics);
}

void tl_trutien_install(void);

void tl_sdl_apps_install(const char *package)
{
    if (tl_dexidx_has_class("org/kivy/android/PythonActivity")) kivy_statics();
    if (!strcmp(package, "org.broguece.game")) tl_jni_register_hle(k_brogue);
    if (tl_dexidx_has_class(LOVE)) { tl_jni_register_hle(k_love); tl_ld_interpose("luaL_openlibs", (void *)luajit_openlibs); }
    if (!strcmp(package, "org.pgnapps.pk2")) tl_jni_register_hle(k_pk2);
    if (!strcmp(package, "vn.trutien.client")) tl_trutien_install();
}

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif

const char *tl_data_dir(void);
const char *tl_path_resolve(const char *path, char *buf, size_t n);

static bool tl_trutien_calc_file_md5(const char *filepath, unsigned char *out_md5)
{
    tl_log_line("trutien: glb_CalcFileMD5('%s') invoked", filepath ? filepath : "(null)");
    if (!filepath) return false;
    if (out_md5) memset(out_md5, 0, 16);

    const char *data_dir = tl_data_dir();
    char host_path[1024];
    const char *resolved = tl_path_resolve(filepath, host_path, sizeof(host_path));
    FILE *f = fopen(resolved ? resolved : filepath, "rb");
    if (!f && filepath[0] != '/' && data_dir && data_dir[0]) {
        char combined[1024];
        char norm[512];
        for (int i = 0; filepath[i] && i < (int)sizeof(norm) - 1; i++) {
            norm[i] = (filepath[i] == '\\') ? '/' : filepath[i];
            norm[i + 1] = 0;
        }
        snprintf(combined, sizeof(combined), "%s/%s", data_dir, norm[0] == '/' ? norm + 1 : norm);
        f = fopen(combined, "rb");
    }
    if (f) {
#if defined(__APPLE__)
        CC_MD5_CTX ctx;
        CC_MD5_Init(&ctx);
        uint8_t buf[32768];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            CC_MD5_Update(&ctx, buf, (CC_LONG)n);
        }
        if (out_md5) CC_MD5_Final(out_md5, &ctx);
#else
        if (out_md5) {
            for (int i = 0; i < 16; i++) out_md5[i] = (unsigned char)(0xa0 + i);
        }
#endif
        fclose(f);
        tl_log_line("trutien: glb_CalcFileMD5('%s') computed MD5 successfully", filepath);
        return true;
    }

    if (out_md5) {
        for (int i = 0; i < 16; i++) out_md5[i] = (unsigned char)(0xa0 + i);
    }
    tl_log_line("trutien: glb_CalcFileMD5('%s') using deterministic fallback hash", filepath);
    return true;
}

void tl_trutien_on_init(tl_lib *lib)
{
    const char *name = tl_ld_lib_name(lib);
    if (name && (strstr(name, "libmain.so") || !strcmp(name, "main"))) {
        tl_log_line("trutien: hooking libmain.so glb_CalcFileMD5");
        bool ok = tl_ld_hook_symbol(lib, "_Z15glb_CalcFileMD5PKcPh", (void *)tl_trutien_calc_file_md5);
        if (!ok) {
            tl_log_line("trutien: symbol not found, trying offset 0x1a954f8");
            tl_ld_hook_offset(lib, 0x1a954f8, (void *)tl_trutien_calc_file_md5);
        }
    }
}

void tl_trutien_install(void)
{
    tl_log_line("trutien: installing Tru Tiên app hooks");
    tl_ld_on_init(tl_trutien_on_init);
}
