/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Google Play services, as the translation layer's games see them: present, up to date, and signed out.
 *
 * Play services is Google's own Android app; a game reaches it through the client libraries it ships in its APK
 * (com.google.android.gms.*, com.android.billingclient.*), mostly from C# or C++ through JNI. None of that Java runs here, so
 * the calls that matter are answered in C, with answers a real phone gives a player who has not signed in:
 *
 *   - availability: installed and current (GoogleApiAvailability says SUCCESS), so games that refuse to start without Play
 *     services, or stop to show "Get Google Play services", carry on;
 *   - Play Games: signed in as the player the person's Google account is in this game, when they have signed in to Husk's
 *     Store with it (husk-tl-playgames.h: the app asks Google, as Play services does on a phone); otherwise "not authenticated".
 *     Achievements, leaderboards and events are accepted and dropped, and the calls that return a Task get one that has
 *     already finished (failed, where Play services would need more than the player);
 *   - Play Billing: the connection finishes with BILLING_UNAVAILABLE, the answer for a device whose Play Store cannot sell.
 *
 * A listener the game hands over -- a Unity C# proxy, most often -- is called back with the result, as Play services would.
 * Purchases are not here.
 */
#define _DARWIN_C_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-bionic.h"
#include "husk-tl-jni.h"
#include "husk-tl-jni-internal.h"
#include "husk-tl-playgames.h"

bool tl_dexidx_has_class(const char *name);
bool tl_dvm_defines_class(const char *jni_name);      /* the Java runtime's classes (the APK's among them) */
jobj *tl_proxy_call(jobj *proxy, const char *iface, const char *name, const char *sig, jobj **args, int nargs);

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static void Noop(tl_jcall *c) { (void)c; }
static void RetFalse(tl_jcall *c) { c->ret = vz(0); }
static void RetZero(tl_jcall *c) { c->ret = vi(0); }

/* The Play services version a current phone has (25.13). */
#define GMS_VERSION 251333035

static jobj *singleton(const char *cls)
{
    static struct { const char *cls; jobj *obj; } cache[32];
    for (int i = 0; i < 32; i++) {
        if (cache[i].cls && !strcmp(cache[i].cls, cls)) return cache[i].obj;
        if (!cache[i].cls) { cache[i].cls = cls; cache[i].obj = tl_jni_new_object(tl_jni_class(cls)); return cache[i].obj; }
    }
    return tl_jni_new_object(tl_jni_class(cls));
}

static void once(const char *what)
{
    char note[200]; snprintf(note, sizeof(note), "gms: %s", what);
    tl_note_once(note);
}

/* Call a listener the game handed over: a proxy for C# (Unity's AndroidJavaProxy) goes through the bridge, anything else
 * through its implementation here. */
static void deliver(jobj *listener, const char *iface, const char *method, const char *sig, jobj *arg)
{
    if (!listener) return;
    if (!strcmp(tl_jni_class_name(listener), "java/lang/reflect/Proxy")) { tl_proxy_call(listener, iface, method, sig, &arg, 1); return; }
    jvalue a = vl(arg);
    tl_jni_call(listener, method, sig, &a);
    if (tl_jni_pending()) tl_jni_clear();
}

/* ------------------------------------------------------- the app's answers */

static _Atomic(husk_pgs_host_fn) g_pgs_host;
void husk_pgs_set_host(husk_pgs_host_fn fn) { g_pgs_host = fn; }

static char g_pgs_package[256], g_pgs_apk[1100];
void tl_pgs_set_game(const char *package, const char *apk)
{
    snprintf(g_pgs_package, sizeof(g_pgs_package), "%s", package ? package : "");
    snprintf(g_pgs_apk, sizeof(g_pgs_apk), "%s", apk ? apk : "");
}

char *tl_pgs_ask(const char *op, const char *arg, const char *arg2, int flag)
{
    husk_pgs_host_fn f = g_pgs_host;
    return f ? f(op, g_pgs_package[0] ? g_pgs_package : NULL, g_pgs_apk[0] ? g_pgs_apk : NULL, arg, arg2, flag) : NULL;
}

bool tl_pgs_parse_player(const char *answer, tl_pgs_player *out, char *why, unsigned why_size)
{
    memset(out, 0, sizeof(*out));
    if (why && why_size) why[0] = 0;
    if (!answer || !answer[0] || answer[0] == '!') {
        if (why && why_size) snprintf(why, why_size, "%s", answer && answer[0] == '!' ? answer + 1 : "Husk has no Google account to sign in with");
        return false;
    }
    char *fields[5] = { out->id, out->name, out->icon, out->hires, out->title };
    const size_t sizes[5] = { sizeof(out->id), sizeof(out->name), sizeof(out->icon), sizeof(out->hires), sizeof(out->title) };
    const char *p = answer;
    for (int i = 0; i < 5 && p; i++) {
        const char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);
        if (n >= sizes[i]) n = sizes[i] - 1;
        memcpy(fields[i], p, n); fields[i][n] = 0;
        p = nl ? nl + 1 : NULL;
    }
    return out->id[0] != 0;
}

/* The player this run is signed in as, asked once; later calls get the same answer, as from Play services' own cache. */
static pthread_mutex_t g_player_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_player_asked, g_player_ok;
static tl_pgs_player g_player;
static bool current_player(bool interactive, tl_pgs_player *out)
{
    pthread_mutex_lock(&g_player_lock);
    if (!g_player_asked || (!g_player_ok && interactive)) {
        char *a = tl_pgs_ask("signin", NULL, NULL, interactive);
        char why[200];
        g_player_ok = tl_pgs_parse_player(a, &g_player, why, sizeof(why));
        g_player_asked = a != NULL;
        free(a);
        char note[300];
        if (g_player_ok) snprintf(note, sizeof(note), "gms: Play Games signed in (player %s)", g_player.id);
        else snprintf(note, sizeof(note), "gms: Play Games signed out: %s", why);
        tl_log_line("%s", note);
    }
    bool ok = g_player_ok;
    if (ok && out) *out = g_player;
    pthread_mutex_unlock(&g_player_lock);
    return ok;
}
/* Every Play Games call the game makes, and its answer, in the game's log: what to look at when an online save does not find
 * its player. The first 40 calls and then every 50th, so a game that polls does not fill the log. */
static void pgs_trace(const char *call, const char *answer)
{
    static _Atomic int n;
    int k = n++;
    if (k < 40 || k % 50 == 0) tl_log_line("gms: Play Games %s -> %s", call, answer);
}

/* Whether the player is in, from what has been asked already: never asks (a game that never signs in is not held up). */
static bool signed_in_now(void)
{
    if (pthread_mutex_trylock(&g_player_lock) != 0) return false;      /* a sign-in under way: not in yet */
    bool ok = g_player_asked && g_player_ok;
    pthread_mutex_unlock(&g_player_lock);
    return ok;
}

/* ------------------------------------------------------------------ Tasks */

#define TASK "com/google/android/gms/tasks/Task"

/*
 * The objects made here are of classes of Husk's own (husk/gms/...), declared on this side only. On Husk's Java runtime the APK's
 * classes of the same name are the interpreter's, and a field they do not declare is kept once for the class rather than in each
 * object -- every Task would share one result, and getCurrentPlayer's player would be gone by the time Tasks.whenAll's listener
 * asked for it. A class only this side knows keeps its fields in each object, on the Java runtime and without it alike. The
 * game's C# does not check the classes; it calls their methods by name, and those are answered here.
 */
#define HTASK "husk/gms/Task"
#define HAPIEX "husk/gms/ApiException"
#define HAUTH "husk/gms/AuthenticationResult"
#define HPLAYER "husk/gms/Player"

/* On Husk's Java runtime (tl_gms_install_java) the game's C# finds a method with Java reflection, which sees only classes a
 * DEX defines: the answers there are objects of the APK's own classes -- real Tasks, its AuthenticationResult and PlayerEntity. */
static bool g_java;

/* `new cls(args)` through the constructor of that signature; NULL if there is none or it threw. */
static jobj *construct(const char *cls, const char *sig, const jvalue *args)
{
    tl_jclass *k = tl_jni_class(cls);
    tl_jmeth *m = tl_jni_method(k, "<init>", sig, false);
    if (!m || !m->exists || !m->dvm) return NULL;
    jobj *o = tl_jni_new_object(k);
    tl_jni_invoke(o, m, true, args);
    if (tl_jni_pending()) { tl_jni_clear(); return NULL; }
    return o;
}

/* An ApiException(new Status(code, message)), as Play services fails a Task; a plain Exception where the APK lacks those. */
static jobj *api_exception(const char *error)
{
    jvalue sa[2]; sa[0].j = 0; sa[0].i = atoi(error); sa[1] = vl(tl_jni_new_string(error));
    jobj *st = construct("com/google/android/gms/common/api/Status", "(ILjava/lang/String;)V", sa);
    jvalue ea = vl(st);
    jobj *ex = st ? construct("com/google/android/gms/common/api/ApiException", "(Lcom/google/android/gms/common/api/Status;)V", &ea) : NULL;
    if (!ex) { jvalue m = vl(tl_jni_new_string(error)); ex = construct("java/lang/Exception", "(Ljava/lang/String;)V", &m); }
    return ex;
}

/* A real Task, finished: the APK's own TaskCompletionSource made it, so its listeners, getResult and Tasks.whenAll are the
 * library's own, run as on a phone (listeners on the main thread). NULL if the APK has no play-services-tasks. */
static jobj *real_task(jobj *result, const char *error)
{
    jobj *tcs = construct("com/google/android/gms/tasks/TaskCompletionSource", "()V", NULL);
    if (!tcs) return NULL;
    jvalue a = vl(error ? api_exception(error) : result);
    if (error) tl_jni_call(tcs, "setException", "(Ljava/lang/Exception;)V", &a);
    else tl_jni_call(tcs, "setResult", "(Ljava/lang/Object;)V", &a);
    if (tl_jni_pending()) { tl_jni_clear(); return NULL; }
    jobj *t = tl_jni_call(tcs, "getTask", "()Lcom/google/android/gms/tasks/Task;", NULL).l;
    if (tl_jni_pending()) { tl_jni_clear(); return NULL; }
    return t;
}

/* A Task that has already finished: with a result, or with an exception ("<status code>: <message>"). */
static jobj *task_done(jobj *result, const char *error)
{
    if (g_java) {
        jobj *real = real_task(result, error);
        if (real) return real;
        static int said; if (!said++) tl_log_line("gms: the APK has no TaskCompletionSource to answer Play Games with; answering with Husk's own Tasks");
    }
    jobj *t = tl_jni_new_object(tl_jni_class(HTASK));
    tl_jni_set_field(t, "result", "Ljava/lang/Object;", vl(result));
    tl_jni_set_field(t, "successful", "Z", vz(error == NULL));
    if (error) {
        jobj *ex = tl_jni_new_object(tl_jni_class(HAPIEX));
        tl_jni_set_field(ex, "message", "Ljava/lang/String;", vl(tl_jni_new_string(error)));
        tl_jni_set_field(ex, "statusCode", "I", vi(atoi(error)));
        tl_jni_set_field(t, "exception", "Ljava/lang/Exception;", vl(ex));
    }
    return t;
}
/* SIGN_IN_REQUIRED, what Play services answers for a player who is not in. */
static jobj *task_failed(void) { return task_done(NULL, "4: SIGN_IN_REQUIRED (not signed in to Google Play Games)"); }
/* Signed in, but not something Husk does for the game (saved games, achievements' lists). */
static jobj *task_unavailable(void) { return task_done(NULL, "17: API_NOT_CONNECTED (not available in Husk)"); }
static bool is_task(jobj *t) { return t && (!strcmp(tl_jni_class_name(t), HTASK) || !strcmp(tl_jni_class_name(t), TASK)); }

static bool ok(jobj *t) { return tl_jni_get_field(t, "successful", "Z").z; }
static void Task_isComplete(tl_jcall *c) { c->ret = vz(1); }
static void Task_isSuccessful(tl_jcall *c) { c->ret = vz(ok(c->self)); }
static void Task_getResult(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "result", "Ljava/lang/Object;"); }
static void Task_getException(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "exception", "Ljava/lang/Exception;"); }

/* A listener is told at once: the task has finished. The last argument is the listener in every overload. */
static void call_listener(tl_jcall *c, int nargs, const char *method, const char *sig, bool want_success, bool want_failure)
{
    char iface[80];
    snprintf(iface, sizeof(iface), "com/google/android/gms/tasks/On%c%sListener", method[2], method + 3);
    jobj *listener = c->args[nargs - 1].l;
    c->ret = vl(c->self);
    if (!listener) return;
    bool s = ok(c->self);
    if ((s && !want_success) || (!s && !want_failure)) return;
    jobj *a;
    if (!strcmp(method, "onComplete")) a = c->self;
    else if (!strcmp(method, "onSuccess")) a = tl_jni_get_field(c->self, "result", "Ljava/lang/Object;").l;
    else a = tl_jni_get_field(c->self, "exception", "Ljava/lang/Exception;").l;
    deliver(listener, iface, method, sig, a);
}
#define LISTEN(nm, n, method, sig, s, f) static void nm(tl_jcall *c) { call_listener(c, n, method, sig, s, f); }
LISTEN(Task_onComplete1, 1, "onComplete", "(Lcom/google/android/gms/tasks/Task;)V", true, true)
LISTEN(Task_onComplete2, 2, "onComplete", "(Lcom/google/android/gms/tasks/Task;)V", true, true)
LISTEN(Task_onSuccess1, 1, "onSuccess", "(Ljava/lang/Object;)V", true, false)
LISTEN(Task_onSuccess2, 2, "onSuccess", "(Ljava/lang/Object;)V", true, false)
LISTEN(Task_onFailure1, 1, "onFailure", "(Ljava/lang/Exception;)V", false, true)
LISTEN(Task_onFailure2, 2, "onFailure", "(Ljava/lang/Exception;)V", false, true)

static void ApiException_getMessage(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "message", "Ljava/lang/String;"); }
static void ApiException_getStatusCode(tl_jcall *c) { int s = tl_jni_get_field(c->self, "statusCode", "I").i; c->ret = vi(s ? s : 17); }

/* ------------------------------------------------------------ availability */

static void GA_getInstance(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/common/GoogleApiAvailability")); }
static void GAL_getInstance(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/common/GoogleApiAvailabilityLight")); }
static void GA_available(tl_jcall *c) { once("a game asked whether Play services is available: yes"); c->ret = vi(0); }    /* ConnectionResult.SUCCESS */
static void GA_version(tl_jcall *c) { c->ret = vi(GMS_VERSION); }
static void GA_makeAvailable(tl_jcall *c) { c->ret = vl(task_done(NULL, NULL)); }

/* -------------------------------------------------------------- Play Games */

static void PG_signInClient(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/games/GamesSignInClient")); }
static void PG_achievements(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/games/AchievementsClient")); }
static void PG_leaderboards(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/games/LeaderboardsClient")); }
static void PG_players(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/games/PlayersClient")); }
static void PG_events(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/games/EventsClient")); }
static void PG_snapshots(tl_jcall *c) { c->ret = vl(singleton("com/google/android/gms/games/SnapshotsClient")); }
static void PG_initialize(tl_jcall *c) { (void)c; once("Play Games initialised"); pgs_trace("PlayGamesSdk.initialize", "done"); }

/* isAuthenticated (the quiet check at start) / signIn (a button): finished, successfully, saying whether the player is in --
 * as Play services answers, signed out included. */
static void sign_in(tl_jcall *c, bool interactive)
{
    jobj *r = tl_jni_new_object(tl_jni_class(g_java ? "com/google/android/gms/games/AuthenticationResult" : HAUTH));
    bool in = current_player(interactive, NULL);
    tl_jni_set_field(r, "authenticated", "Z", vz(in));
    c->ret = vl(task_done(r, NULL));
    pgs_trace(interactive ? "GamesSignInClient.signIn" : "GamesSignInClient.isAuthenticated", in ? "authenticated" : "not authenticated");
}
static void SignIn_check(tl_jcall *c) { sign_in(c, false); }
static void SignIn_signIn(tl_jcall *c) { sign_in(c, true); }
static void Auth_isAuthenticated(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "authenticated", "Z"); }
static void Task_failed(tl_jcall *c) { c->ret = vl(signed_in_now() ? task_unavailable() : task_failed()); }

#define PLAYER "com/google/android/gms/games/Player"
#define PLAYER_ENTITY "com/google/android/gms/games/PlayerEntity"
static jobj *player_object(const tl_pgs_player *p)
{
    /* on the Java runtime, the APK's PlayerEntity: its getters are answered here (below), with this player's */
    const char *cls = g_java && tl_dvm_defines_class(PLAYER_ENTITY) ? PLAYER_ENTITY : HPLAYER;
    jobj *o = tl_jni_new_object(tl_jni_class(cls));
    tl_jni_set_field(o, "playerId", "Ljava/lang/String;", vl(tl_jni_new_string(p->id)));
    tl_jni_set_field(o, "displayName", "Ljava/lang/String;", vl(tl_jni_new_string(p->name)));
    tl_jni_set_field(o, "iconImageUrl", "Ljava/lang/String;", vl(p->icon[0] ? tl_jni_new_string(p->icon) : NULL));
    tl_jni_set_field(o, "hiResImageUrl", "Ljava/lang/String;", vl(p->hires[0] ? tl_jni_new_string(p->hires) : NULL));
    tl_jni_set_field(o, "title", "Ljava/lang/String;", vl(p->title[0] ? tl_jni_new_string(p->title) : NULL));
    return o;
}
static void PC_currentPlayer(tl_jcall *c)
{
    tl_pgs_player p;
    bool in = current_player(false, &p);
    c->ret = vl(in ? task_done(player_object(&p), NULL) : task_failed());
    pgs_trace("PlayersClient.getCurrentPlayer", in ? p.id : "failed: SIGN_IN_REQUIRED");
}
static void PC_currentPlayerId(tl_jcall *c)
{
    tl_pgs_player p;
    bool in = current_player(false, &p);
    c->ret = vl(in ? task_done(tl_jni_new_string(p.id), NULL) : task_failed());
    pgs_trace("PlayersClient.getCurrentPlayerId", in ? p.id : "failed: SIGN_IN_REQUIRED");
}
#define PLAYER_STR(nm, field) static void nm(tl_jcall *c) \
    { c->ret = tl_jni_get_field(c->self, field, "Ljava/lang/String;"); pgs_trace("Player." field, tl_jni_string(c->ret.l) ? tl_jni_string(c->ret.l) : "null"); }
PLAYER_STR(Player_id, "playerId")
PLAYER_STR(Player_name, "displayName")
PLAYER_STR(Player_icon, "iconImageUrl")
PLAYER_STR(Player_hires, "hiResImageUrl")
PLAYER_STR(Player_title, "title")
static void Player_hasIcon(tl_jcall *c) { c->ret = vz(tl_jni_get_field(c->self, "iconImageUrl", "Ljava/lang/String;").l != NULL); }
static void Player_hasHiRes(tl_jcall *c) { c->ret = vz(tl_jni_get_field(c->self, "hiResImageUrl", "Ljava/lang/String;").l != NULL); }

/* requestServerSideAccess(serverClientId, forceRefresh): a code the game's server trades with Google for the player's tokens. */
static void SignIn_serverAccess(tl_jcall *c)
{
    const char *client = tl_jni_string(c->args[0].l);
    if (!client || !current_player(false, NULL)) { c->ret = vl(task_failed()); pgs_trace("GamesSignInClient.requestServerSideAccess", "failed: SIGN_IN_REQUIRED"); return; }
    char *code = tl_pgs_ask("servercode", client, "", c->args[1].z);
    if (code && code[0] && code[0] != '!') {
        c->ret = vl(task_done(tl_jni_new_string(code), NULL));
        pgs_trace("GamesSignInClient.requestServerSideAccess", "a server auth code");      /* never the code itself */
    } else {
        char why[300]; snprintf(why, sizeof(why), "8: INTERNAL_ERROR (%s)", code && code[0] == '!' ? code + 1 : "no server auth code");
        c->ret = vl(task_done(NULL, why));
        pgs_trace("GamesSignInClient.requestServerSideAccess", why);
    }
    free(code);
}

/* requestServerSideAccess(serverClientId, forceRefresh, List<AuthScope>): the same code, with the OpenID scopes the game asked
 * for (EMAIL, PROFILE, OPEN_ID), as an AuthResponse; the scopes granted are the ones asked for. */
#define HAUTHRESP "husk/gms/AuthResponse"
#define AUTHRESP "com/google/android/gms/games/gamessignin/AuthResponse"
static void SignIn_serverAccessScopes(tl_jcall *c)
{
    const char *client = tl_jni_string(c->args[0].l);
    jobj *list = c->args[2].l;
    if (!client || !current_player(false, NULL)) { c->ret = vl(task_failed()); pgs_trace("GamesSignInClient.requestServerSideAccess(scopes)", "failed: SIGN_IN_REQUIRED"); return; }
    char scopes[200] = "";
    int n = list ? tl_jni_call(list, "size", "()I", NULL).i : 0;
    if (tl_jni_pending()) { tl_jni_clear(); n = 0; }
    for (int i = 0; i < n && i < 8; i++) {
        jvalue ix; ix.j = 0; ix.i = i;
        jobj *e = tl_jni_call(list, "get", "(I)Ljava/lang/Object;", &ix).l;
        const char *nm = e ? tl_jni_string(tl_jni_call(e, "name", "()Ljava/lang/String;", NULL).l) : NULL;
        if (tl_jni_pending()) { tl_jni_clear(); continue; }
        const char *scope = !nm ? NULL : !strcmp(nm, "EMAIL") ? "email" : !strcmp(nm, "PROFILE") ? "profile" : !strcmp(nm, "OPEN_ID") ? "openid" : NULL;
        if (scope) { size_t l = strlen(scopes); snprintf(scopes + l, sizeof(scopes) - l, "%s%s", l ? " " : "", scope); }
    }
    char *code = tl_pgs_ask("servercode", client, scopes, c->args[1].z);
    if (code && code[0] && code[0] != '!') {
        const char *rc = g_java && tl_dvm_defines_class(AUTHRESP) ? AUTHRESP : HAUTHRESP;
        jobj *r = tl_jni_new_object(tl_jni_class(rc));
        tl_jni_set_field(r, "authCode", "Ljava/lang/String;", vl(tl_jni_new_string(code)));
        tl_jni_set_field(r, "grantedScopes", "Ljava/util/List;", vl(list));
        c->ret = vl(task_done(r, NULL));
        pgs_trace("GamesSignInClient.requestServerSideAccess(scopes)", scopes[0] ? scopes : "a server auth code");
    } else {
        char why[300]; snprintf(why, sizeof(why), "8: INTERNAL_ERROR (%s)", code && code[0] == '!' ? code + 1 : "no server auth code");
        c->ret = vl(task_done(NULL, why));
        pgs_trace("GamesSignInClient.requestServerSideAccess(scopes)", why);
    }
    free(code);
}
static void AuthResp_code(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "authCode", "Ljava/lang/String;"); }
static void AuthResp_scopes(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "grantedScopes", "Ljava/util/List;"); }

/* Tasks.whenAll: the Tasks here have finished already. The list's own contents are not kept on this side (Husk's C lists are
 * empty), and the one a game waits on at sign-in is getCurrentPlayer's, so it has succeeded when the player is in. */
static void Tasks_whenAll(tl_jcall *c)
{
    bool in = current_player(false, NULL);
    c->ret = vl(in ? task_done(NULL, NULL) : task_failed());
    pgs_trace("Tasks.whenAll", in ? "succeeded" : "failed: SIGN_IN_REQUIRED");
}
static void Tasks_forResult(tl_jcall *c) { c->ret = vl(task_done(c->args[0].l, NULL)); }

/* ----------------------------------------------------------------- Billing */

#define BC "com/android/billingclient/api/BillingClient"
#define BCB "com/android/billingclient/api/BillingClient$Builder"
#define BR "com/android/billingclient/api/BillingResult"
#define PPP "com/android/billingclient/api/PendingPurchasesParams"
#define PPPB "com/android/billingclient/api/PendingPurchasesParams$Builder"
#define BILLING_UNAVAILABLE 3

static jobj *billing_result(int code, const char *message)
{
    jobj *r = tl_jni_new_object(tl_jni_class(BR));
    tl_jni_set_field(r, "code", "I", vi(code));
    tl_jni_set_field(r, "message", "Ljava/lang/String;", vl(tl_jni_new_string(message)));
    return r;
}
static void BC_newBuilder(tl_jcall *c) { c->ret = vl(tl_jni_new_object(tl_jni_class(BCB))); }
static void BCB_self(tl_jcall *c) { c->ret = vl(c->self); }
static void PPP_newBuilder(tl_jcall *c) { c->ret = vl(tl_jni_new_object(tl_jni_class(PPPB))); }
static void PPPB_build(tl_jcall *c) { c->ret = vl(tl_jni_new_object(tl_jni_class(PPP))); }
static void BCB_build(tl_jcall *c) { c->ret = vl(tl_jni_new_object(tl_jni_class(BC))); }
static void BC_startConnection(tl_jcall *c)
{
    once("Play Billing: connection finished with BILLING_UNAVAILABLE");
    deliver(c->args[0].l, "com/android/billingclient/api/BillingClientStateListener", "onBillingSetupFinished",
            "(Lcom/android/billingclient/api/BillingResult;)V", billing_result(BILLING_UNAVAILABLE, "Google Play Billing is not available in Husk"));
}
static void BC_isFeatureSupported(tl_jcall *c) { c->ret = vl(billing_result(BILLING_UNAVAILABLE, "")); }
static void BR_code(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "code", "I"); }
static void BR_message(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "message", "Ljava/lang/String;"); }

/* ------------------------------------------------------- Firebase's helpers */

/*
 * Firebase's C++ SDK (Unity's Firebase plugin is built on it) carries a few Java classes of its own, which it writes out as a
 * dex at start and loads through a DexClassLoader. Without them Firebase stops at "Java class ... Log not found" and nothing
 * of it works, the availability check included. They are glue between Java callbacks and its C++, so here they are, in C:
 * results are handed to the natives Firebase registered on these classes.
 */
#define FB "com/google/firebase/app/internal/cpp/"

#define GLVR "com/google/firebase/platforminfo/GlobalLibraryVersionRegistrar"
static void GLVR_getInstance(tl_jcall *c) { c->ret = vl(singleton(GLVR)); }
static void FBLog_getInstance(tl_jcall *c) { c->ret = vl(singleton(FB "Log")); }
static void FBLog_line(tl_jcall *c)
{
    const char *tag = tl_jni_string(c->args[0].l), *msg = tl_jni_string(c->args[1].l);
    tl_log_line("firebase/%s: %s", tag ? tag : "?", msg ? msg : "");
    c->ret = vi(0);
}

/* CppThreadDispatcherContext: a C++ function and its data, run on the thread asked for. */
typedef void (*fb_fn_native)(void *env, jobj *self, int64_t fn, int64_t data);
static void Ctx_init(tl_jcall *c)
{
    tl_jni_set_field(c->self, "functionPtr", "J", c->args[0]);
    tl_jni_set_field(c->self, "functionData", "J", c->args[1]);
    tl_jni_set_field(c->self, "cancelFunctionPtr", "J", c->args[2]);
}
static void ctx_execute(jobj *ctx)
{
    fb_fn_native f = tl_jni_native(FB "CppThreadDispatcherContext", "nativeFunction", "(JJ)V");
    int64_t fn = tl_jni_get_field(ctx, "functionPtr", "J").j;
    if (!f || !fn || tl_jni_get_field(ctx, "done", "Z").z) return;
    tl_jni_set_field(ctx, "done", "Z", vz(1));
    f(tl_jni_env(), ctx, fn, tl_jni_get_field(ctx, "functionData", "J").j);
    if (tl_jni_pending()) tl_jni_clear();
}
static void Ctx_execute(tl_jcall *c) { ctx_execute(c->self); }
static void Ctx_cancel(tl_jcall *c) { tl_jni_set_field(c->self, "done", "Z", vz(1)); }
static void Ctx_acquire(tl_jcall *c) { c->ret = vz(!tl_jni_get_field(c->self, "done", "Z").z); }
static void *ctx_thread(void *arg) { ctx_execute(arg); tl_jni_unref(arg); return NULL; }
static void Dispatch_main(tl_jcall *c) { ctx_execute(c->args[1].l); }
static void Dispatch_background(tl_jcall *c)
{
    pthread_t t;
    jobj *ctx = tl_jni_ref(c->args[0].l);
    if (pthread_create(&t, NULL, ctx_thread, ctx) == 0) pthread_detach(t); else { ctx_execute(ctx); tl_jni_unref(ctx); }
}

/* JniResultCallback: a Task's result for a C++ future. The Tasks here have finished already, so the answer goes at once. */
typedef void (*fb_result_native)(void *env, jobj *self, jobj *result, uint8_t success, uint8_t cancelled, jobj *status, int64_t fn, int64_t data);
static void jrc_deliver(jobj *self, jobj *result, bool success, bool cancelled, const char *status)
{
    fb_result_native f = tl_jni_native(FB "JniResultCallback", "nativeOnResult", "(Ljava/lang/Object;ZZLjava/lang/String;JJ)V");
    int64_t fn = tl_jni_get_field(self, "callbackFn", "J").j;
    if (!f || !fn || tl_jni_get_field(self, "done", "Z").z) return;
    tl_jni_set_field(self, "done", "Z", vz(1));
    f(tl_jni_env(), self, result, success, cancelled, status ? tl_jni_new_string(status) : NULL, fn, tl_jni_get_field(self, "callbackData", "J").j);
    if (tl_jni_pending()) tl_jni_clear();
}
static void JRC_init2(tl_jcall *c)
{
    tl_jni_set_field(c->self, "callbackFn", "J", c->args[0]);
    tl_jni_set_field(c->self, "callbackData", "J", c->args[1]);
}
static void JRC_init3(tl_jcall *c)
{
    jobj *task = c->args[0].l;
    tl_jni_set_field(c->self, "callbackFn", "J", c->args[1]);
    tl_jni_set_field(c->self, "callbackData", "J", c->args[2]);
    if (!is_task(task)) { jrc_deliver(c->self, NULL, false, false, "Google Play services task not available in Husk"); return; }
    jobj *ex = tl_jni_get_field(task, "exception", "Ljava/lang/Exception;").l;
    jrc_deliver(c->self, tl_jni_get_field(task, "result", "Ljava/lang/Object;").l, ok(task), false,
                ex ? tl_jni_string(tl_jni_get_field(ex, "message", "Ljava/lang/String;").l) : NULL);
}
static void JRC_cancel(tl_jcall *c) { jrc_deliver(c->self, NULL, false, true, "cancelled"); }

/* GoogleApiAvailabilityHelper.makeGooglePlayServicesAvailable: there is nothing to make available; it says so straight away. */
static void GAH_make(tl_jcall *c)
{
    typedef void (*done_fn)(void *env, jobj *cls, int code, jobj *msg);
    done_fn f = tl_jni_native(FB "GoogleApiAvailabilityHelper", "onCompleteNative", "(ILjava/lang/String;)V");
    if (f) { f(tl_jni_env(), tl_jni_class_object(FB "GoogleApiAvailabilityHelper"), 0, tl_jni_new_string("")); if (tl_jni_pending()) tl_jni_clear(); }
    c->ret = vz(1);
}

/* ------------------------------------------------------------------ tables */

static const struct { const char *name, *super; } k_classes[] = {
    { "dalvik/system/BaseDexClassLoader", "java/lang/ClassLoader" }, { "dalvik/system/DexClassLoader", "dalvik/system/BaseDexClassLoader" },
    { TASK, "java/lang/Object" },
    { "com/google/android/gms/common/api/ApiException", "java/lang/Exception" },
    { "com/google/android/gms/common/GoogleApiAvailabilityLight", "java/lang/Object" },
    { "com/google/android/gms/common/GoogleApiAvailability", "com/google/android/gms/common/GoogleApiAvailabilityLight" },
    { "com/google/android/gms/common/GooglePlayServicesUtilLight", "java/lang/Object" },
    { "com/google/android/gms/common/GooglePlayServicesUtil", "com/google/android/gms/common/GooglePlayServicesUtilLight" },
    { "com/google/android/gms/games/PlayGamesSdk", "java/lang/Object" }, { "com/google/android/gms/games/PlayGames", "java/lang/Object" },
    { "com/google/android/gms/games/GamesSignInClient", "java/lang/Object" },
    { "com/google/android/gms/games/AuthenticationResult", "java/lang/Object" },
    { PLAYER, "java/lang/Object" }, { "com/google/android/gms/tasks/Tasks", "java/lang/Object" },
    { HTASK, TASK }, { HAPIEX, "java/lang/Exception" }, { HAUTH, "java/lang/Object" }, { HPLAYER, "java/lang/Object" },
    { HAUTHRESP, "java/lang/Object" },
    { "com/google/android/gms/games/AchievementsClient", "java/lang/Object" },
    { "com/google/android/gms/games/LeaderboardsClient", "java/lang/Object" },
    { "com/google/android/gms/games/PlayersClient", "java/lang/Object" },
    { "com/google/android/gms/games/EventsClient", "java/lang/Object" },
    { "com/google/android/gms/games/SnapshotsClient", "java/lang/Object" },
    { FB "Log", "java/lang/Object" }, { FB "CppThreadDispatcher", "java/lang/Object" },
    { FB "CppThreadDispatcherContext", "java/lang/Object" }, { FB "JniResultCallback", "java/lang/Object" },
    { FB "GoogleApiAvailabilityHelper", "java/lang/Object" },
    { PPP, "java/lang/Object" }, { PPPB, "java/lang/Object" },
    { BC, "java/lang/Object" }, { BCB, "java/lang/Object" }, { BR, "java/lang/Object" },
};

/* ------------------------------------------------------------ class loader */

/*
 * ClassLoader.loadClass and friends, for every engine: native code reaches the game's own Java -- Firebase's and Play
 * services' among it -- through the activity's class loader by dotted name, as FindClass sees only the system's. The class
 * is there if the framework, the APK, or the runtime itself has it.
 */
static void CL_loadClass(tl_jcall *c)
{
    char name[300]; snprintf(name, sizeof(name), "%s", tl_jni_string(c->args[0].l) ? tl_jni_string(c->args[0].l) : "");
    for (char *p = name; *p; p++) if (*p == '.') *p = '/';
    bool framework = !strncmp(name, "android/", 8) || !strncmp(name, "java/", 5) || !strncmp(name, "javax/", 6)
                  || !strncmp(name, "dalvik/", 7) || !strncmp(name, "org/json/", 9);
    bool ours = false;
    for (size_t i = 0; i < sizeof(k_classes) / sizeof(k_classes[0]); i++) if (!strcmp(k_classes[i].name, name)) ours = true;
    if (name[0] && (framework || ours || tl_dexidx_has_class(name))) { c->ret = vl(tl_jni_class_object(name)); return; }
    tl_jni_throw("java/lang/ClassNotFoundException", name);
    c->ret = vl(NULL);
}
static void CL_loadClass2(tl_jcall *c) { CL_loadClass(c); }      /* loadClass(String, boolean resolve) */
/* new DexClassLoader(path, optimizedDir, libraryPath, parent): the classes it would add are looked up in the APK's anyway. */
static void CL_init(tl_jcall *c) { (void)c; }

#define M(c, n, s, f) { c, n, s, f }
#define AC "com/google/android/gms/games/AchievementsClient"
#define LC "com/google/android/gms/games/LeaderboardsClient"
static const tl_jhle k_gms[] = {
    /* class loaders */
#define CLS_ "(Ljava/lang/String;)Ljava/lang/Class;"
#define CLSZ "(Ljava/lang/String;Z)Ljava/lang/Class;"
    M("java/lang/ClassLoader", "loadClass", CLS_, CL_loadClass), M("java/lang/ClassLoader", "loadClass", CLSZ, CL_loadClass2),
    M("java/lang/ClassLoader", "findClass", CLS_, CL_loadClass), M("java/lang/ClassLoader", "findLoadedClass", CLS_, CL_loadClass),
    M("dalvik/system/BaseDexClassLoader", "findClass", CLS_, CL_loadClass),
    M("dalvik/system/PathClassLoader", "loadClass", CLS_, CL_loadClass), M("dalvik/system/PathClassLoader", "findClass", CLS_, CL_loadClass),
    M("dalvik/system/DexClassLoader", "loadClass", CLS_, CL_loadClass),
    M("dalvik/system/DexClassLoader", "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V", CL_init),

    /* Task */
    M(TASK, "isComplete", "()Z", Task_isComplete), M(TASK, "isSuccessful", "()Z", Task_isSuccessful),
    M(TASK, "isCanceled", "()Z", RetFalse),
    M(TASK, "getResult", "()Ljava/lang/Object;", Task_getResult), M(TASK, "getException", "()Ljava/lang/Exception;", Task_getException),
    M(TASK, "addOnCompleteListener", "(Lcom/google/android/gms/tasks/OnCompleteListener;)Lcom/google/android/gms/tasks/Task;", Task_onComplete1),
    M(TASK, "addOnCompleteListener", "(Landroid/app/Activity;Lcom/google/android/gms/tasks/OnCompleteListener;)Lcom/google/android/gms/tasks/Task;", Task_onComplete2),
    M(TASK, "addOnCompleteListener", "(Ljava/util/concurrent/Executor;Lcom/google/android/gms/tasks/OnCompleteListener;)Lcom/google/android/gms/tasks/Task;", Task_onComplete2),
    M(TASK, "addOnSuccessListener", "(Lcom/google/android/gms/tasks/OnSuccessListener;)Lcom/google/android/gms/tasks/Task;", Task_onSuccess1),
    M(TASK, "addOnSuccessListener", "(Landroid/app/Activity;Lcom/google/android/gms/tasks/OnSuccessListener;)Lcom/google/android/gms/tasks/Task;", Task_onSuccess2),
    M(TASK, "addOnSuccessListener", "(Ljava/util/concurrent/Executor;Lcom/google/android/gms/tasks/OnSuccessListener;)Lcom/google/android/gms/tasks/Task;", Task_onSuccess2),
    M(TASK, "addOnFailureListener", "(Lcom/google/android/gms/tasks/OnFailureListener;)Lcom/google/android/gms/tasks/Task;", Task_onFailure1),
    M(TASK, "addOnFailureListener", "(Landroid/app/Activity;Lcom/google/android/gms/tasks/OnFailureListener;)Lcom/google/android/gms/tasks/Task;", Task_onFailure2),
    M(TASK, "addOnFailureListener", "(Ljava/util/concurrent/Executor;Lcom/google/android/gms/tasks/OnFailureListener;)Lcom/google/android/gms/tasks/Task;", Task_onFailure2),
    M("com/google/android/gms/common/api/ApiException", "getMessage", "()Ljava/lang/String;", ApiException_getMessage),
    M("com/google/android/gms/common/api/ApiException", "getStatusCode", "()I", ApiException_getStatusCode),
    M(HAPIEX, "getMessage", "()Ljava/lang/String;", ApiException_getMessage), M(HAPIEX, "getLocalizedMessage", "()Ljava/lang/String;", ApiException_getMessage),
    M(HAPIEX, "toString", "()Ljava/lang/String;", ApiException_getMessage), M(HAPIEX, "getStatusCode", "()I", ApiException_getStatusCode),

    /* availability */
    M("com/google/android/gms/common/GoogleApiAvailability", "getInstance", "()Lcom/google/android/gms/common/GoogleApiAvailability;", GA_getInstance),
    M("com/google/android/gms/common/GoogleApiAvailabilityLight", "getInstance", "()Lcom/google/android/gms/common/GoogleApiAvailabilityLight;", GAL_getInstance),
    M("com/google/android/gms/common/GoogleApiAvailabilityLight", "isGooglePlayServicesAvailable", "(Landroid/content/Context;)I", GA_available),
    M("com/google/android/gms/common/GoogleApiAvailabilityLight", "isGooglePlayServicesAvailable", "(Landroid/content/Context;I)I", GA_available),
    M("com/google/android/gms/common/GoogleApiAvailabilityLight", "getApkVersion", "(Landroid/content/Context;)I", GA_version),
    M("com/google/android/gms/common/GoogleApiAvailabilityLight", "isUserResolvableError", "(I)Z", RetFalse),
    M("com/google/android/gms/common/GoogleApiAvailability", "makeGooglePlayServicesAvailable", "(Landroid/app/Activity;)Lcom/google/android/gms/tasks/Task;", GA_makeAvailable),
    M("com/google/android/gms/common/GooglePlayServicesUtilLight", "isGooglePlayServicesAvailable", "(Landroid/content/Context;)I", GA_available),
    M("com/google/android/gms/common/GooglePlayServicesUtilLight", "isGooglePlayServicesAvailable", "(Landroid/content/Context;I)I", GA_available),
    M("com/google/android/gms/common/GooglePlayServicesUtilLight", "getApkVersion", "(Landroid/content/Context;)I", GA_version),

    /* Play Games v2 */
    M("com/google/android/gms/games/PlayGamesSdk", "initialize", "(Landroid/content/Context;)V", PG_initialize),
    M("com/google/android/gms/games/PlayGames", "getGamesSignInClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/GamesSignInClient;", PG_signInClient),
    M("com/google/android/gms/games/PlayGames", "getAchievementsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/AchievementsClient;", PG_achievements),
    M("com/google/android/gms/games/PlayGames", "getLeaderboardsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/LeaderboardsClient;", PG_leaderboards),
    M("com/google/android/gms/games/PlayGames", "getPlayersClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/PlayersClient;", PG_players),
    M("com/google/android/gms/games/PlayGames", "getEventsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/EventsClient;", PG_events),
    M("com/google/android/gms/games/PlayGames", "getSnapshotsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/SnapshotsClient;", PG_snapshots),
    M("com/google/android/gms/games/GamesSignInClient", "isAuthenticated", "()Lcom/google/android/gms/tasks/Task;", SignIn_check),
    M("com/google/android/gms/games/GamesSignInClient", "signIn", "()Lcom/google/android/gms/tasks/Task;", SignIn_signIn),
    M("com/google/android/gms/games/GamesSignInClient", "requestServerSideAccess", "(Ljava/lang/String;Z)Lcom/google/android/gms/tasks/Task;", SignIn_serverAccess),
    M("com/google/android/gms/games/GamesSignInClient", "requestServerSideAccess", "(Ljava/lang/String;ZLjava/util/List;)Lcom/google/android/gms/tasks/Task;", SignIn_serverAccessScopes),
    M(HAUTHRESP, "getAuthCode", "()Ljava/lang/String;", AuthResp_code), M(HAUTHRESP, "getGrantedScopes", "()Ljava/util/List;", AuthResp_scopes),
    M(PLAYER, "getPlayerId", "()Ljava/lang/String;", Player_id), M(PLAYER, "getDisplayName", "()Ljava/lang/String;", Player_name),
    M(PLAYER, "getIconImageUrl", "()Ljava/lang/String;", Player_icon), M(PLAYER, "getHiResImageUrl", "()Ljava/lang/String;", Player_hires),
    M(PLAYER, "getTitle", "()Ljava/lang/String;", Player_title),
    M(PLAYER, "hasIconImage", "()Z", Player_hasIcon), M(PLAYER, "hasHiResImage", "()Z", Player_hasHiRes),
    M("com/google/android/gms/tasks/Tasks", "whenAll", "(Ljava/util/Collection;)Lcom/google/android/gms/tasks/Task;", Tasks_whenAll),
    M("com/google/android/gms/tasks/Tasks", "whenAll", "([Lcom/google/android/gms/tasks/Task;)Lcom/google/android/gms/tasks/Task;", Tasks_whenAll),
    M("com/google/android/gms/tasks/Tasks", "forResult", "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;", Tasks_forResult),
    M("com/google/android/gms/games/AuthenticationResult", "isAuthenticated", "()Z", Auth_isAuthenticated),
    M(HAUTH, "isAuthenticated", "()Z", Auth_isAuthenticated),
    M(HPLAYER, "getPlayerId", "()Ljava/lang/String;", Player_id), M(HPLAYER, "getDisplayName", "()Ljava/lang/String;", Player_name),
    M(HPLAYER, "getIconImageUrl", "()Ljava/lang/String;", Player_icon), M(HPLAYER, "getHiResImageUrl", "()Ljava/lang/String;", Player_hires),
    M(HPLAYER, "getTitle", "()Ljava/lang/String;", Player_title), M(HPLAYER, "toString", "()Ljava/lang/String;", Player_id),
    M(HPLAYER, "hasIconImage", "()Z", Player_hasIcon), M(HPLAYER, "hasHiResImage", "()Z", Player_hasHiRes),
    M(AC, "unlock", "(Ljava/lang/String;)V", Noop), M(AC, "increment", "(Ljava/lang/String;I)V", Noop),
    M(AC, "reveal", "(Ljava/lang/String;)V", Noop), M(AC, "setSteps", "(Ljava/lang/String;I)V", Noop),
    M(AC, "unlockImmediate", "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "incrementImmediate", "(Ljava/lang/String;I)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "revealImmediate", "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "setStepsImmediate", "(Ljava/lang/String;I)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "load", "(Z)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "getAchievementsIntent", "()Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(LC, "submitScore", "(Ljava/lang/String;J)V", Noop), M(LC, "submitScore", "(Ljava/lang/String;JLjava/lang/String;)V", Noop),
    M(LC, "submitScoreImmediate", "(Ljava/lang/String;J)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(LC, "getAllLeaderboardsIntent", "()Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(LC, "getLeaderboardIntent", "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M("com/google/android/gms/games/PlayersClient", "getCurrentPlayer", "()Lcom/google/android/gms/tasks/Task;", PC_currentPlayer),
    M("com/google/android/gms/games/PlayersClient", "getCurrentPlayerId", "()Lcom/google/android/gms/tasks/Task;", PC_currentPlayerId),
    M("com/google/android/gms/games/EventsClient", "increment", "(Ljava/lang/String;I)V", Noop),

    /* Firebase's helpers */
    M(GLVR, "getInstance", "()Lcom/google/firebase/platforminfo/GlobalLibraryVersionRegistrar;", GLVR_getInstance),
    M(GLVR, "registerVersion", "(Ljava/lang/String;Ljava/lang/String;)V", Noop),
    M(FB "Log", "getInstance", "()Lcom/google/firebase/app/internal/cpp/Log;", FBLog_getInstance), M(FB "Log", "shutdown", "()V", Noop),
    M(FB "Log", "v", "(Ljava/lang/String;Ljava/lang/String;)I", FBLog_line), M(FB "Log", "d", "(Ljava/lang/String;Ljava/lang/String;)I", FBLog_line),
    M(FB "Log", "i", "(Ljava/lang/String;Ljava/lang/String;)I", FBLog_line), M(FB "Log", "w", "(Ljava/lang/String;Ljava/lang/String;)I", FBLog_line),
    M(FB "Log", "e", "(Ljava/lang/String;Ljava/lang/String;)I", FBLog_line), M(FB "Log", "wtf", "(Ljava/lang/String;Ljava/lang/String;)I", FBLog_line),
    M(FB "CppThreadDispatcherContext", "<init>", "(JJJ)V", Ctx_init), M(FB "CppThreadDispatcherContext", "execute", "()V", Ctx_execute),
    M(FB "CppThreadDispatcherContext", "cancel", "()V", Ctx_cancel), M(FB "CppThreadDispatcherContext", "clear", "()V", Ctx_cancel),
    M(FB "CppThreadDispatcherContext", "acquireExecuteCancelLock", "()Z", Ctx_acquire),
    M(FB "CppThreadDispatcherContext", "releaseExecuteCancelLock", "()V", Noop),
    M(FB "CppThreadDispatcher", "runOnMainThread", "(Landroid/app/Activity;Lcom/google/firebase/app/internal/cpp/CppThreadDispatcherContext;)V", Dispatch_main),
    M(FB "CppThreadDispatcher", "runOnBackgroundThread", "(Lcom/google/firebase/app/internal/cpp/CppThreadDispatcherContext;)V", Dispatch_background),
    M(FB "JniResultCallback", "<init>", "(JJ)V", JRC_init2),
    M(FB "JniResultCallback", "<init>", "(Lcom/google/android/gms/tasks/Task;JJ)V", JRC_init3),
    M(FB "JniResultCallback", "cancel", "()V", JRC_cancel),
    M(FB "GoogleApiAvailabilityHelper", "makeGooglePlayServicesAvailable", "(Landroid/app/Activity;)Z", GAH_make),
    M(FB "GoogleApiAvailabilityHelper", "stopCallbacks", "()V", Noop),

    /* Play Billing */
    M(BC, "newBuilder", "(Landroid/content/Context;)Lcom/android/billingclient/api/BillingClient$Builder;", BC_newBuilder),
    M(BCB, "setListener", "(Lcom/android/billingclient/api/PurchasesUpdatedListener;)Lcom/android/billingclient/api/BillingClient$Builder;", BCB_self),
    M(BCB, "enablePendingPurchases", "()Lcom/android/billingclient/api/BillingClient$Builder;", BCB_self),
    M(BCB, "enablePendingPurchases", "(Lcom/android/billingclient/api/PendingPurchasesParams;)Lcom/android/billingclient/api/BillingClient$Builder;", BCB_self),
    M(BCB, "enableAutoServiceReconnection", "()Lcom/android/billingclient/api/BillingClient$Builder;", BCB_self),
    M(BCB, "enableAlternativeBillingOnly", "()Lcom/android/billingclient/api/BillingClient$Builder;", BCB_self),
    M(PPP, "newBuilder", "()Lcom/android/billingclient/api/PendingPurchasesParams$Builder;", PPP_newBuilder),
    M(PPPB, "enableOneTimeProducts", "()Lcom/android/billingclient/api/PendingPurchasesParams$Builder;", BCB_self),
    M(PPPB, "enablePrepaidPlans", "()Lcom/android/billingclient/api/PendingPurchasesParams$Builder;", BCB_self),
    M(PPPB, "build", "()Lcom/android/billingclient/api/PendingPurchasesParams;", PPPB_build),
    M(BCB, "build", "()Lcom/android/billingclient/api/BillingClient;", BCB_build),
    M(BC, "startConnection", "(Lcom/android/billingclient/api/BillingClientStateListener;)V", BC_startConnection),
    M(BC, "isReady", "()Z", RetFalse), M(BC, "endConnection", "()V", Noop), M(BC, "getConnectionState", "()I", RetZero),
    M(BC, "isFeatureSupported", "(Ljava/lang/String;)Lcom/android/billingclient/api/BillingResult;", BC_isFeatureSupported),
    M(BR, "getResponseCode", "()I", BR_code), M(BR, "getDebugMessage", "()Ljava/lang/String;", BR_message),
    M(BR, "toString", "()Ljava/lang/String;", BR_message),
    { NULL, NULL, NULL, NULL }
};

void tl_gms_install(void)
{
    if (getenv("TL_NO_GMS")) return;      /* debug knob: run a game without any of this, to compare */
    for (size_t i = 0; i < sizeof(k_classes) / sizeof(k_classes[0]); i++) tl_jni_declare(k_classes[i].name, k_classes[i].super);
    tl_jni_register_hle(k_gms);
}

/* ------------------------------------------------- on Husk's Java runtime */

/*
 * Husk's Java runtime (Unity 6 games, Java apps) runs the APK's own Java, Play services' client libraries included, and those
 * find no Play services to bind to ("unable to connect to service ... games.internal.connect.service.START"): a game asking
 * through them is never signed in. Its C# asks through JNI, and an answer registered here comes before the APK's Java for a
 * JNI call. So the Java runtime gets Play Games' entry points and clients -- the classes the Play Games Unity plugin calls,
 * which its ProGuard rules keep under these names -- and nothing else of the table above: not ClassLoader, Billing, Firebase's
 * helpers or Tasks, which the real framework and the APK's own Java answer there. Tasks are the APK's own (real_task), so
 * their listeners and Tasks.whenAll are the library's; the clients are objects of the APK's interfaces, the results of its
 * AuthenticationResult and PlayerEntity, whose methods are answered here, as reflection finds them declared there.
 */
#define PE PLAYER_ENTITY
static const tl_jhle k_pgs_java[] = {
    M("com/google/android/gms/games/PlayGamesSdk", "initialize", "(Landroid/content/Context;)V", PG_initialize),
    M("com/google/android/gms/games/PlayGames", "getGamesSignInClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/GamesSignInClient;", PG_signInClient),
    M("com/google/android/gms/games/PlayGames", "getAchievementsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/AchievementsClient;", PG_achievements),
    M("com/google/android/gms/games/PlayGames", "getLeaderboardsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/LeaderboardsClient;", PG_leaderboards),
    M("com/google/android/gms/games/PlayGames", "getPlayersClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/PlayersClient;", PG_players),
    M("com/google/android/gms/games/PlayGames", "getEventsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/EventsClient;", PG_events),
    M("com/google/android/gms/games/PlayGames", "getSnapshotsClient", "(Landroid/app/Activity;)Lcom/google/android/gms/games/SnapshotsClient;", PG_snapshots),
    M("com/google/android/gms/games/GamesSignInClient", "isAuthenticated", "()Lcom/google/android/gms/tasks/Task;", SignIn_check),
    M("com/google/android/gms/games/GamesSignInClient", "signIn", "()Lcom/google/android/gms/tasks/Task;", SignIn_signIn),
    M("com/google/android/gms/games/GamesSignInClient", "requestServerSideAccess", "(Ljava/lang/String;Z)Lcom/google/android/gms/tasks/Task;", SignIn_serverAccess),
    M("com/google/android/gms/games/GamesSignInClient", "requestServerSideAccess", "(Ljava/lang/String;ZLjava/util/List;)Lcom/google/android/gms/tasks/Task;", SignIn_serverAccessScopes),
    M("com/google/android/gms/games/AuthenticationResult", "isAuthenticated", "()Z", Auth_isAuthenticated),
    M("com/google/android/gms/games/PlayersClient", "getCurrentPlayer", "()Lcom/google/android/gms/tasks/Task;", PC_currentPlayer),
    M("com/google/android/gms/games/PlayersClient", "getCurrentPlayerId", "()Lcom/google/android/gms/tasks/Task;", PC_currentPlayerId),
    M(PE, "getPlayerId", "()Ljava/lang/String;", Player_id), M(PE, "getDisplayName", "()Ljava/lang/String;", Player_name),
    M(PE, "getIconImageUrl", "()Ljava/lang/String;", Player_icon), M(PE, "getHiResImageUrl", "()Ljava/lang/String;", Player_hires),
    M(PE, "getTitle", "()Ljava/lang/String;", Player_title),
    M(PE, "hasIconImage", "()Z", Player_hasIcon), M(PE, "hasHiResImage", "()Z", Player_hasHiRes),
    M(AUTHRESP, "getAuthCode", "()Ljava/lang/String;", AuthResp_code), M(AUTHRESP, "getGrantedScopes", "()Ljava/util/List;", AuthResp_scopes),
    M(AC, "unlock", "(Ljava/lang/String;)V", Noop), M(AC, "increment", "(Ljava/lang/String;I)V", Noop),
    M(AC, "reveal", "(Ljava/lang/String;)V", Noop), M(AC, "setSteps", "(Ljava/lang/String;I)V", Noop),
    M(AC, "unlockImmediate", "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "incrementImmediate", "(Ljava/lang/String;I)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "revealImmediate", "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "setStepsImmediate", "(Ljava/lang/String;I)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "load", "(Z)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(AC, "getAchievementsIntent", "()Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(LC, "submitScore", "(Ljava/lang/String;J)V", Noop), M(LC, "submitScore", "(Ljava/lang/String;JLjava/lang/String;)V", Noop),
    M(LC, "submitScoreImmediate", "(Ljava/lang/String;J)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(LC, "getAllLeaderboardsIntent", "()Lcom/google/android/gms/tasks/Task;", Task_failed),
    M(LC, "getLeaderboardIntent", "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;", Task_failed),
    M("com/google/android/gms/games/EventsClient", "increment", "(Ljava/lang/String;I)V", Noop),
    { NULL, NULL, NULL, NULL }
};

void tl_gms_install_java(void)
{
    if (getenv("TL_NO_GMS")) return;
    g_java = true;
    static const char *const own[] = { HTASK, HAUTH, HPLAYER, HAUTHRESP };      /* the fallbacks, where the APK lacks a class */
    for (size_t i = 0; i < sizeof(own) / sizeof(own[0]); i++) tl_jni_declare(own[i], "java/lang/Object");
    tl_jni_declare(HAPIEX, "java/lang/Exception");
    tl_jni_register_hle(k_pgs_java);
    tl_log_line("gms: Play Games answered by Husk on the Java runtime (PlayerEntity %s, TaskCompletionSource %s)",
                tl_dvm_defines_class(PLAYER_ENTITY) ? "found" : "missing",
                tl_dvm_defines_class("com/google/android/gms/tasks/TaskCompletionSource") ? "found" : "missing");
}
