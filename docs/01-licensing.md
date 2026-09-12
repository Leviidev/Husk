# Licensing and distribution: decided

The brief asked for this to be resolved before substantial original code gets
written. It is resolved. Every claim below was checked against the actual
LICENSE file in the actual source tree, not from memory.

## Decision

**Husk is GPL-2.0-or-later. The source is public. Distribution is a sideloaded
IPA plus the full corresponding source, never the App Store.**

Add `LICENSE` (GPL-2.0), and an SPDX header on every Husk-authored file:

```c
/* SPDX-License-Identifier: GPL-2.0-or-later */
```

## Why GPL-2.0, and why there was never really a choice

QEMU's own `LICENSE`, verbatim from `qemu-10.0.12-utm/LICENSE`:

> 1) The QEMU emulator as a whole is released under the GNU General
>    Public License, version 2.

Husk links QEMU into its own process as `libqemu-aarch64-softmmu.dylib`, and we
ship that dylib inside the IPA. That is distribution of a combined work, so the
combined work must be offered under GPLv2. Choosing `GPL-2.0-or-later` for
Husk's own files is the right shade: it is compatible with QEMU's GPLv2 (the
combination is simply GPLv2 in practice) while leaving the door open to GPLv3
for anything that later needs it.

The second, independent reason: `husk-ios-jit.c` is derived from AetherPS4-iOS's
`src/core/ios/ios_jit_allocator.cpp`, whose header reads

```
// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
```

so that file was going to be GPL-2.0-or-later regardless of QEMU.

## The trap worth naming: UTM's app code is Apache-2.0

`utm/LICENSE` is Apache License 2.0. **Apache-2.0 is not compatible with
GPLv2** (it is compatible with GPLv3, which does not help us, because QEMU as a
whole is GPLv2). So:

| Thing | License | Usable in Husk? |
|---|---|---|
| `utmapp/qemu` fork (the tarball we build) | GPL-2.0 — it *is* QEMU | **Yes** |
| UTM's `scripts/build_dependencies.sh` | ISC (header credits Angelo Haller, 2014) | **Yes** — permissive |
| UTM's Swift/ObjC app code, CocoaSpice, their UI | Apache-2.0 | **No — do not copy** |

Husk has so far taken the QEMU tarball and *ideas* from the build script
(toolchain triple, meson cross-file shape). No UTM application code has been
copied, and none should be. When Phase 0's display bridge gets written, it must
be written fresh rather than adapted from CocoaSpice.

## StikDebug is AGPL-3.0, and that is fine

`StikDebug-PiP/LICENSE` is the GNU Affero GPL v3. That would be a serious
problem if Husk linked it. Husk does not. StikDebug is a **separate application**
that attaches to Husk as a debugger over the gdb-remote protocol. There is no
linking, no shared address space, and no derived work — the only thing crossing
the boundary is a wire protocol, which is not copyrightable subject matter.

Users install StikDebug themselves from its own distribution. Husk should
document that dependency and link to it, not bundle it.

## BreakpointJIT: resolved by removing the dependency

`BreakpointJIT.framework` (bundle id `com.stossy11.BreakpointJIT`) ships as a
bare Mach-O with **no license file anywhere in its distribution**. MeloNX, which
vendors it, is MIT — but that covers Ryujinx's code, not a third party's binary
dropped into `Dependencies/`. An unlicensed binary is not something a GPL project
can take a dynamic-linking dependency on.

This turned out not to matter, because the framework is 11 instructions. `otool -tV`
disassembles the whole thing:

```
_BreakGetJITMapping:  mov x16, #0x1 ; brk #0xf00d ; ret
_BreakJITDetach:      mov x16, #0x0 ; brk #0xf00d ; ret
_BreakMarkJITMapping: brk #0x69     ; ret
```

Husk implements these itself in `src/ios-jit/husk-brk.S`, assembled to
byte-identical encodings. What is being reproduced is the *interface* to
StikDebug — the brk immediates and the x16 command numbers, all of which are
plainly documented in StikDebug's own AGPL JavaScript — not anyone's creative
expression. Three instruction pairs dictated entirely by an external protocol are
not a meaningful authorship contribution.

This is also better engineering. Dropping the framework removes the `dlopen`
indirection and, more importantly, removes the AMFI failure mode that forced the
lazy load in the first place: an embedded framework carrying entitlements is
rejected at launch on sideloaded builds ("has entitlements but is not a main
binary"), killing the process during static init before `main()`.

## Distribution

- **Public source repository**, complete and buildable. This is what satisfies
  GPLv2 §3 when we hand someone an IPA.
- **IPA releases** sideloaded via AltStore / SideStore / TrollStore, matching how
  AetherPS4 already ships.
- **Not the App Store.** Two independent blockers, either one sufficient: GPLv2's
  terms conflict with the App Store's distribution restrictions (the long-settled
  VLC question), and Husk fundamentally requires `get-task-allow` plus an external
  debugger attaching at runtime, which App Review does not permit.

## Bundled guest images — the one still-open item

This one is deferred to Phase 1, not answered here, but it should not be
forgotten:

- **Phase 0's Debian arm64 guest** is not redistributed. The build fetches it;
  it never goes in the repo or the IPA.
- **AOSP** is Apache-2.0 for userspace, but the **Linux kernel Husk boots is
  GPLv2**, which means shipping a kernel binary obliges us to offer its source
  too. Pin the exact kernel tag and keep the source link next to the image.
- **Google Play Services must not be bundled**, as the brief already states — the
  brief is right, and this is a licensing prohibition, not just a complexity
  argument.
