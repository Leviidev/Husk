# Phase 0 runbook — taking it to the device

Everything below the device line is built and, where it could be, verified on the
host. This is what remains, and it needs the phone.

## What is already proven

| Claim | How it was checked |
|---|---|
| QEMU + deps cross-compile for arm64-apple-ios | `otool -l` on the dylib: `platform 2` (iOS), minos 16.0, SDK 27.0 |
| QEMU builds as an in-process library | `libqemu-aarch64-softmmu.dylib` exports `qemu_init`, `qemu_main_loop`, `qemu_cleanup` |
| Husk's JIT + display API is reachable from the app | all 9 `husk_*` symbols show as `T` in `nm -gU` |
| The brk trap sequences are correct | `otool -tV` output is byte-identical to BreakpointJIT.framework |
| The guest boots under pure TCG | booted on the host: Linux 6.18.35 to userspace in ~5 s |
| The guest gives us a framebuffer and a pointer | host dmesg: `fb0: virtio_gpudrmfb`, `QEMU Virtio Tablet`, `QEMU Virtio Keyboard` |
| The app compiles, links and bundles | `BUILD SUCCEEDED`; 51 MB bundle with dylib, guest, shaders, script |

## Measured on device (iPhone18,1, iOS 27.0, 11.7 GB RAM)

First successful JIT bring-up:

```
#1: StikDebug attach probe OK (0xcccccccc690000e0)
#1: requesting execute-capable region, size=268435456 (256.0 MiB), attempt 1/3
#1: BreakGetJITMapping returned 0x115254000          <- 749 ms later
#1: dual mapping established: rw=0x125254000 rx=0x115254000 diff=-268435456
selftest: PASS -- executed generated code from the RX alias, got 42
```

| Measurement | Value | Implication |
|---|---|---|
| `BreakGetJITMapping`, 256 MiB | **749 ms** (16,384 pages) | ~46 us per page over the debugger. 1 GiB would cost ~3 s, so `tb-size` has real room to grow for Android |
| Footprint after JIT alloc | 274.9 MiB phys | On 11.7 GB this is irrelevant; the caution behind `-m 1024` was unnecessary |
| Footprint before `qemu_init` | 13.9 MiB | App overhead is negligible |
| Probe to allocation start | 80 ms | StikDebug round trips are cheap |

The dual-mapping `diff` is negative (`rx` sits below `rw`), which is fine --
`tcg_splitwx_diff` is a signed offset.

## What is NOT proven, in the order it will break

1. **StikDebug services our `brk`.** Everything rests on this.
2. **The JIT region is big enough, and arrives fast enough.** 256 MiB is 16,384
   debugger round trips.
3. **TCG actually executes from the RX alias.** If the page preparation silently
   fails, QEMU will crash the first time it jumps into generated code.
4. **jetsam tolerates the footprint.** 1 GiB guest + 256 MiB JIT + QEMU itself.
5. **Frames reach Metal, and touches reach the guest.**

## Running it

```bash
./scripts/fetch_sources.sh
./scripts/build_ios.sh
./scripts/fetch_phase0_guest.sh
cd src/app && xcodegen generate
```

Then open `src/app/Husk.xcodeproj`, set your signing team on the Husk target, and
build to the device. Sideload the IPA the same way AetherPS4 ships if you prefer.

On the phone: install StikDebug, launch Husk, tap **Enable JIT with StikDebug**.
Husk hands the JIT script over inline, so nothing needs configuring inside
StikDebug. It attaches, prepares the region, and relaunches Husk; the second
launch should find JIT live and boot the guest.

## Reading what happens

Everything lands in **one chronological file**: `Documents/husk.log`. Before
anything else runs, `HuskLog.start()` redirects stdout and stderr into a pipe and
drains it on its own thread, so QEMU's diagnostics, Husk's C-side logging and the
Swift layer all interleave in the order they actually happened — which is the
ordering that matters when the question is "did the JIT region get set up before
TCG tried to use it".

Three ways to read it, in order of convenience:

1. **In-app.** Tap *View logs* on the setup screen, or the magnifier button in
   the top-right once the guest is running. It tails live, colour-coded by source
   (green = JIT, cyan = display, yellow = guest kernel, red = failures), and the
   share button exports both `husk.log` and `guest-serial.log`.
2. **Console.app** with the device attached, filtered to `subsystem:com.husk.app`.
3. **Files app** — `UIFileSharingEnabled` is set, so both logs are visible under
   Husk's Documents.

### The guest's own kernel console

`guest-serial.log` is the single most informative signal available: if the guest
is executing translated code at all, its kernel says so. QEMU writes it through a
file chardev (not stdio, which would have QEMU reach for a stdin that does not
exist on iOS and fail during init), and a tailer thread folds it back into
`husk.log` as it appears, so it is both a standalone file and part of the unified
timeline.

### The sequence to look for

```
[boot ] device      : iPhoneNN,N
[boot ] TXM expected: YES
[husk-jit] trap guard installed (SIGTRAP, SIGBUS)
[husk-jit] #1: StikDebug attach probe OK (0xe0000069)
[husk-jit] #1: requesting execute-capable region, size=268435456 (256.0 MiB), attempt 1/3
[husk-jit] #1: BreakGetJITMapping returned 0x...
[husk-jit] #1: dual mapping established: rw=0x... rx=0x... size=268435456 diff=...
[husk-jit] footprint[after-jit-alloc]: phys=... MiB
[husk-jit] selftest: RX alias reflects RW writes (0x52800540 0xd65f03c0) -- aliasing OK
[husk-jit] selftest: CALLING generated code at 0x... 
[husk-jit] selftest: PASS -- executed generated code from the RX alias, got 42. JIT is genuinely live.
[husk-dpy] init: console[0]=0x... graphic=1
[husk-dpy] gfx_switch gen=1 surface=0x... 1280x800 stride=5120 bpp=32
[guest ] Booting Linux on physical CPU 0x0000000000
```

### The self-test is the important line

`husk_jit_selftest()` writes `movz w0, #42 ; ret` through the RW alias, flushes
the icache on the RX alias, reads it back to prove the two views share pages, and
then **calls it**. Getting 42 back means JIT genuinely works — not "StikDebug
returned an address", which is a much weaker claim.

This matters because the failure it catches is otherwise invisible. If `_M,rx`
allocates memory but `prepare_memory_region` never walks the pages, the allocation
looks completely successful and nothing goes wrong until TCG branches into
generated code, at which point the app dies deep inside the emulator with no
context. The self-test moves that failure to a labelled line at a known moment,
and refuses to hand the region to TCG rather than letting QEMU crash on it.

## Failure modes, and what each one means

**Probe returns `0x0` instead of `0xe0000069`.** StikDebug never attached, or it
attached and detached before we asked. Not a Husk bug — check StikDebug actually
ran the script.

**Probe is fine, `BreakGetJITMapping` returns 0 three times.** StikDebug attached
but is not servicing `brk #0xf00d` with `x16=1`. Most likely the script it ran was
not the universal one; check the script text that arrived in its log.

**`selftest: FAIL -- RX alias does not reflect RW writes`.** `vm_remap` succeeded
but the two mappings are not backed by the same pages. The dual-mapping step is
wrong, not StikDebug.

**Allocation and readback fine, then the app dies on `selftest: CALLING`.** The
pages were never actually prepared — `_M,rx` gave us memory but the per-page
`M<addr>,1:69` writes did not happen. Confirm against StikDebug's own log.

**Attach takes a very long time.** Expected, and it scales linearly with
`tb-size`. Drop `tb-size=256` to `128` in `QemuRunner.phase0Arguments()` and
compare; this is the first number to tune on real hardware.

**App is killed with no crash log.** jetsam. The `footprint[...]` lines bracket
every phase — compare the last one against the device's limit, and reduce `-m
1024` first, since Alpine needs nothing like that much.

**Black screen, no crash.** Check `[husk-dpy] init:` — if it says
`qemu_console_lookup_by_index(0) returned NULL`, there is no graphics console to
attach to and nothing downstream can work. If it registered fine but no
`gfx_switch` ever appears, the guest is not driving the GPU. If `gfx_switch`
appears and the screen is still black, the problem is on the Metal side.

**Guest dies early with nothing useful.** Switch verbosity to **Firehose** on the
setup screen (`-d ...,int,exec,in_asm`) and reproduce. It logs every translated
instruction — gigabytes per minute — so only use it to catch a crash in the first
seconds, but for "it dies three instructions into the first block" nothing else
will do.

## Deliberately deferred

- `tb-size` is a guess until measured. It is the single most important number in
  Phase 0, because it cannot be renegotiated after StikDebug detaches.
- `-cpu cortex-a72` is an ARMv8.0 baseline. Android may want ARMv8.2; that is a
  Phase 1 question.
- Nothing calls `husk_ios_jit_detach()` yet. Holding the debugger open costs
  nothing in Phase 0 and keeps the option of a second allocation while the sizing
  is still being learned.
