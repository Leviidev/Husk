# iPhone cutouts and gameplay stability

User evidence on 2026-11-05: VHTG reaches gameplay on iPhone 15 Pro Max,
some controls overlap the screen cutout, and Husk exits after playing for a while.
Resolution was previously reported as Low; the setting at the latest crash is unknown.
The existing local crash report is build `0dd57e1-dirty` and contains the older
`__strlen_chk` startup failure. It does not establish the cause of this gameplay exit.

## Display change

`Keep game clear of screen cutouts` defaults to on, including older settings files.
The host intersects its available area with the actual UIWindow safe frame, then
fits the complete game surface there. Existing surfaces keep their launch size
and aspect ratio when reopened or moved between a window and full screen;
UIKit's inverse view transform maps touches into the original surface coordinates.
Java web content follows the same bounds/transform and is reattached when a cached
surface moves to another host. Keyboard coverage is converted into game coordinates.
SDL, Java and Flutter receive only the insets still overlapping that surface,
avoiding a second inset after the host has already moved it clear of the cutout.
The keyboard continues to use guest IME insets without resizing the game surface.
The setting can be disabled for games that handle cutouts themselves.
Resolution choices are preserved; this change does not raise Low to Native.
Restart Husk completely when changing display settings for a loaded game.

## Frame allocation and crash evidence

The Java compositor now tracks CPU allocation capacity separately from displayed
texture dimensions. Both readback paths reuse that allocation and check growth
before reading or copying into it. A failed allocation keeps the last good frame
rather than losing the old pointer or copying into an undersized/null buffer.
This addresses concrete allocation hazards found in review; it is **not a confirmed
fix for the user's gameplay crash**.

Crash markers capture requested resolution/scale, safe-area mode and window mode at launch.
Viewport logs record actual surface scale and any cached launch size, since a loaded
engine may still retain its earlier settings until Husk is fully restarted.
Memory warnings log process footprint and thermal state; thermal transitions are
also logged while a game is active. A low available-memory reading is described
as evidence, not proof of jetsam, and no longer overrides an explicit fatal signal.

## Validation

- Windows portable C checks and 19 Python tests: PASS.
- New C test executes the production compositor copy function with controlled
  allocation failures, invalid dimensions, texture/CPU size changes, lock cleanup,
  and 100,000 frames with no further allocations after reaching capacity: PASS.
- Swift model tests added for legacy settings/roundtrip, cutouts, both orientations,
  fitting a cached surface without cropping, inverse touch geometry, zero double
  inset, and fatal-signal precedence. SKIP on this Windows host (no Swift compiler).
- macOS/Linux sanitizer QA, Xcode build and iPhone rendering/touch: PENDING.
- Sustained gameplay and the reported exit: NOT VERIFIED; fresh device report required.

## Device check after installing the new build

1. Keep cutout avoidance on. Start VHTG; verify controls near the cutout and Home
   indicator are visible and respond at their displayed positions in both landscape directions.
2. Hide/show Husk's bar, open/close the login keyboard, leave/reopen the game and
   check that the surface is not cropped, squeezed or inset twice.
3. Record the selected resolution. Compare High against the previous setting in
   the same scene before increasing to Native; record session length and heat.
4. If Husk exits, reopen it and use **Share Report** on the crash sheet. Save that
   new report locally under `build/` before another run replaces the previous log.
   If iOS provides a corresponding JetsamEvent/Husk `.ips` report, retain it locally
   to distinguish an OS memory kill from a native signal.

Keep device logs, game APKs and signing material out of commits. The existing IPA
`HUNG-APK-PLAYER-login-fix.ipa` does not contain this patch.
