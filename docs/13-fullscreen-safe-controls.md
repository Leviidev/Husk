# Full-screen background with cutout-aware controls

The user clarified that the background must cover the entire iPhone screen,
including behind Dynamic Island; only controls should move away from the camera.
The previous `97914c0` build fitted the entire scene inside the safe area and
therefore did not meet this requirement. This change supersedes that display policy.
It retains that build's frame-allocation guards and crash logging.

## Evidence from the supplied VHTG APK

The original `libMyGame.so` has a usable separation between scene size and HUD anchors:

- `GameClientScene::update(float)` calls `Director::getSafeAreaRect()` at ELF
  address `0x83b8fc`, then calls `KT_SetUiAnchorSafeArea` at `0x83b984`.
- `GLViewImpl::getSafeAreaRect()` reads `Cocos2dxHelper.isCutoutEnabled()` and
  `getSafeInsets()` using JNI. The array is bottom/left/right/top, in render pixels.
- The APK's `AppActivity.onCreate` enables short-edge cutout layout. Its Java
  `getSafeInsets()` returns zeros unless `DisplayCutout.getBoundingRects()` is nonempty.
- Husk's previous DisplayCutout implementation always returned an empty list,
  even when safe depths were nonzero. Its ViewRoot also constructed a cutout
  without any bounds. Consequently the game's helper could not use those insets.

Analysis uses the same APK previously supplied by the user. No game binary,
resource pack, account data or raw device log is included in this patch.

## Changes

For VHTG, [quest-only positioning](14-vhtg-quest-only-cutout.md) refines the
generic HUD policy below after the user reported that other control groups moved.

The full-screen host now uses its complete bounds. Cutout avoidance never reduces
the render area or adds safety margins around the whole picture. Cached surfaces
still keep aspect ratio and touch transforms when moving between hosts. Close Husk
fully before testing a display change so the game starts at the new full-screen size.

The existing per-game preference is retained, now labelled
**Keep game controls clear of screen cutouts**. With it on, iPhone layout reports
the actual camera edge to Android/SDL/Flutter. In landscape, the opposite safe-area
margin and Home-indicator margin are not reported as additional cutouts. Updates
are sent when layout/insets change. Husk's bar and virtual controller use their
own safe layout, independently of the native game's full-screen background.

DisplayCutout now retains defensive copies of bounds, returns nonempty bounds for
the affected edge, and exposes the corresponding safe depths. ViewRoot constructs
this metadata without subtracting it from windows that request short-edge layout.
The game can then use its own HUD-anchor logic while drawing the scene at full size.
iOS exposes safe depths, not the precise Dynamic Island mask; the metadata uses a
conservative edge rectangle rather than claiming the exact physical camera shape.

This does not reposition arbitrary controls in games that ignore cutout metadata.
The supplied VHTG has the relevant API and native HUD-anchor path; actual display
and touch positions remain to be checked on the user's iPhone.

API references: [Apple interface orientation](https://developer.apple.com/documentation/uikit/uiinterfaceorientation/landscapeleft),
[Android DisplayCutout](https://android.googlesource.com/platform/frameworks/base/+/main/core/java/android/view/DisplayCutout.java),
[Cocos cutout helper](https://github.com/cocos2d/cocos2d-x/blob/v4/cocos/platform/android/java/src/org/cocos2dx/lib/Cocos2dxHelper.java).

## Validation before publication

- Full Java framework javac and D8 full/core build on Windows: PASS.
- Existing dialog lifecycle tests: PASS.
- New JVM test uses production DisplayCutout/WindowInsets and the APK helper's
  public API sequence. It checks all camera edges, zero frame insets with nonzero
  HUD insets, absent cutouts, invalid sizes, bounds clamping and defensive copies: PASS.
- Portable C checks and 19 Python tests: PASS.
- The original APK's ARM64 `GLViewImpl::getSafeAreaRect()` was run in Unicorn with
  controlled JNI responses and a modeled GLView rectangle/scale. No-cutout and all
  four camera-edge cases produce the expected HUD rectangle: PASS. This is a
  native geometry check, not rendered gameplay or touch verification.
- Swift geometry tests for full-screen size and one-edge HUD metadata:
  SKIP on Windows (no Swift toolchain); PASS on macOS and Linux CI.
- Portable sanitizer QA on macOS/Linux, Java dialog/cutout contracts and Xcode
  iOS build: PASS in [run 41988946317](https://github.com/hungtm200401-bot/Husk/actions/runs/41988946317).
- Packaged IPA: VERIFIED. iPhone full-screen/HUD/touch: NOT TESTED.

## Verified delivery

The built revision is `439564c1b3cc7d2526e847b0a4f1588985971a5d`, stamped
`439564c-dirty`. Local file: `build/HUNG-APK-PLAYER-fullscreen-hud-fix.ipa`
(113,675,123 bytes). Download the
[IPA and matching source artifact](https://github.com/hungtm200401-bot/Husk/actions/runs/41988946317/artifacts/13396399502).

IPA SHA-256: `9d4cf2fa86fa258512540f06fb6de80a1c34b6e5bf4f8c1d799f4fa804e0d1db`.

The downloaded archive checksum matches the CI upload log. The executable's
revision, full-host viewport/cutout markers, native components and unchanged
entitlements were checked. Both embedded Java DEX files match the locally tested
and CI-tested framework hashes. The source ZIP identifies the built commit and
matches its changed files. This verifies packaging, not gameplay on a physical iPhone.

After installing the new build, keep the control-cutout preference on, restart
Husk fully and check the scene reaches all screen edges. Test the controls near
Dynamic Island in both landscape directions. The previously reported gameplay
exit still requires a fresh crash report if it occurs again.
