# The translation layer: Android apps without booting Android

Status: **experimental, first stretch built.** Settings › Experimental › Android
Translation Layer exists. Apps can be added there and each gets a report of
what running it would take, and the phone can be asked the questions the design
depends on. **Nothing opens an app yet.** The rest of Husk is unaffected either
way.

Sources read for this document, September 2026:

- Android Translation Layer (ATL) `b4bd403` (2026-09-23),
  `gitlab.com/android_translation_layer/android_translation_layer`
- its `bionic_translation` `b36c17d` and `art_standalone` `66a5d90`
- AetherPS4 `bachata-core` `b7127f0` (2026-09-11), the FEXCore port

## Why

Husk runs an APK by booting a whole Android system under QEMU's TCG and showing
one app's surface. It works, and before the first frame it costs a kernel, an
init, `system_server`, zygote and minutes of emulated CPU, all translated through
a software MMU (see [00-architecture.md](00-architecture.md)).

ATL shows that none of that is needed to run an app. Its own architecture notes
put the cut "directly between the Apps and the Java APIs provided by the android
frameworks": keep the app's Dex and its `.so` files, run them in a normal host
process, and give them a reimplementation of the framework. On iOS that means no
guest at all, and an app's native code runs on the real CPU rather than under
TCG.

## What ATL actually is

Read from the sources, not from its README:

| Piece | What it is | Licence | On iOS |
|---|---|---|---|
| `art_standalone` | ART, libcore and their dependencies, "frankenstained" onto AOSP's android-6.0.1 dalvik branch and built for glibc/musl hosts. About 20,000 files. | Apache-2.0 (AOSP); libcore's OpenJDK parts GPLv2 + Classpath Exception | **Port.** `kPageSize` is a compile-time `4096` (`libartbase/base/globals.h`), and iOS pages are 16 KiB. The JIT code cache already supports a dual RW/RX view (`jit_code_cache.cc`: `exec_pages_` / `non_exec_pages_`), which is exactly the shape of Husk's `vm_remap` pair. Only its `memfd_create` allocation has to change. |
| `bionic_translation` | Loads bionic-linked `.so` files on glibc/musl: the old AOSP linker (BSD), plus libc and pthread wrappers from android2gnulinux | Linker BSD; the wrappers carry **no licence notice at all** | **Replace.** Its libc shim targets glibc/musl structure layouts, and the wrappers cannot be used anyway: an unlicensed file is not something a GPL project can take (compare BreakpointJIT in [01-licensing.md](01-licensing.md)). |
| `src/api-impl` | The Android framework, reimplemented in Java: 819 files, 329 of them carrying AOSP's Apache header | GPL-3.0 (ATL) + Apache-2.0 | Java runs anywhere ART runs, **but see Licensing.** |
| `src/api-impl-jni` | The framework's native half: 83 C files on GTK4, Wayland, EGL, WebKitGTK, ALSA, libavcodec, libportal | GPL-3.0 | **Rewrite** against UIKit, Metal and CoreAudio. None of GTK/Wayland exists on iOS. |
| `src/libandroid` | The NDK's `libandroid.so` over GTK, EGL and Vulkan | GPL-3.0 | **Rewrite.** EGL/GLES comes from ANGLE, which Husk already builds for iOS. |

## Option 1 or option 2

**Option 1, an iOS-native translation layer.** Husk ships ART built for
`arm64-apple-ios`, a framework layer whose native half is UIKit/Metal, and its
own loader for the app's arm64 `.so` files.

**Option 2, run ATL under FEXCore.** Not a good fit, and the reason is in the
FEXCore port itself. `bachata-core` builds FEXCore *only*
(`runtime/patches/fex-fexcore-only.patch` adds `BUILD_FEXCORE_ONLY`). That is
FEX's x86-64 → arm64 JIT, with FEX's Linux frontend (its ELF loader and syscall
layer) left out, and AetherPS4's HLE bridge supplying the OS. FEXCore translates
*x86* code. The work a translation layer does on an iPhone is arm64 through and
through: ART built for arm64, and the `arm64-v8a` libraries almost every APK
carries. There is nothing for FEX to translate unless everything is first
rebuilt for x86-64, which then costs a translation on every instruction the
iPhone could have run directly. FEX's Linux frontend passes system calls to a
Linux kernel, so it would not help either: the Linux → Darwin work remains.
(QEMU's user mode is no shortcut for the same reason: it only runs on Linux and
BSD hosts.)

Where FEXCore *does* have a place is later and narrow. An APK whose native code
is x86-only could have just those libraries run through FEXCore, bridged to the
same framework the way AetherPS4 bridges HLE calls. The scanner already
recognises those apps ("Native code for x86 only").

**Decision: option 1.** It is also what was preferred.

## Licensing: the constraint that decides the layout

Every piece of an Android runtime worth reusing is under a licence that cannot
be combined with QEMU's GPLv2 in one program:

- ART and AOSP's framework code: **Apache-2.0**, which is incompatible with
  GPLv2. (This is the same trap as UTM's app code in
  [01-licensing.md](01-licensing.md).)
- ATL itself: **GPL-3.0**, also incompatible with GPLv2-only code.

Husk links `libqemu-aarch64-softmmu.dylib` into its own process today. A
process that also loads ART is one combined work containing GPLv2-only and
Apache-2.0 code, which nobody can distribute.

What this change does about it: **everything under `src/translation-layer/` is
Husk's own code, GPL-2.0-or-later, and none of it comes from ATL or AOSP.** It is
compiled into the app, not into the QEMU library. Nothing licence-sensitive
ships yet. The decision is needed **before milestone 2**, the first time ART
enters the build. The realistic choices:

1. **A separate build of Husk without QEMU** for the translation layer.
   Unambiguous.
2. **One IPA, never both in one process lifetime.** QEMU and the runtime become
   separately `dlopen`ed libraries, and Husk loads one or the other per launch.
   Whether that counts as "mere aggregation" is a legal question, not an
   engineering one, and is not decided here.

Husk-written code stays GPL-2.0-or-later either way. Under GPLv3 it can go
alongside ATL-derived or Apache code, and under GPLv2 alongside QEMU.

## The target

```
┌───────────────────────────────────────────────────────────────┐
│ Husk.app — library, one UIView/CAMetalLayer per Activity      │
├───────────────────────────────────────────────────────────────┤
│ framework (Java, dex)          │ framework native half         │
│   android.app, .view, .widget  │   UIKit/Metal/CoreAudio,      │
│                                │   ANGLE for EGL/GLES          │
├───────────────────────────────────────────────────────────────┤
│ ART for arm64-apple-ios — interpreter first, JIT on Husk's    │
│ RW/RX pair later                                              │
├───────────────────────────────────────────────────────────────┤
│ Husk's loader — the app's arm64 .so files into JIT memory,    │
│ bionic's ABI mapped onto Darwin's libc                        │
├───────────────────────────────────────────────────────────────┤
│ JIT substrate (StikDebug / MAP_JIT / TrollStore), shared with │
│ QEMU mode once it is its own library                          │
└───────────────────────────────────────────────────────────────┘
```

## The hard problems, and how each is being answered

Each item says whether it is **measured**, **handled** or still **open**.

### Executable memory for libraries: measured on the phone

A library's code has to execute, and its data has to be writable, a fixed
distance away: compiled code finds its globals PC-relatively (`adrp`). So each
library image needs ordinary memory directly beside executable memory. Husk's
JIT memory comes in one region, granted once, so the loader has to carve
images out of it. Two layouts are possible, and the device checks try both with
real generated code:

- **carve**: map executable memory, then replace the data pages inside it with
  ordinary memory;
- **place**: reserve ordinary memory, then map an executable page at a chosen
  address in front of the data.

Both pass on arm64 Linux under qemu-user (`tests/translation-layer/run.sh`),
which proves the code the checks generate. Whether XNU allows either is
what the phone answers. The checks run where MAP_JIT executes (TrollStore,
and iOS versions where a debugger is enough). The region a *trap-servicing*
debugger grants belongs to the allocator compiled into the QEMU library, so that
case is reported as not measured. Measuring it means moving
`src/ios-jit/husk-ios-jit.c` into a small library of its own, which both modes
link.

Budget: the region is one-shot (see [02-jit-substrate.md](02-jit-substrate.md)),
so it must hold every library an app loads, plus ART's code cache. A Unity
IL2CPP game's `libil2cpp.so` alone is tens of megabytes.

### 16 KiB pages: handled, per library

iOS pages are 16 KiB. A library linked for 4 KiB pages can have code and
writable data in the same 16 KiB page, which would have to be executable and
writable at once. The scanner lays every arm64 library out on 16 KiB pages
exactly as the loader will (`tl_page_plan` in `husk-tl-elf.c`) and counts those
pages. Writable bytes under `PT_GNU_RELRO` are not counted: they are written only
while relocating, which the loader does through the RW alias. Since November 2025
Google Play has required 16 KiB-compatible native code in new apps and updates
that target Android 15 or later, so current APKs from Play come out clean. Older ones show up as
"needs work", and the fix is write emulation for those few pages.

### The thread register: measured on the phone

Clang's Android target reads the stack-protector cookie from
`TPIDR_EL0 + 0x28` in every protected function. The test suite confirms this on
the fixtures: `mrs x8, TPIDR_EL0; ldr x9, [x8, #0x28]`. On glibc, ATL gets
away with it because glibc's thread pointer is where bionic's slots can be made
to live. Darwin keeps its thread pointer in `TPIDRRO_EL0` instead, so
`TPIDR_EL0` may simply be free. The `tpidr` check measures that: whether it is
zero, whether a value set on four threads survives hundreds of context switches,
and whether it survives into a signal handler. If it passes, Android code runs
as it is. If it fails, every `mrs Xn, tpidr_el0` has to be rewritten at load
time, and the scanner already counts them per library.

### x18: measured on the phone

Clang reserves x18 for both Android and iOS targets (checked: it allocates x18
for `aarch64-linux-gnu` and never for `aarch64-linux-android` or
`arm64-apple-ios`), so ordinary Android code never touches it. Code built with
shadow call stacks, and possibly libraries from older compilers, would. The
`x18` check measures whether XNU preserves it.

### Direct system calls: counted, open

bionic makes system calls itself, so the shim replaces them wholesale. But some
code issues `svc #0` with Linux numbers inline (Go's runtime, some anti-tamper
code), and iOS would treat those as Darwin calls. The scanner counts `svc`
instructions per library. The loader will have to patch each one into a call to
the shim.

### bionic on Darwin's libc: open

This is the largest mechanical part of the loader. Structure layouts (`stat`,
`dirent`, `sigaction`), `errno` values, `open`/`mmap` flags, signal numbers,
`pthread_mutex_t` sizes (40 bytes on bionic arm64, 64 on Darwin), and bionic's
`__sF` stdio all differ. The scanner's per-app "Android libraries it needs"
list is the list of what has to be provided.

### 32-bit apps: not possible natively

Apple's CPUs have not executed AArch32 code since the A11. An APK with only
`armeabi-v7a` libraries would need a CPU emulator, and the scanner says so.

### ART: open

Built for `arm64-apple-ios`, with `kPageSize` = 16 KiB. It starts
interpreter-only: `-Xusejit:false`, and no boot-image dex2oat (ATL's README
gives the same workaround for Apple Silicon Linux). The JIT follows once the
code cache sits on Husk's RW/RX pair.

## Milestones

| # | What | State |
|---|---|---|
| M0 | Setting, the translation layer's own APK store, per-app reports, device checks | **built** — host-tested; not yet run on a phone |
| M1 | Device checks pass on hardware; the JIT substrate becomes its own library so the trap-servicer case can be measured too | not started |
| M2 | ART for `arm64-apple-ios`, interpreter only, runs a `main()` from a Dex file in-process. **Licensing decision first.** | not started |
| M3 | Husk's loader: an arm64 `.so` into JIT memory, relocated against a first bionic shim; `JNI_OnLoad` runs | not started |
| M4 | Framework with a UIKit native half: a pure-Java app shows an Activity with text and buttons | not started |
| M5 | GLES through ANGLE: a NativeActivity game draws | not started |
| M6 | Audio, input, IME, storage, network | not started |
| later | ART's JIT on the RW/RX pair; FEXCore for x86-only libraries | — |

Nothing above M0 should be described as working until it has been seen working
on hardware.

## What exists now

- `src/translation-layer/`, in C and compiled into the app:
  - `husk-tl-zip.c`: an APK reader (ZIP, ZIP64, stored and deflated entries),
    written for untrusted input.
  - `husk-tl-elf.c`: ELF analysis, including the 16 KiB page planner the loader
    will reuse, and decoders for Android's packed relocations (APS2) and RELR.
  - `husk-tl-scan.c`: the per-app report.
  - `husk-tl-probe.c`: the device checks.
- `src/app/Husk/TranslationLayer.swift`: the setting, the APK store
  (`Documents/TranslationLayer/`, apart from Android's), the reports and the
  checks screen.
- `tests/translation-layer/run.sh`:
  - the page planner on layouts worked out by hand;
  - the scanner on real arm64 Android libraries built by clang and lld, with
    relocation, import and stack-guard counts checked against `llvm-readelf`
    and `llvm-objdump`;
  - the device checks, run as arm64 code under qemu-user.

  `fuzz.c` is a libFuzzer harness for the two parsers. Neither has crashed in
  300,000 runs under ASan and UBSan.
