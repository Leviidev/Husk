# VHTG: move only the quest tracker

This records the first quest-only implementation and its delivered IPA. The
follow-up [selected HUD adjustment](15-vhtg-selected-hud.md) corrects coordinate
units and timing, and adds the separately requested walk/sit button adjustment.

The user reported that build `439564c` moves the avatar, 18+ logo and walk/sit
controls along with the quest tracker. The requested behavior is a full-screen
scene with only the quest tracker moved out of the camera edge. The general
Cocos safe-area rectangle affects every screen-anchored HUD group and cannot
express this selection.

## Implementation

VHTG's per-game settings now include **Move only the quest tracker**, defaulting
to on for legacy settings as well. With the existing cutout option on, this
profile keeps Android's global HUD cutout at zero and privately retains the
camera-edge depth for the selected quest windows. Thus the general Cocos safe
rectangle keeps the other HUD groups in their original positions.

The native profile selects only `UI_TASKTRACK/Main` and `UI_TASKTRACKTMP/Main`
through the game's own `Wnd_FindWindow` export. It runs after the existing
`KT_SetUiAnchorSafeArea` call, before `Wnd_RenderWindows` reapplies anchor layout.
For anchored windows it translates only the quest's anchor offset; for legacy
unanchored windows it uses the game's `SetPosition` method. It calculates the
baseline from the window's own anchor, pivot, dimensions and parent scale,
avoiding accumulated movement across frames and preserving external layout edits.
An already clear quest window is not shifted. Removing the cutout restores its
original offset. The full render size and non-quest window objects are untouched.
Oversized windows that cannot fit in the remaining area are left unchanged.

This is intentionally limited to package `review.com.game3c.vhtgmobile` and the
supplied arm64 `libMyGame.so`, size 14,643,976 bytes and SHA-256
`0c468ae6407377f61d33f1bbe77928b1d5c32cdede98ab163b86a897f0b60dcb`.
The inspected object-field offsets are used only after this hash check.
Unknown binaries disable this profile and retain the general cutout behavior.
The runtime logs this condition. The setting requires a complete Husk restart.

The loader adds an optional filter for PLT function slots, checked against the
calling library, symbol and expected original function offset. It does not filter
data relocations, function pointers in vtables, or dlsym results. The original
function remains callable and is always called before quest adjustment. The
filter resets with the runtime session. This adds no executable instruction patch,
JIT allocator change, entitlement change, or persistent edit to game assets.

Raw APK resources were inspected locally under ignored `build/` to identify UI
groups and anchor behavior. They are not included in the patch or source tree.

## Validation before publication

- Windows portable C checks and 19 Python checks: PASS.
- Production quest-profile C contracts: PASS for package/hash/PLT guards, global
  cutout masking with IME preservation, left/right camera cases, parent scale,
  resolution changes, external layout edits, legacy windows, no-quest cases,
  restoring offsets, and no drift over 10,000 updates. Loader/hash/window objects
  are controlled fixtures; the host test is not a cryptographic implementation
  test or thread race detector. Device code uses Apple CommonCrypto SHA-256.
- The production PLT relocation case is tested for data relocations/addends,
  function slots, failed resolution, weak/null slots and sessions with no filter: PASS.
- A controlled Unicorn run combines the production C profile with the APK's
  original ARM64 `applyAnchorLayoutTree` and `AbsoluteMove`, using a synthetic
  quest/avatar/18+/walk-sit window chain: PASS. At 2796×1290, a modeled 177-pixel
  left cutout moves the quest from `(10,160)` to `(59,160)` in the modeled 3× UI
  coordinates; all other window objects remain byte-identical. The original
  native function updates the quest draw/hit rectangle consistently. Right-edge
  and absent-camera cases keep an already clear left quest in place.
- Swift settings roundtrip: SKIP on Windows (no Swift toolchain); PASS on
  macOS and Linux CI.
- Production quest-profile/PLT contracts, portable sanitizer QA on macOS/Linux,
  Java framework contracts, native-module iOS SDK check and Xcode build: PASS in
  [run 42003272097](https://github.com/hungtm200401-bot/Husk/actions/runs/42003272097).
- Physical iPhone drawing/touch and long gameplay stability: NOT TESTED.

## Verified IPA delivery

Built revision `b2b44b04219dd96e9a61a6e7b54372de9d8df752`, stamped
`b2b44b0-dirty`, produced `build/HUNG-APK-PLAYER-quest-only-fix.ipa`
(113,679,872 bytes). Download the
[IPA and matching source artifact](https://github.com/hungtm200401-bot/Husk/actions/runs/42003272097/artifacts/13404400535).

IPA SHA-256: `48f5b6702844410e0b3c0f1ea77777d767532fda63f93219d90f36c7644f2838`.

The downloaded archive checksum matches the CI upload log. The arm64 executable
contains the quest-only setting, native-profile functions, PLT-filter symbol and
full-screen viewport markers. Embedded Java full/core DEX hashes match the tested
framework, native components are present, and entitlements match the unchanged
source. The source ZIP identifies the build revision and matches its reviewed
files. These checks verify packaging and revision identity, not device gameplay.

After installing this build, keep both **Keep game controls clear of screen cutouts**
and **Move only the quest tracker** on for VHTG, close Husk completely, then reopen it.

The controlled native check does not verify the actual downloaded UI assets,
screen rendering, physical touch handling or the previously reported gameplay exit.
After installation, test VHTG in both landscape directions: only the quest panel
should move when it overlaps the camera edge. Verify its quest links still respond
at the displayed position, with avatar, 18+ and walk/sit controls at their original
positions. Keep a new crash report locally if Husk exits during play.
