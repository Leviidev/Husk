# Play Games sign-in, and Unity 6 on the Java runtime

Made so that Idling to Rule the Gods (`de.shugasu.itrtg`, Unity 6000.6) plays on an iPhone with its online saves, which it
keeps on its own server under the player's Play Games ID. What changed:

1. **Google Play Games signs in with the person's own Google account** (the one in the Store tab). A game's Play Games
   calls then get the same player as on Android, so its online saves reach the same data.
2. **Unity 6 fixes:**
   - The loader kept only 95 bytes of a library's path (`tl_lib.name[96]`). A Unity 6 game's libraries live under
     `java-data/lib`, with paths of 160 bytes and more, so `is_unity_lib` never matched. Unity's writable data then ended up
     in JIT memory, and the game died at start with SIGBUS.
   - Unity 6 runs its memory on mimalloc, which tells threads apart by the thread pointer: it reads `tpidr_el0` and takes
     the word there as the thread's id. The loader turns every `mrs tpidr_el0` into the address of one thread block per
     library (`patch_image`), so mimalloc took every Unity thread for the same one. A free from another thread was done as
     a local free, and memory still in use was handed out again. Loading.PreloadManager and Loading.AsyncRead died at start
     on pointers holding old data, and later a job worker died on `0xc0dedbad`. Unity reads `boot.config` out of the APK
     with its own zip reader, so its mimalloc setting cannot be changed on the way, and Unity documents no command-line
     argument for it. On the Java runtime, a Unity whose `libunity.so` has mimalloc in it is now started with
     `-systemallocator` (Unity's documented Android switch). It goes into the launch intent's `unity` extra when the engine
     first reads the intent through JNI (`tl_jni_before_java`). `TL_UNITY_SYSTEM_ALLOCATOR=1|0` decides instead, and
     `TL_UNITY_ARGS` adds arguments. With this, ITRTG plays, and survives switching apps and starting again.
   - On the Java runtime, Unity 6 hands each frame to the screen on the GPU (a shared texture) instead of reading it back
     through the CPU on every swap. **Game Settings › Frame Handoff** sets this per game, and **CPU Copy** keeps the old
     path.
3. **Switching apps.** A phone that leaves an app takes its window's focus, pauses it and stops it, and reverses that on
   return. Husk's framework only paused and resumed it (`AppRunner.hostPause`). Unity tells the game about the focus
   (OnApplicationFocus), and a player that allows a resizable window pauses on onStop, not onPause, so ITRTG did not count
   its time away. For a Unity game on the Java runtime, `Native.pollInput` (the app's main thread, before the framework
   handles the host's pause or resume) now makes the missing steps: focus lost, onPause, onStop; then onRestart, onStart,
   onResume, focus back. The log shows `javaapp: left the screen` / `back on the screen`, and Unity's `nativePause`,
   `nativeResume` and `nativeFocusChanged` as they are called (a new RegisterNatives filter, `tl_jni_native_wrap`, wraps them
   to log). Pausing is also what makes a game save, so Husk asks iOS for up to 8 seconds of background time while a game is
   loaded, to let that save finish before iOS suspends Husk.
4. **Debugging:**
   - **Game Settings › Debugging › Detailed Log** turns on the file and network traces (`TL_FILE_TRACE`, `TL_NET_TRACE`)
     for one game. A path under `/proc` or `/sys` is traced once: Unity polls every core's `cpu_capacity` and
     `cpuinfo_max_freq` for as long as it runs.
   - The crash report names SIGTRAP and SIGBUS too.
   - `net: the game looks up <host>` notes each server a game contacts once, and no longer says `-- not found` for an
     `AI_NUMERICHOST` check of a host name.

## Using it

1. Sign in to the **Store** tab with **Sign In with Google**, using the account the game is played with on Android. The
   guest (Aurora) account has no Play Games player. If Play is pressed on a game that uses Play Games with no Google
   account in Husk, Husk asks once whether to sign in first.
2. On the game's page in the Library, **Online Saves › Check Play Games Sign-In** shows the Play Games player and its ID.
3. The game signs in by itself at start, as on Android.

## What says why, when something is wrong

- **The game's log** (the game's page › Technical Details › Share Log):
  - `gms: Play Games signed in (player …)` or `gms: Play Games signed out: <reason>`.
  - `gms: Play Games <call> -> <answer>`: each Play Games call the game made and what it got (`isAuthenticated`,
    `getCurrentPlayer`, the `Player` getters, `Tasks.whenAll`, `requestServerSideAccess`). For `requestServerSideAccess`
    only the fact that a code was issued is logged, never the code.
  - `unity: command line "-systemallocator"`: the player was started with malloc for its memory.
  - `javaapp: left the screen …` / `back on the screen …`, and `unity: nativePause` / `nativeResume` /
    `nativeFocusChanged(…)`: switching apps, and what Unity was told.
- **Online Saves › Copy Sign-In Details** copies the build, the game, the SHA-1 of the APK's signing certificate, the
  Google account, the player and the last sign-in error. It has no passwords and no tokens. The certificate SHA-1 should
  match the one Google Play lists for the game; an APK re-signed by someone else will not sign in.
- **Husk's own log** has the same steps under `pgs`.

## How the sign-in works

On Android, Play services signs a game in. It asks `android.clients.google.com/auth` for a token in the game's name. The
request carries the game's package and the SHA-1 of the certificate its APK is signed with, which tells Google which game's
Play Console project the token is for. Play services then reads the player from
`www.googleapis.com/games/v1/players/me`.

Husk makes the same two requests (`src/app/Husk/Play/PlayGamesSignIn.swift`):

- it uses the long-lived account token from the Store sign-in, which Husk already keeps for Play Store downloads;
- it uses the game's own package, and the certificate digest read from the APK's v3/v2 signing block or its v1 PKCS#7 file;
- Play services is named as the caller, and the scope is `games_lite`.

The answer is a real player that Google returns for that account in that game. Husk never invents an ID and never claims a
sign-in Google did not make. With no Google account in Husk, the game is told the player is signed out, as before.
`requestServerSideAccess` (a code a game's own server trades with Google) is answered the same way, with
`oauth2:server:client_id:<the game's server client>:api_scope:…`.

The runtime asks through `husk_pgs_set_host` (`src/translation-layer-next/husk-tl-playgames.h`). The game's C# reaches Play
Games through JNI, and Husk's answers in `husk-tl-jni-gms.c` come before the APK's Java. Unity 6 games run on Husk's Java
runtime, which runs the APK's own Play services client. That client finds no Play services to bind to (`unable to connect
to service … connect.service.START`), so the Java runtime gets its own table, `tl_gms_install_java`, which holds only Play
Games' entry points.

On the Java runtime, Unity finds methods with Java reflection, which only sees classes in the APK. So the answers there are
objects of the APK's own classes:

- the Tasks are real ones from its `TaskCompletionSource`, so their listeners and `Tasks.whenAll` are the library's own;
- the results are its `AuthenticationResult` and `PlayerEntity`, whose getters Husk answers.

The Play Games Unity plugin's ProGuard rules keep all of these under their names. These calls are answered:

- `GamesSignInClient.isAuthenticated` / `signIn` / `requestServerSideAccess` (both forms);
- `PlayersClient.getCurrentPlayer` / `getCurrentPlayerId`, and the `Player` getters;
- `Tasks.whenAll`, which the Play Games Unity plugin (2.x) waits on after sign-in.

The objects made where the APK lacks a class (tasks, results, players) use classes of Husk's own (`husk/gms/…`). On the
Java runtime, a field that the APK's class does not declare is kept once per class rather than once per object. Each task's
result would then have overwritten the last one's, and the plugin would have read no player after `whenAll`.

When the game's screen opens, the sign-in starts (`PlayGamesSignIn.gameStarting`), so the answer is usually ready before
Unity asks for it. The player is remembered for the session. It is also remembered on disk, per account and game, so that
offline the game keeps the same identity, as Play services would.

## If the sign-in fails

- **The account's sign-in in Husk has expired:** sign out of the Store and sign in again.
- **The account has no Play Games profile:** open Google Play Games once on an Android device, or at
  play.google.com/games, then try again.
- **Google wants consent (`NeedPermission`):** sign in to the game once on an Android device with that account.
- **The picture is wrong (black, frozen, torn):** in the game's **Game Settings**, set **Frame Handoff** to **CPU Copy**.
