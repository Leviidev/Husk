# The JIT substrate: how Husk gets W+X memory on iOS 27

Status: **verified by source reading** (not yet verified on device).
Sources read, all local clones, September 2026:

- `~/Documents/Coding/StikDebug-PiP` — StikDebug @ `94bc9e8`, origin `github.com/StikDebug/StikDebug`
- `~/Documents/Coding/MeloNX-Source` — Ryujinx-on-iOS, ships `BreakpointJIT.framework`
- `~/Documents/Coding/AetherCore4/aetherps4-public-release` — the user's own shadPS4 iOS port,
  which contains a production, on-device-proven implementation in
  `src/core/ios/ios_jit_allocator.cpp`

## Why the obvious approaches are dead

`mmap(PROT_READ|PROT_WRITE|PROT_EXEC)` does not work for a sideloaded iOS app. The
`dynamic-codesigning` entitlement that would allow it is only honoured for
platform binaries or on TrollStore/jailbroken devices. Hypervisor.framework is
not available to third-party iOS apps at all — confirmed by UTM's own
`Documentation/Architecture.md` — so hardware virtualization is off the table
and every guest instruction must go through TCG, which means we need a JIT, which
means we need writable-and-executable memory.

## TXM changes the rules, and on iOS 27 it is universal

Apple's Trusted Execution Monitor replaced the older page-protection scheme for
code-signing enforcement. StikDebug's `ProcessInfo+TXM.swift` encodes exactly
which devices are affected:

- iOS 26: TXM on iPhone12,x/A14-era and newer (`iPhone` HW version >= 14.2), iPads >= 14.5
- **iOS 27: TXM on everything except `iPad8,11` / `iPad8,12`**

Husk targets iOS 27 (the only SDK installed here is iphoneos27.0) on recent, high-RAM
iPhones. Every one of those devices is TXM-enforced. So the TXM-compatible path is
the *only* path — the older "just mark the region RWX once you're debugged" trick is
not a fallback we can keep in our back pocket.

## The mechanism: an RPC channel built out of breakpoints

The app cannot grant itself executable memory. A *debugger attached to it* can. So
the app asks the debugger to do it, and the request channel is the `brk`
instruction.

`BreakpointJIT.framework` exports three `naked` functions — nothing but a `brk`
and a `ret`:

```c
void* BreakGetJITMapping(void *addr, size_t len);  // brk #0xf00d, x16 = 1
void  BreakJITDetach(void);                        // brk #0xf00d, x16 = 0
void* BreakMarkJITMapping(size_t bytes);
```

StikDebug attaches over the debugserver/gdb-remote protocol and runs a JavaScript
script (`StikDebug/Scripts/universal.js`; MeloNX embeds a base64 copy, decoded here to
`research/decoded/stikdebug_jit26_universal.js`). The script is a plain
breakpoint-dispatch loop:

1. `vAttach;<pid hex>`
2. loop: `c` (continue) → target traps → parse the stop reply for `thread:`, `pc` (reg 20),
   and `x16` (reg 10)
3. read 4 bytes at `pc`, extract the `brk` immediate as `(instr >> 5) & 0xFFFF`
4. dispatch on the immediate, then `P20=<pc+4>` to step over the `brk`, then continue

Command numbers arrive in **x16**, arguments in **x0/x1**, and the return value is
written back into **x0** before resuming — a syscall ABI where the kernel is a
JavaScript interpreter on the other end of a USB cable.

### The one that matters: `JIT26PrepareRegion` (x16 = 1)

```
x0 = 0, x1 = size   ->   returns an executable region in x0
```

Two steps, and the second is the non-obvious one:

```js
let requestRXResponse = send_command(`_M${x1.toString(16)},rx`);   // allocate RX in the inferior
jitPageAddress = BigInt(`0x${requestRXResponse}`);
let prepareJITPageResponse = prepare_memory_region(jitPageAddress, x1);
send_command(`P0=${...jitPageAddress...};thread:${tid};`);          // hand the address back
```

`_M<size>,rx` is the debugserver packet for "allocate memory in the debugged
process with these permissions". That gets us an R-X region.

`prepare_memory_region` is native (`JSSupport/JSDebugSupport.swift` →
`handleJITPageWrite`), and all it does is, **for every single 16 KiB page in the
region**, send a gdb-remote `M<addr>,1:69` packet — a one-byte write of `0x69`,
through the debugger, into that page:

```swift
private let jitPageSize: UInt64 = 16_384
// builds "$M<9 hex digits>,1:69#<cksum>" per page, flushed 128 commands per batch
```

That is the whole trick. A debugger writing into the executable pages of a process
it controls is a permitted operation, and doing it forces each page into a state
where the app may thereafter write to it itself. Every page must be touched
individually; untouched pages stay read-only.

### Writing code: local `vm_remap`, no debugger involved

Once the RX region is prepared, the app makes its own writable alias of the *same
physical pages* and never needs the debugger again. From
`ios_jit_allocator.cpp`:

```cpp
vm_remap(mach_task_self(), &rw, bytes, 0, VM_FLAGS_ANYWHERE,
         mach_task_self(), (vm_address_t)rx, /*copy=*/FALSE,
         &cur_prot, &max_prot, VM_INHERIT_NONE);
vm_protect(mach_task_self(), rw, bytes, FALSE, VM_PROT_READ | VM_PROT_WRITE);
```

Result: `rw_addr` and `rx_addr` are two virtual addresses backed by one set of
physical pages. Emit code through `rw`, execute through `rx`, and call
`sys_icache_invalidate` on the `rx` side in between. This is textbook split-W^X, and
— importantly for us — it is the *exact* shape QEMU's TCG already expects.

Note `mach_vm.h` is not available in the iOS SDK; the classic `vm_map.h` API is used
instead, which is equivalent on 64-bit Darwin since `vm_address_t` is pointer-width.

## The constraint that shapes Husk's entire design

**Allocation is effectively one-shot.** After setup the app calls `BreakJITDetach()`
(`brk #0xf00d`, x16 = 0 → gdb `D`), the debugger goes away, and `BreakGetJITMapping`
stops being serviced. Worse, an unserviced `brk` is not a failed call that returns
an error — it is a real `SIGTRAP` that kills the process. `ios_jit_allocator.cpp`
carries a thread-local `g_expecting_jit_mapping_trap` flag purely so the signal
handler can tell "StikDebug missed this one" from a genuine breakpoint and
synthesize a `nullptr` return instead of dying; MeloNX has the same defence in
`JIT26Breakpoint.swift`, installing `SIGTRAP`/`SIGBUS` handlers that just do
`pc += 4; x0 = 0`.

StikDebug can also be killed mid-session by iOS's background wake-rate limiter —
the AetherPS4 source documents a Console.app capture of exactly that, a "cpulimit
violation".

So the rule, which matches the standing guidance on this user's other projects:
**size one allocation up front so that it is provably sufficient, and never design
around getting a second one.**

There is a real cost to sizing it large. `prepare_memory_region` sends one packet
per 16 KiB page over USB, with a response read per command:

| Region | 16 KiB pages | `M` packets |
|---|---|---|
| 256 MiB | 16,384 | 16,384 |
| 512 MiB | 32,768 | 32,768 |
| 1 GiB | 65,536 | 65,536 |
| 2 GiB | 131,072 | 131,072 |

This is a straight-line tradeoff between attach latency and JIT headroom, and it is
the first thing to measure on device.

## Why this fits QEMU unusually well

QEMU's TCG allocates its translation buffer **once, at init**, sized by
`-accel tcg,tb-size=N`, and it already supports split-W^X internally
(`tcg_splitwx_diff` — the RW and RX halves are a fixed pointer delta apart, exactly
what `vm_remap` produces). QEMU never needs to grow the buffer; when it fills up it
flushes and reuses it. A JIT with a one-shot, fixed-size code cache is precisely the
kind of JIT this substrate can support, which is a large part of why QEMU is the
right engine here rather than something that mmaps on demand.

The integration work is therefore narrow: replace QEMU's own code-buffer allocator
with one that calls `BreakGetJITMapping` + `vm_remap`, and let the rest of TCG's
existing splitwx machinery work unmodified.

## Packaging gotcha, learned the expensive way

`BreakpointJIT.framework` must **not** be linked by the Xcode target and must not
appear in `LC_LOAD_DYLIB`. If dyld auto-loads it at launch, AMFI rejects it on
sideloaded/free-provisioned builds — "has entitlements but is not a main binary" —
and the process `SIGABRT`s during static init, before `main()`. Copy it into the
bundle with a Run Script phase (not "Embed & Sign") and `dlopen` it lazily:

```c
dlopen("@executable_path/Frameworks/BreakpointJIT.framework/BreakpointJIT", RTLD_NOW | RTLD_LOCAL);
```

## Required entitlements

Confirmed from the shipping `AetherPS4-iOS.entitlements`:

```xml
<key>get-task-allow</key><true/>                                  <!-- lets StikDebug attach -->
<key>dynamic-codesigning</key><true/>                             <!-- honoured only on TrollStore/JB -->
<key>com.apple.developer.kernel.increased-memory-limit</key><true/>
```

The third one is what makes a multi-gigabyte Android guest survivable against jetsam,
and it is already proven to work in this user's sideloaded builds.
