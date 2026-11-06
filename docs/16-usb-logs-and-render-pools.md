# USB logs, installation space and the VHTG render crash

The requested scope is direct log access on this Windows workspace, the reported
iPhone crash/installation failure, and removing old build duplicates. No game
resource, account, HUD position or graphics-quality setting is changed here.

## Installation and cleanup

The supplied Sideloadly screenshot reports signing failure `-72`,
`passthrough_copy: There is not enough space on the disk (os error 112)`.
The Windows C: drive had only 72,527,872 bytes free when checked, consistent with
this host-side copy failure; this is separate from the iPhone gameplay crash.

The explicitly requested cleanup removed 100 obsolete build deliveries and
duplicate downloaded/extracted Actions artifacts under `build/`, about 2.69 GiB.
Every recursive target was checked to remain inside the workspace's build
directory and to contain no reparse points. Source, game APKs, device evidence,
analysis dependencies and the pinned upstream support IPA were retained.
The deletion inventory is local at `build/cleanup-manifest.json`.

The current verified custom IPA is `IPA/HUNG-APK-PLAYER-4e85bdf.ipa`, 113,680,222 bytes, SHA-256
`e67139e5f398f37ce074a40cc7252fd915e8c8ba77f9d5b742d251baf523b8c9`.
The superseded `5bc7dcc` custom IPA has been removed after verification.
Corresponding source and verification are in `build/releases/4e85bdf/`.
The pinned upstream reference archive is kept separately in `IPA/support/`.
Select the current custom IPA in Sideloadly to retry signing after freeing
space. Signing/installing on the iPhone has not been repeated by this agent.

## Direct logs over USB

`scripts/ios_logs.ps1` runs the companion `capture_ios_logs.py` on Windows.
It uses the optional, separately installed
[pymobiledevice3](https://github.com/doronz88/pymobiledevice3) version 11.50.0.
No change to the installed Husk IPA is required; its existing File Sharing
support exposes Documents through the trusted USB connection.

```powershell
.\scripts\ios_logs.ps1 -Setup       # First setup only; isolated Python environment.
.\scripts\ios_logs.ps1 -Background  # Hidden collector; automatically reconnects.
.\scripts\ios_logs.ps1 -Once        # One snapshot instead of following.
.\scripts\ios_logs.ps1 -Stop        # Graceful stop after the current USB operation.
```

Keep the iPhone connected, unlocked as needed and trusted by Windows.
Open `device-logs/latest/husk.log` for current output, `husk-prev.log` and
`husk-native-prev.log` for the last run, and `crashes/` for matching iOS reports.
`status.json` records connection state, collector PID, copy time and file sizes.
The collector reads only the selected Husk app's four log files, plus Husk crash
reports and recent Jetsam snapshots that mention Husk. It never uploads logs or
writes/removes files in the application. iOS's crash-report service is asked to
make pending reports available, and reports are copied without removal.

Local logs are bounded to 16 MiB per file; up to eight Husk and two Jetsam reports
are retained, at most 8 MiB each. Replacements/truncation are detected, only new
bytes are fetched after the first snapshot, and an OS file lock prevents two
collectors from writing the same destination. `IPA/` and `device-logs/` are
ignored by Git. No pairing record, device log or crash report is in this patch.

The collector was tested against the connected iPhone 15 Pro Max: all four log
files and iOS `.ips` reports were retrieved successfully. This also works while
Husk is closed, while the USB service remains available. USB disconnection,
device lock/restrictions or a missing install can interrupt access; the collector
records a waiting state and retries. It is not a wireless or cloud logger.

## Crash evidence and targeted fix

The pasted report was from build `b2b44b0` and did not contain its final fatal
lines. Direct USB retrieval supplied the complete previous-run logs and
`Husk-2026-11-06-085403.ips`. At roughly 360 seconds, they record:

- Repeated OpenGL errors `0x0505` during texture creation.
- `NSMallocException`: allocation of `MTLTextureDescriptorInternal` failed.
- `SIGABRT`; the exception stack enters ANGLE's Metal texture creation from
  `GL_DrawElements`, followed by Husk's Java `gl_main` rendering thread.
- The last periodic available-memory reading was 1,607 MiB. This is an allocation
  exception, not evidence that iOS jetsam killed this run. A separate older
  process-exit watchdog report is not classified as this same crash.

Inspection found that the C-created Java GL thread has no Objective-C autorelease
scope at initialization or within its long-running frame loop. Apple requires
secondary threads using Cocoa to provide their own pools, and recommends draining
temporary objects within long-running loops; see
[Apple's autorelease-pool guide](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MemoryMgmt/Articles/mmAutoreleasePools.html).

`husk-tl-autorelease.h` provides C scopes using the same runtime push/pop operations
that Clang emits for `@autoreleasepool`. The Java GL thread now has an outer scope
for initialization and an inner scope for every active frame. Clang cleanup
attributes balance scopes on normal exits, early returns and `continue`.
Owned renderer objects remain retained normally. There is no exception swallowing,
forced GL error clearing, graphics-quality downgrade, texture-cache rewrite or
JIT allocator change.

The missing pool is a confirmed lifecycle defect; whether this correction prevents
the observed texture-allocation crash must still be verified in sustained device
play. Other texture/resource pressure can also cause allocation failure.

## Validation before push

- Six USB mirror contracts: PASS for incremental reads, replacement/truncation,
  bounded tails, one-writer locking and selecting only an unambiguous Husk bundle.
- Real USB snapshot and background collector on the user's iPhone: PASS.
- Portable C checks, 19 analyzer/tool tests and 100,000-frame autorelease scope
  balance: PASS on Windows. Mock pools verify C cleanup behavior, not Apple memory
  usage.
- A new macOS test uses a real secondary POSIX thread and Foundation objects to
  check per-frame deallocation, retained objects and early-return cleanup. It is
  wired into the portable CI runner: PASS on macOS CI.
- iOS SDK/Xcode build and a new IPA containing this fix: PASS; verified below.
- Sustained iPhone gameplay after the fix: NOT TESTED.


## Verified render-pool IPA

Built revision `4e85bdf9f4d19f6f82d58bb9e685b60c964a24b2`, stamped `4e85bdf-dirty`, produced
`IPA/HUNG-APK-PLAYER-4e85bdf.ipa` (113,680,222 bytes). Download the
[IPA and corresponding source artifact](https://github.com/hungtm200401-bot/Husk/actions/runs/42149710824/artifacts/13472276927).
All four CI jobs completed successfully in [the build run](https://github.com/hungtm200401-bot/Husk/actions/runs/42149710824).

IPA SHA-256: `e67139e5f398f37ce074a40cc7252fd915e8c8ba77f9d5b742d251baf523b8c9`.

Downloaded artifact ZIP SHA-256: `4d8d2fffe113e57a5d46f264f5b4d02685494c0fb21d35ede857bb4e36c5b0c4`, matching GitHub's published
digest. The source ZIP identifies the build revision and matches the approved
files. The arm64 app contains the new per-frame temporary-object diagnostic and
Objective-C pool runtime references, along with the previous HUD implementation.
Embedded full/core Java DEX hashes match the tested framework, required runtime
components are present and entitlements match the unchanged source.

The macOS CI test exercises actual Foundation deallocation on a secondary POSIX
thread. This verifies the pool behavior; sustained gameplay on the physical
iPhone after installing this new build remains NOT TESTED. Direct USB logs remain
the source of evidence for the next device run.

Only this current custom IPA is retained directly in `IPA/`; the pinned upstream
reference remains under `IPA/support/`. Corresponding source and verification
are under `build/releases/4e85bdf/`. Downloaded packaging duplicates and the
superseded custom IPA are removed after the new delivery has been verified.
