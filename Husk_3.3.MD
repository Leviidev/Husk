# Husk 3.3: in-process AOSP

**Headline:** Java apps run on a real AOSP `framework.jar` inside Husk, not only on the hand-written `src/java-framework` stand-ins. QEMU/LineageOS stays for a full guest; 3.3 is the translation-layer Java stack.

3.2 shipped JIT-less games. 3.3 is the old **M4** from `Husk_3.2.MD`, which that plan said must not block 3.2.

## Why

`husk-framework.dex` is hundreds of stubs. Reflection, newer APIs, and `ActivityThread` fall through. The real `framework.jar` already knows how to start an activity; Husk has to look like Linux enough for it to boot.

## Stages

| Stage | What | Done when | Status |
| --- | --- | --- | --- |
| **B1 Binder** | `husk-tl-binder.c`: `/dev/binder` ioctl in-process | `tests/translation-layer/run_binder.sh` ping/pong | done |
| **B2 Service manager** | handle 0 is `servicemanager`; `addService` / `getService` | a tiny native client looks up a service | done (`sm: add/get ping ok`) |
| **B3 Boot path** | optional `framework.jar` after `husk-framework.dex` | missing `android.*` classes come from AOSP; AppRunner still starts the APK | done |
| **B4 system_server slice** | ActivityManager / PackageManager / WindowManager over Binder | those names resolve for `ServiceManager.getService` | done |
| **B5 Default** | more apps, not a pure AOSP boot | Husk UI/lifecycle stays first; AOSP fills classes Husk does not define | done |
| **B6 Binder replies** | ping / INTERFACE / display / power / package answers, not 64 zeros | `run_binder.sh` INTERFACE on `activity` returns `IActivityManager` | done |

## How to turn it on

`framework.jar` is Apache-2.0 and is **not** copied into Husk's sources. From a Lineage/AOSP system image:

```
scripts/copy_aosp_bundle.sh /path/to/system
```

That fills `build/java-runtime/aosp/`. `scripts/build_java_runtime.sh` does the same unless `HUSK_AOSP=0`. Packaging already rsyncs `build/java-runtime/` into the IPA.

Without that folder, Java apps use only `husk-framework.dex` and `husk.AppRunner`.

## Compatibility model

Dropping Husk's framework in favour of `ActivityThread.main` does not run more apps: AOSP's View/Window/Resources need SurfaceFlinger, AssetManager natives, and a real AMS, which empty Binder replies do not provide.

The boot class path is therefore:

1. libcore (ART APEX)
2. **`husk-framework-core.dex`** — handwritten Activity, View, resources, `husk.AppRunner` (the path that can start an APK today). Generated empty stubs are **not** in this dex.
3. **`framework.jar`** — real bytecode for the ~2 200 platform classes that used to be empty `gen-src` stand-ins

Without `framework.jar`, the fat `husk-framework.dex` (core + generated stubs) stays on the path so those class names still exist.

That is the compatibility win: Play/F-Droid apps that reference platform classes our stand-ins never included get real AOSP bytecode, without losing the lifecycle and GL path that already works.

Binder services now answer ping, `INTERFACE_TRANSACTION`, screen size, “interactive”, and permission-granted. That is enough for AOSP stubs to construct. Typed `PackageInfo` / `Configuration` parcels and a full `ActivityThread` boot are still later.

## Constraints

- One process, jetsam ceiling: no full `system_server` copy of every service.
- Dalvik is still an interpreter; a GC is a follow-up if `framework.jar` churns too much memory.
- Games that already run on the translation layer keep using today's path. AOSP is for **Java apps**.
