# VHTG HUD lookup, process-exit watchdog and fresh USB logs

The user reports that the quest still overlaps the left Dynamic Island and the
walk/sit buttons remain in their old positions on the render-pool IPA. They also
ask to inspect the current failure directly over USB. This patch fixes selection
of those game windows, removes redundant host-layout writes and keeps USB log
sessions current after reinstalling Husk.

## Why the previous HUD adjustment did nothing

Fresh device logs identify build `4e85bdf`, enable the selected-HUD profile, and
bind both its native hooks. They then wait for `UI_TASKTRACK/Main`,
`UI_TASKTRACKTMP/Main`, `UI_POPBAR/BtnSit` and `UI_POPBAR/BtnRun`, without any
baseline/offset messages. This was not evidence of an old installed IPA: the
profile was enabled, but it never found the target windows.

The old profile incorrectly passed script-group names to `Wnd_FindWindow`.
That function compares the first argument with native root-window names and
searches the root's children for the second argument. `UI_TASKTRACK` is a
registered script group, while its root window is named `Main`.

The game itself uses `g_UIManager` → `KUIManage::GetUIGroup(groupName)` →
`KUIGroup::GetWindow(windowName)`. Original ARM64 call sites confirm the ABI and
that `g_UIManager` is the manager object, not a pointer variable. The profile now
uses these same exported methods. The loader supplies the writable data view
for the manager, as it does for the game's own GOT. All native object access
remains behind the exact package/library SHA-256 guard. Missing groups/windows
are retried; missing exports disable adjustments while native rendering continues.

Only the two quest roots and `BtnSit`/`BtnRun` are selected. The avatar, 18+ logo,
whole popbar and other controls are not selected. The full-screen background is
unchanged. No game resource file or executable instruction is patched.

Read-only inspection of the downloaded UI on the connected phone confirms the
group/window names and their screen anchors. With its observed 1665×768 scene,
2796×1290 framebuffer and 177-pixel left cutout, the model moves quest X from
66 to 106 scene units. Sit/run Y moves down by 21 scene units (about 12 screen
points). The actual buttons are screen-anchored children of a shallow 220×54
popbar: their movement must use screen bounds rather than that parent's bounds.

Local Unicorn execution of the original library reproduces all four NULL
results from the old lookup, and obtains the correct four windows through the
native manager/group methods, including their original STL lookup code. Only
libc byte/string primitives are stubbed. A separate eight-case run combines the
production C adjustment with original ARM64 anchor-layout and hit-rectangle
methods, including the phone's downloaded UI geometry. Other modeled HUD objects
remain byte-identical. These use synthetic registries/window objects, not a
live gameplay session. Private game binaries/resources remain local.

The previous host fixture returned a window for the incorrect lookup, hiding
this error. The regression fixture now requires the actual two-stage registry
API, checks the manager-object ABI, covers missing groups/exports and verifies
the observed device geometry without cumulative drift over 10,000 updates.

## Evidence from the current device

After reopening the USB Documents service, both the current and previous Husk
logs identify build `4e85bdf`, built 2026-11-06 at 02:26 UTC. The application's
Mach-O UUID in `Husk-2026-11-06-093116.ips` matches the delivered `4e85bdf` IPA.
Thus this is evidence from the new build, unlike the stale logs initially held
by the previous USB session.

The report at 09:31 local time records `EXC_CRASH/SIGKILL`, FRONTBOARD code
`0x8BADF00D`, `WatchdogEvent: process-exit`, and a failure to terminate gracefully
within five seconds. Its main-thread stack is in UIKit safe-area/layout updates
and QuartzCore's layout/transaction commit. This is a **process-exit watchdog**,
not proof that the app was killed for memory or for slow startup. A sampled
stack does not by itself establish which source operation caused the delay.

The prior game log reached 2,348 frames, with the last heartbeat showing 180
frames in its sampling interval and 2,108 MiB available before the process limit.
It contains the per-frame Objective-C pool marker and both VHTG HUD hooks. In
this captured run there is no fatal-signal line, `NSMallocException`, or OpenGL
`0x0505` texture-allocation error. That does not prove sustained gameplay is
fixed; the new run is much shorter than the previous six-minute texture crash.

The latest reopening log says JIT is not enabled yet and it is waiting for a
debugger. Game execution cannot be retested in that state; open Husk through
the user's existing StikDebug JIT flow first. This patch does not change JIT.

## Layout changes

The previous host resets the game's transform to identity, writes bounds and
center, then restores scale on **every** layout pass. The Java web host also
resets/restores its transform every time the game lays out. The captured run
changes from a 932×430 host to 932×410 while retaining a 932×430 guest surface,
so a non-identity scale is active. Repeating these writes needlessly invalidates
view geometry and safe-area work and is a plausible contributor to the observed
watchdog. Device confirmation is still required.

The host now computes a final placement and writes bounds, center or transform
only when that specific value differs. It uses bounds/center with the transform,
without intermediate identity or transformed-frame assignments. Child layout is
requested for real geometry changes, and separately for safe-area changes so HUD
metadata can still update without resizing the picture. The web host, stats
label and Metal drawable size likewise avoid redundant geometry writes.

Cutout calculation converts `window.bounds` inset by UIKit's existing
`window.safeAreaInsets`, avoiding a layout-guide frame query from inside the
game's layout pass. The physical-camera-edge selection, full-screen scene,
cached guest size, resolution and touch coordinate scheme are preserved.
Quest/button adjustments use the corrected registry lookup described above.
There is no forced exit, timeout suppression or exception catch.

The pure placement model is tested for the observed 932×430→932×410 transition,
stable repeated results, invalid geometry and restoration. A host test executes
the actual `TLWindowHost` class against counting UIView stand-ins to detect
geometry/layout writes over 10,000 stable passes, a real resize, safe-area-only
updates, Flutter sizing and a detached cached game. This is a model check, not
UIKit/device validation.

## USB session changes

The old collector could keep an outdated House Arrest session after sideloading
and report `following` even with `files: []`, leaving older local files visible.
Reopening the service immediately recovered all four current logs on this
device. The collector now:

- Reopens the current app container after two empty polls and periodically after
  30 seconds even when the old session still appears readable.
- Reports `waiting-for-logs` and `localFilesMayBeStale` when nothing was read.
- Sets `observedBuild` only from a freshly read current log's boot banner; a
  retained or replayed previous log cannot supply that field.
- Bounds each log read to eight seconds before retrying the USB connection.

This uses the existing local read-only USB flow; no device file or setting is
modified and no log is uploaded. The updated background collector has been
started locally and reads build `4e85bdf` successfully. The user approved
publication and the new IPA build; the delivered revision is recorded below.

## Validation

- Portable C checks and 28 Python analyzer/USB contracts: PASS on Windows.
- Original ARM64 registry lookup: PASS for both quest roots, sit and run;
  old calls return NULL, corrected calls select the expected windows.
- Production C with original ARM64 anchor/hit-rectangle layout: PASS in eight
  modeled cases, including the current phone UI geometry; other HUD unchanged.
- Nine USB contracts include empty-session state, reopen policy, periodic
  refresh, current-build provenance, bounded tails, replacement and locking.
- Real USB retrieval after reconnect: PASS; the live status names the correct
  build and reports the four files as available.
- Swift model and production-host tests: PASS on macOS and Linux CI, including
  10,000 stable host-layout passes. Windows has no Swift compiler.
- iOS UIKit behavior, physical HUD/touch positions and absence of watchdog after
  the patch: NOT TESTED. The new IPA has been built and verified below.

Primary references:
[Apple's watchdog guide](https://developer.apple.com/documentation/xcode/addressing-watchdog-terminations),
[UIView layout](https://developer.apple.com/documentation/uikit/uiview/layoutsubviews()),
[the USB House Arrest service implementation](https://github.com/doronz88/pymobiledevice3/blob/master/pymobiledevice3/services/house_arrest.py).
Raw device logs, `.ips` reports and UUID comparison remain local under ignored
directories and are not included in the source patch.


## Verified IPA delivery

Built revision `882adf9c6b508a7cdd29bdbffb78085931f56f8f`, stamped `882adf9-dirty`, produced
`IPA/HUNG-APK-PLAYER-882adf9.ipa` (113,680,274 bytes). All four CI jobs passed
in [the build run](https://github.com/hungtm200401-bot/Husk/actions/runs/42178926934); download the
[IPA and matching source artifact](https://github.com/hungtm200401-bot/Husk/actions/runs/42178926934/artifacts/13484816773).

IPA SHA-256: `1739e4d41c95f5f435f683d401756e3dee488bcfbc67e5582b319c6efe54bdff`.

Artifact ZIP SHA-256: `4bd215ecf15b874946908b4cc63562e96f10f94c700ddababcad0e81abe81ca2`, matching GitHub's published digest.
The source ZIP identifies the build revision and matches all ten approved files.
The arm64 app has the new UI-registry lookup marker and all three required
registry export strings, as well as the previous per-frame render-pool fix.
Required runtime components, Java full/core DEX hashes and unchanged embedded
entitlements have been verified.

macOS and Linux QA both executed the production Swift host convergence test and
the selected-HUD C contracts. macOS also passed the actual Foundation lifetime
test on a secondary POSIX thread. These checks establish the code and build
contents; physical HUD positions, touch behavior and sustained gameplay after
installing this IPA remain NOT TESTED.

The superseded `4e85bdf` custom IPA and downloaded packaging duplicates were
removed after the new delivery was verified. The sole current custom IPA is
kept directly in `IPA/`, with its checksum. The upstream dependency archive
remains separately in `IPA/support/`; matching source and verification are in
`build/releases/882adf9/`. Direct USB capture remains active for the next device
run; no game APK, device log, pairing record or signing certificate was published.
