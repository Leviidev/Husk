# VHTG quest coordinates and walk/sit buttons

The user reports that the quest panel still conflicts with Dynamic Island, and
requests moving the walk and stand/sit controls a little farther down on an
iPhone 15 Pro Max. They confirmed the installed IPA is `quest-only-fix` and the
island is on the left. Live window geometry has not yet been captured. The
following verified code defects are addressed; their role in the device symptom
still requires a device test.

## Changes

- **Consistent units.** UIKit supplies cutout depths in framebuffer pixels.
  `GameClientScene::update` supplies Cocos visible dimensions to
  `KT_SetUiAnchorSafeArea`; these are not necessarily framebuffer pixels.
  The native profile now receives the actual Java launch framebuffer dimensions
  and converts the cutout into the game UI scene with those dimensions. For
  example, a 177-pixel cutout on a 2796-pixel-wide framebuffer corresponds to
  59 units in a 932-unit-wide scene. Dividing by a 932-unit Cocos visible width
  would incorrectly reserve 177 scene units.
- **Apply before drawing.** The safe-area setter records geometry. A second,
  narrowly scoped PLT wrapper adjusts live windows immediately before the
  original `Wnd_RenderWindows` applies anchors and paints. This catches windows
  created or repositioned after the setter during the rest of the update.
  Both original functions are called, with unchanged arguments.
- **Independent baselines.** Script edits on one axis preserve the original
  baseline on the other axis. Removing the cutout restores the original offset
  instead of preserving a previous compatibility correction.
- **Axis-specific fit checks.** An oversized vertical quest container no longer
  prevents horizontal camera avoidance. A window that cannot fit along an axis
  containing a cutout is still left unchanged; the profile logs this condition.
- **Two movement buttons.** Only `UI_POPBAR/BtnSit` and `UI_POPBAR/BtnRun` receive
  a downward adjustment, separate from `UI_TASKTRACK/Main` and
  `UI_TASKTRACKTMP/Main`. The request is normalized to 12 screen points on a
  430-point-tall landscape surface. It is limited by both the bottom of the
  picture and the parent's native clip rectangle, so the displayed control and
  its entire hit rectangle remain inside those bounds. A shallow legacy parent
  may permit only a small movement. Avatar, 18+ logo, enclosing popbar, mount and
  PK controls retain their original window data.
- **Bounded diagnostics.** Startup logs identify both bound hooks, framebuffer
  and UI scene dimensions, missing target windows, baseline bounds and applied
  offsets. Layout event logging stops after 24 events per session.

The existing per-game `questOnlyCutout` setting key is retained for saved
settings. Its UI label is now **Adjust quest and walk/sit controls**. Keep it and
**Keep game controls clear of screen cutouts** enabled, then fully close/reopen
Husk. Movement adjustment applies only in landscape with a reported camera
edge; disabling the cutout restores its baseline.

The existing exact package, library size and SHA-256 guard remains in force.
The new render hook additionally requires symbol `_Z17Wnd_RenderWindowsv` and
the inspected original function offset `0x7cdbdc`; the setter retains its
`0x7cd9d0` guard. Unknown binaries retain generic cutout behavior. Full-screen
surface dimensions, JIT allocation and signing entitlements are unchanged.

## Local validation

- Windows portable C checks and 19 Python checks: PASS.
- Production profile contracts: PASS for package/hash/both PLT guards,
  framebuffer/visible-size differences, late window creation, left/right camera,
  independent script edits, tall quest containers, parent scaling, restoration,
  button bottom/parent bounds, IME preservation and 10,000-frame drift checks.
- Seven controlled native runs combine production C with the supplied APK's
  original ARM64 `applyAnchorLayoutTree` and `AbsoluteMove`: PASS. Left cutout
  avoidance is consistent at Low/High/Native pixel scales and with different
  Cocos visible sizes. Walk/sit draw and hit rectangles move together; a shallow
  parent clamps the movement. Unselected synthetic HUD objects remain
  byte-identical.
- The native runs use synthetic window trees and controlled loader/hash
  dependencies. They do not render the downloaded game UI or exercise physical
  iPhone touch input.
- Swift/Xcode/macOS/Linux: PENDING; Swift is unavailable on this Windows host.
- New IPA and physical iPhone validation: NOT BUILT / NOT TESTED.
- The previously reported exit during gameplay remains UNCONFIRMED.

The local native inspection, fixtures and APK-derived bytes remain under ignored
`build/`. They are not part of the source patch. No APK, raw device report,
account credential or signing material is included.

## Device check after an approved build

Confirm the installed build stamp first. With both cutout/profile toggles on,
fully restart Husk, then inspect VHTG in both landscape directions. The quest
panel should clear only the camera edge. Check quest links and walk/sit button
taps at their displayed positions, and confirm avatar and 18+ remain in place.
If it still conflicts, the new `vhtg-quest:` entries expose whether the inspected
profile bound, which windows exist and the geometry used for the adjustment.
