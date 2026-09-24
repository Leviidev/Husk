# Husk architecture

Husk runs unmodified Android APKs on iOS by booting a real ARM64 Android guest
under QEMU's TCG, inside the host app's own process, and presenting one app's
rendered surface full-screen with no Android chrome visible.

This document records what has been **verified against source or built**, and
marks clearly what has not yet run on a device.

An experimental second runtime, which runs an app's own code without booting
Android at all, is planned in [04-translation-layer.md](04-translation-layer.md).

## The stack

```
┌─────────────────────────────────────────────────────────────┐
│ Husk.app (Swift/ObjC, GPL-2.0-or-later)                     │
│   library grid · full-screen presentation · Metal blit      │
│   touch -> qemu_input_queue_abs/btn                         │
├─────────────────────────────────────────────────────────────┤
│ DisplayChangeListener  (registered from the app side)       │
├─────────────────────────────────────────────────────────────┤
│ libqemu-aarch64-softmmu.dylib   QEMU 10.0.12-utm, GPLv2     │
│   qemu_init() / qemu_main_loop() on a pthread               │
│   TCG, MTTCG, split-W^X via tcg_splitwx_diff                │
├─────────────────────────────────────────────────────────────┤
│ husk-ios-jit.c + husk-brk.S     RX from StikDebug,          │
│                                 RW alias via vm_remap       │
├─────────────────────────────────────────────────────────────┤
│ StikDebug (separate app, AGPL-3.0) attached as debugger     │
└─────────────────────────────────────────────────────────────┘
```

## Decisions already made, and why

**No hypervisor.** Hypervisor.framework is unavailable to third-party iOS apps
(UTM's own `Documentation/Architecture.md` says so). UTM does ship a
`build_hypervisor` step for iOS using a decompiled Hypervisor framework, but that
needs `com.apple.private.hypervisor`, which means a jailbreak. Husk targets
sideloading, so TCG it is. This is the single largest performance constraint and
it is not negotiable.

**QEMU, specifically `qemu-10.0.12-utm`, not upstream.** Upstream QEMU has no way
to build itself as a library, and iOS cannot spawn processes, so QEMU must live
in-process. UTM's fork adds `-Dshared_lib=true`, which emits
`libqemu-aarch64-softmmu.dylib` exporting `qemu_init` / `qemu_main_loop` /
`qemu_cleanup`, plus the two small adaptations that a library build needs
(`rcu_register_thread` moves into `qemu_init`; the non-library constructor path
is skipped). Upstream v11.1.1 was considered and rejected: porting that
capability forward buys nothing Phase 0 needs.

**TCG's split-W^X is upstream, so almost nothing needed patching.** osy's 2020
"tcg: implement bulletproof JIT" commit in `utmapp/qemu` — written for exactly
this problem, "on iOS, we cannot allocate RWX pages without special
entitlements" — was upstreamed as `tcg_splitwx_diff`. UTM's current 107 KB iOS
patch touches 28 files and **none of them are under `tcg/`**. Husk's only TCG
change is a single new allocator function.

**Same-ISA does not mean free.** The brief says ARM64-on-ARM64 means "no
cross-ISA translation overhead to pay". That is half right: there is no
instruction-set mismatch, but system-mode TCG still translates guest → TCG IR →
host and emulates the MMU in software. Expect a large constant-factor slowdown
against native, not near-native speed. The conclusion is unchanged — TCG is the
only option — but Phase 1 timings should be predicted accordingly, and MTTCG
(guest vCPUs on separate host threads) matters a lot on a multicore phone.

## The JIT path

Fully documented in [02-jit-substrate.md](02-jit-substrate.md). In brief: on
iOS 27 every target device enforces TXM, so executable memory can only be
granted by an attached debugger. Husk traps to StikDebug with `brk #0xf00d`
(x16 = 1), receives a debugger-allocated RX region whose pages have each been
individually touched through the debugger, and makes its own RW alias with
`vm_remap`. QEMU then uses the pair directly as `tcg_splitwx_diff`.

The allocation is **one-shot** — StikDebug detaches afterwards and an unserviced
`brk` is a fatal SIGTRAP, not an error return. QEMU suits this unusually well
because TCG sizes its code buffer once at init from `-accel tcg,tb-size=N` and
flushes rather than grows.

## Display and input

`register_displaychangelistener()` is a public QEMU API, so the display bridge
needs **no QEMU patch at all**. The app registers a `DisplayChangeListener` and
receives `dpy_gfx_switch` (new surface) and `dpy_gfx_update` (dirty rect)
callbacks carrying a `DisplaySurface` backed by a pixman image; those pixels get
blitted into a `CAMetalLayer`. Input goes the other way through
`qemu_input_queue_abs` / `qemu_input_queue_btn` / `qemu_input_event_sync` against
a `virtio-tablet-pci` (absolute coordinates map directly from touch points, with
no pointer-capture problem to solve).

Deliberately *not* adapted from UTM's CocoaSpice: that code is Apache-2.0, which
is incompatible with GPLv2. See [01-licensing.md](01-licensing.md).

## Phase 0 status

| Component | State |
|---|---|
| Dependency set cross-compiled for arm64-apple-ios | **built** — libffi, glib 2.83, pixman, libucontext, libslirp |
| `libqemu-aarch64-softmmu.dylib` for iOS | **built** — verified `platform 2` (iOS), minos 16.0, SDK 27.0 |
| `husk-brk.S` | **built** — disassembly byte-identical to BreakpointJIT.framework |
| `husk-ios-jit.c` | **compiles clean** (`-Wall -Wextra`), iOS arm64 |
| TCG splitwx diverted to the iOS allocator | **patched**, `patches/husk-qemu-ios-jit.patch` |
| Phase 0 guest (Alpine 3.24.1 aarch64) | **fetched**, sha256 verified |
| JIT allocation + execute self-test on device | **PASS** — iPhone18,1 / iOS 27.0, 749 ms for 256 MiB |
| Guest boots under TCG on device | in progress |
| Guest reaches userspace on device | not yet |
| Frames reach Metal, touch reaches guest | not yet |

Nothing in the unproven rows should be described as working until it has been seen
working on hardware. The JIT row moved to PASS only because the self-test executed
generated code from the RX alias and returned 42 on a real device.
