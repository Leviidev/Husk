/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Google Play Games for the translation layer's games, signed in with the person's own Google account.
 *
 * On a phone, Play services signs a game in: it asks Google for a token in the game's name (its package and signing
 * certificate) with the account on the phone, and reads the player that account is in that game from the Play Games API. Husk's
 * app does the same with the account signed in to its Store (TLPlayGames.swift), and the runtime answers the game's Play Games
 * calls from that: on Husk's Java runtime (Unity 6, Java apps) through husk.PlayGamesHost, and on the C side for the engines
 * whose Java Husk answers itself (husk-tl-jni-gms.c). A game that saves online under its player id (Idling to Rule the Gods)
 * then finds the same saves it has on Android.
 */
#ifndef HUSK_TL_PLAYGAMES_H
#define HUSK_TL_PLAYGAMES_H

#include <stdbool.h>

/*
 * The app's side. `op` is what is asked:
 *   "signin"     -- the player. Answer: "<playerId>\n<displayName>\n<iconUrl>\n<hiResUrl>\n<title>", or "!<why not>".
 *                   `flag` is 1 when the game asked to sign in (a button), 0 for the quiet check at start.
 *   "servercode" -- a server auth code for the game's own server: `arg` is the server's OAuth client id, `arg2` the scopes
 *                   (space separated, empty for Play Games' own), `flag` 1 to force a new refresh token. Answer: the code, or "!<why>".
 * `package` and `apk` are the game's package name and its (base) APK, whose signing certificate tells Google which game it is;
 * either may be NULL when the runtime does not know them (the app then uses the game it opened). The answer is malloc'd (the
 * caller frees it); NULL means no answer. The call blocks until Google has answered (seconds at most): never from a game's
 * render or audio thread.
 */
typedef char *(*husk_pgs_host_fn)(const char *op, const char *package, const char *apk, const char *arg, const char *arg2, int flag);
void husk_pgs_set_host(husk_pgs_host_fn fn);

/* The runtime's side. The game being started (husk-tl-unity-app.c), then what the host answers for it (malloc'd, or NULL). */
void tl_pgs_set_game(const char *package, const char *apk);
char *tl_pgs_ask(const char *op, const char *arg, const char *arg2, int flag);

/* The player, from a "signin" answer. False (and a reason in `why`, if given) when there is none. */
typedef struct tl_pgs_player {
    char id[128], name[128], icon[512], hires[512], title[128];
} tl_pgs_player;
bool tl_pgs_parse_player(const char *answer, tl_pgs_player *out, char *why, unsigned why_size);

#endif
