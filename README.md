# Husk

Run unmodified Android APKs on iOS. Drop in an APK, it appears in a library with
its own real icon and name, and tapping it opens that app full-screen — no
status bar, no nav bar, no launcher, no VM window.

Underneath, a real ARM64 Android guest runs under QEMU's TCG inside the host
app's process, with JIT memory obtained from an attached debugger. The user
never sees any of that.

**Status: Phase 0, pre-hardware.** The substrate builds; nothing has been run on
a device yet. See [docs/00-architecture.md](docs/00-architecture.md) for exactly
what is verified and what is not.

## Licence

GPL-2.0-or-later. Husk links QEMU, which is GPLv2, so the shipped binary is a
combined GPLv2 work and the full source is public. It cannot go on the App
Store — both because of that and because it needs `get-task-allow` plus a
debugger attaching at runtime. See [docs/01-licensing.md](docs/01-licensing.md).

## What you need

- An iPhone on iOS 26 or 27 with plenty of RAM. Every iOS 27 device enforces TXM,
  so the debugger-assisted JIT path is mandatory.
- [StikDebug](https://github.com/StikDebug/StikDebug), installed separately. Husk
  does not bundle it — it is AGPL-3.0 and runs as its own app.
- Xcode 27 with the iOS 27 SDK, and Homebrew with `meson`, `ninja`, `pkg-config`.

## Building

```bash
./scripts/fetch_sources.sh        # QEMU 10.0.12-utm + 5 dependencies
./scripts/build_ios.sh            # cross-compiles everything for arm64-apple-ios
./scripts/fetch_phase0_guest.sh   # Alpine 3.24.1 aarch64 kernel + initramfs
```

`build_ios.sh` is stage-based and idempotent — `./scripts/build_ios.sh qemu`
rebuilds just QEMU. Per-stage logs land in `build/logs/`.

Output: `build/ios-arm64/sysroot/` and
`third_party/build/qemu-10.0.12-utm/_husk_build/libqemu-aarch64-softmmu.dylib`.

## Layout

```
docs/           architecture, licensing, the JIT protocol writeup
src/ios-jit/    husk-brk.S + husk-ios-jit.c -- the whole JIT substrate
patches/        our QEMU patch, plus the UTM patches we reuse
scripts/        fetch + cross-build
research/       decoded StikDebug JIT protocol script
guests/         guest images (gitignored)
third_party/    upstream sources (gitignored)
```

## Non-goals for v1

No Google Play Services in the guest, so Google Sign-In, Maps, FCM push and Play
Billing apps are out of scope and will be surfaced as unsupported rather than
failing silently. No GPU passthrough — heavy 3D is a stretch goal. Older and
low-RAM devices are not targeted.

## Credits

The iOS JIT work this stands on is not ours: osy/UTM for making QEMU work on iOS
at all and for the split-W^X design now upstream in QEMU, Stossy11 for the
BreakpointJIT trap protocol, the StikDebug project for the debugger side, and the
shadPS4/AetherPS4-iOS `ios_jit_allocator.cpp` that `husk-ios-jit.c` is derived
from.
