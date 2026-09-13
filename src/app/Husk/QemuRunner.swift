// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import os

/// Runs QEMU inside this process on a dedicated thread.
///
/// iOS does not let an app spawn processes, so QEMU cannot be a child the way it
/// is on macOS. Instead `libqemu-aarch64-softmmu.dylib` is built with
/// `-Dshared_lib=true`, exposing `qemu_init` / `qemu_main_loop` / `qemu_cleanup`,
/// and we drive them from a pthread. QEMU never returns from `qemu_main_loop`
/// until the guest shuts down, so this thread belongs to QEMU for the session.
final class QemuRunner: ObservableObject {
    static let shared = QemuRunner()
    private var thread: Thread?
    private(set) var isRunning = false

    /// Last line the guest printed about its own setup. The first-boot service in
    /// the guest writes HUSK-SETUP markers to the serial console, which is already
    /// being tailed -- so Android's one-time download reports progress to the UI
    /// without needing any channel of its own.
    @Published var setupMessage: String?

    /// How much QEMU tells us. `in_asm`/`exec` produce gigabytes in seconds, so
    /// they are never on by default — but they are here because when a guest dies
    /// three instructions into its first translated block, nothing else will do.
    enum Verbosity: String, CaseIterable {
        case normal    = "guest_errors,unimp"
        case detailed  = "guest_errors,unimp,cpu_reset,page,mmu"
        case firehose  = "guest_errors,unimp,cpu_reset,page,mmu,int,exec,in_asm"
    }

    var verbosity: Verbosity = .detailed

    /// Which guest to boot.
    ///
    /// Phase 0's Alpine guest is kept because it is the fastest way to tell a
    /// broken substrate from a broken Android setup: it boots in seconds from the
    /// app bundle with no download, no UEFI and no disk, so if it draws and Phase 1
    /// does not, the problem is above the JIT/display layer.
    enum Profile: String, CaseIterable {
        case phase0Alpine   = "Alpine (substrate check)"
        case phase1Android = "Android (LineageOS guest)"
    }

    var profile: Profile = .phase1Android

    /// True once the GL display is live. The UI needs this: HuskGLView must
    /// exist before QEMU starts, because it is what publishes the layer, but if
    /// GL then fails to initialise nothing ever draws into that layer. Without
    /// this flag the fallback to the software display is invisible -- the log
    /// says it fell back and the screen stays black.
    @Published var glDisplayActive = false

    /// True when this session started from a saved machine rather than booting.
    @Published var restoredFromSnapshot = false

    /// True while the machine is being written to disk.
    @Published var isSavingState = false
    /// Completion for an explicit save. A static because the C callback is a
    /// bare function pointer and cannot carry context.
    nonisolated(unsafe) static var saveCompletion: ((Bool) -> Void)?
    /// Set once a save has been requested, so it is only ever done once.
    nonisolated(unsafe) static var snapshotRequested = false
    /// When the guest was started, for the progress readout.
    nonisolated(unsafe) static var bootStarted = Date()

    /// Recognisable points in an Android boot, in the order they occur.
    ///
    /// Matched against serial console lines. These are milestones a person can
    /// read, not every service -- the point is to show movement, not detail.
    nonisolated(unsafe) static let bootMilestones: [(String, String)] = [
        ("Linux version",                 "Starting the Linux kernel"),
        ("init: init first stage started","Android init, first stage"),
        ("init: init second stage started","Android init, second stage"),
        ("SELinux: policy loaded",        "Loading the security policy"),
        ("apexd: activating",             "Activating system packages"),
        ("servicemanager: Waiting",       "Starting system services"),
        ("starting service 'vold'",       "Preparing storage"),
        ("starting service 'surfaceflinger'", "Starting the display server"),
        ("starting service 'zygote'",     "Starting the Android runtime"),
        ("starting service 'bootanim'",   "Boot animation running"),
        ("Service 'bootanim' (pid",       "Compiling apps (this is the slow part)"),
        ("sys.boot_completed=1",          "Android is up"),
    ]

    /// When Android announced `sys.boot_completed=1` on the serial console.
    ///
    /// Android says this itself, on a line init prints; it does not have to be
    /// inferred, and inferring it was the bug. Nil until the guest says so.
    nonisolated(unsafe) static var bootCompletedAt: Date?
    nonisolated(unsafe) static var pendingSnapshotMiB = 0
    /// Current balloon target, once the guest has been shrunk. Nil while it
    /// still has everything it was given.
    nonisolated(unsafe) static var balloonTargetMiB: Int?
    /// Guest size actually handed to QEMU, recorded alongside any snapshot.
    nonisolated(unsafe) var lastGuestMiB = 0
    /// Resolution actually handed to virtio-gpu.
    ///
    /// The touch mapping needs this and had been carrying its own copy, which
    /// said 360x640 while QEMU was being told 360x800. Every touch was then
    /// mapped into a box a fifth too short: squashed vertically, with the
    /// bottom of the guest screen unreachable. One number, in one place, taken
    /// from what was actually asked for.
    nonisolated(unsafe) static var lastGuestRes = (w: 360, h: 800)

    private var documentsDir: String {
        NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true)[0]
    }
    var guestSerialLogPath: String { "\(documentsDir)/guest-serial.log" }

    /// Phase 0 guest: Alpine 3.24.1 aarch64, booted directly from kernel+initramfs.
    ///
    /// This exact command line was validated on the host under TCG before ever
    /// being run on device: the kernel reaches userspace, virtio-gpu registers
    /// fb0, and the virtio tablet and keyboard both enumerate.
    private func phase0Arguments() -> [String] {
        let bundle = Bundle.main.bundlePath

        return [
            "qemu-system-aarch64",
            "-M", "virt",
            "-cpu", "cortex-a72",
            "-smp", "4",
            "-m", "1024",

            // tb-size is the whole JIT budget for the session. StikDebug detaches
            // after startup and a second allocation is impossible, so this has to
            // be right up front. It also costs attach time: StikDebug touches every
            // 16 KiB page through the debugger, so 256 MiB is 16384 round trips.
            // split-wx=on is NOT optional here. It defaults to 0 (see
            // tcg_accel_instance_init), and with it off TCG never calls the
            // splitwx allocator at all -- it goes straight to an RWX mmap, which
            // iOS refuses with EPERM and which no amount of JIT setup can fix.
            // Forcing it on (rather than "auto") also disables the RWX fallback,
            // so a genuine failure surfaces as itself instead of as a confusing
            // "Operation not permitted".
            "-accel", "tcg,tb-size=256,thread=multi,split-wx=on",

            "-kernel", "\(bundle)/vmlinuz-virt",
            "-initrd", "\(bundle)/initramfs-virt",
            "-append", "console=tty0 console=ttyAMA0 loglevel=8",

            // -M virt adds a default virtio-net-pci unless told otherwise, and that
            // device wants its PXE option ROM at startup -- which is why the first
            // device run died on 'failed to find romfile "efi-virtio.rom"'. Phase 0
            // needs no network at all, so the device simply should not exist. The
            // ROMs are bundled anyway, because Android will need networking and the
            // next occurrence of this would be just as opaque.
            "-nic", "none",
            "-L", "\(bundle)/pc-bios",

            "-device", "virtio-gpu-pci",
            "-device", "virtio-tablet-pci",
            "-device", "virtio-keyboard-pci",

            // The guest's own kernel console. By far the most informative single
            // signal available: if the guest is executing translated code at all,
            // it says so here. A file chardev rather than stdio, because stdio
            // would have QEMU reach for a stdin that does not exist on iOS and
            // fail during init -- a bad way to lose a first device run. A tailer
            // folds this back into the unified log a moment later.
            "-chardev", "file,id=ser0,path=\(guestSerialLogPath)",
            "-serial", "chardev:ser0",

            // Our own DisplayChangeListener is the display; QEMU needs no backend.
            "-display", "none",
            "-monitor", "none",
            "-no-reboot",

            // No -D: let QEMU's own diagnostics go to stderr, which HuskLog has
            // already redirected. One file, one chronology, so "JIT region
            // allocated" and "first translated block" appear in the order they
            // actually happened.
            "-d", verbosity.rawValue,
        ]
    }

    /// How much RAM to give the guest, in MiB.
    ///
    /// Sized from what iOS says this process may still allocate rather than from a
    /// fixed number. A jetsam kill is a SIGKILL: no signal handler runs, the log
    /// just stops mid-line, and it looks exactly like a crash. Guessing 4096 here
    /// is what produced one.
    ///
    /// The budget has to cover the JIT region and QEMU's own allocations as well as
    /// guest RAM, and leave genuine headroom -- Android dirties most of what it is
    /// given, and dirty pages count against the footprint.
    /// Where the guest RAM size used when the snapshot was taken is recorded.
    /// A restored machine must be given exactly the memory it was saved with,
    /// and the budget is computed from a figure that can drift between runs.
    nonisolated var snapshotSizePath: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk-snapshot.mib").path
    }

    /// Whether to give the guest a real GPU.
    ///
    /// Opt-in, because turning it on invalidates any snapshot saved without one
    /// and therefore costs a single cold boot before it pays for itself.
    nonisolated static var gpuModeEnabled: Bool {
        UserDefaults.standard.bool(forKey: "husk.gpuMode")
    }

    /// Which display device the saved machine was built around.
    ///
    /// A snapshot is only valid against the device it was taken with, exactly
    /// as it is only valid against its RAM size, so this is recorded next to it
    /// and checked before any restore is attempted.
    nonisolated var snapshotDisplayPath: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk-snapshot.display").path
    }
    nonisolated var snapshotDisplay: String? {
        (try? String(contentsOfFile: snapshotDisplayPath, encoding: .utf8))?
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }

    nonisolated var hasSnapshot: Bool {
        FileManager.default.fileExists(atPath: snapshotSizePath)
    }

    /// Whether a saved machine exists that this session could actually restore.
    ///
    /// Existence alone is the wrong question, and asking it is why turning GPU
    /// mode on never produced a GPU snapshot however many times it cold-booted:
    /// a software snapshot from an earlier run left its stamp on disk, the
    /// auto-save saw "a snapshot exists" and skipped, and the one machine worth
    /// saving -- the one running on the GPU -- was never written. A snapshot
    /// taken in the other display mode is not a snapshot this session can use.
    nonisolated var hasUsableSnapshot: Bool {
        guard hasSnapshot else { return false }
        return (snapshotDisplay ?? "sw") == (QemuRunner.glProven ? "gl" : "sw")
    }

    /// Write the running machine to disk so the next launch restores *this*.
    ///
    /// Not an optimisation -- it is what makes anything persist at all.
    /// Restoring rewinds the userdata disk to the moment the snapshot was taken,
    /// and it has to: the restored RAM describes that exact disk, down to the
    /// page cache and the ext4 journal, and resuming a kernel against a
    /// filesystem that moved on underneath it corrupts both. So everything done
    /// since the snapshot -- an app installed, an account signed into, a setting
    /// changed -- is discarded on the next launch unless the machine is saved
    /// over the top.
    ///
    /// Saving writes the whole of guest RAM, so the picture freezes for as long
    /// as that takes. Callers are expected to say so.
    func saveState(reason: String, completion: ((Bool) -> Void)? = nil) {
        guard isRunning else { completion?(false); return }
        guard !isSavingState else {
            HuskLog.log("snap", "save already in progress; ignoring \(reason)")
            completion?(false); return
        }
        isSavingState = true
        QemuRunner.pendingSnapshotMiB = lastGuestMiB
        QemuRunner.saveCompletion = completion
        HuskLog.log("snap", "saving the machine (\(reason), guest "
                          + "\(QemuRunner.pendingSnapshotMiB) MiB); the picture freezes "
                          + "while RAM is written to disk")

        // Off the main thread from here.
        //
        // Quiescing is three round trips to the guest plus five seconds of
        // waiting for processes to actually die, and callers reach this from
        // the UI. Doing that inline would freeze the interface for fifteen
        // seconds and invite the watchdog to kill the app -- which would look
        // exactly like the crash this whole path exists to avoid.
        DispatchQueue.global(qos: .userInitiated).async {
            self.performSave()
        }
    }

    private func performSave() {
        let completion = QemuRunner.saveCompletion

        // With a GPU, saving is only safe once the guest owns no 3D resources.
        //
        // virglrenderer holds those on the host and none of them are written to
        // the snapshot, so a machine saved while SurfaceFlinger is running comes
        // back believing in textures and contexts that no longer exist. Stopping
        // the framework closes every DRM file and frees them with it, leaving
        // only the kernel's 2D framebuffer -- which virtio_gpu_save does write.
        //
        // This is the promise the patched migration blocker is trusting, so it
        // happens here, next to the save, rather than anywhere a caller could
        // forget it. The framework is started again on the way out and, more
        // importantly, by whoever restores this snapshot.
        if QemuRunner.glProven {
            HuskLog.log("snap", "stopping the compositor and app processes so no 3D "
                              + "resources are live while the machine is written")
            // Named services, never a blanket `stop`.
            //
            // `stop` acts on whole init classes, and Husk's own command bridge
            // is an init service too -- stopping it would take away the only
            // channel able to start anything again, leaving a black screen and
            // no way back. ctl.stop names one service and can only ever affect
            // that service, so the bridge is safe by construction.
            //
            // Zygote first: it owns the app processes, and each of those holds
            // its own EGL contexts. SurfaceFlinger last, because it is what the
            // apps are talking to.
            // Every stop must actually succeed. Asking whether the bridge is
            // still alive proves nothing: SELinux refuses ctl.stop from
            // u:r:shell:s0 --
            //
            //   avc: denied { set } for property=ctl.stop$zygote
            //        scontext=u:r:shell:s0 tcontext=u:object_r:ctl_stop_prop:s0
            //
            // -- so all three calls can fail while the shell keeps answering
            // happily. That is how a save once went ahead with SurfaceFlinger
            // still holding 3D resources, which is precisely the case the
            // migration blocker exists to prevent.
            var quiesced = true
            for svc in ["zygote_secondary", "zygote", "surfaceflinger"] {
                let r = try? GuestBridge.shared.run("setprop ctl.stop \(svc)", timeout: 60)
                // zygote_secondary does not exist on a 64-bit-only guest, so
                // its absence is not a failure; a permission refusal is.
                if svc != "zygote_secondary", (r?.status ?? -1) != 0 {
                    HuskLog.log("snap", "could not stop \(svc): "
                              + (r?.out.trimmingCharacters(in: .whitespacesAndNewlines)
                                 ?? "no answer"))
                    quiesced = false
                }
            }

            if !quiesced {
                HuskLog.log("snap", "REFUSING to save: the GPU could not be quiesced, and "
                                  + "saving with live 3D resources crashes the process "
                                  + "outright. The guest is untouched.")
                QemuRunner.saveCompletion = nil
                DispatchQueue.main.async {
                    QemuRunner.shared.isSavingState = false
                    completion?(false)
                }
                return
            }

            // ctl.stop returns as soon as init has been told, not once the
            // process has died and its buffers have been released.
            Thread.sleep(forTimeInterval: 5)
            HuskLog.log("snap", "compositor and app processes stopped")
        }

        husk_snapshot_save { ok, what in
            let label = what.map { String(cString: $0) } ?? "save"
            HuskLog.log("snap", "\(label) \(ok ? "succeeded" : "FAILED")")
            if ok {
                try? String(QemuRunner.pendingSnapshotMiB)
                    .write(toFile: QemuRunner.shared.snapshotSizePath,
                           atomically: true, encoding: .utf8)
                try? QemuRunner.memoryStrategy
                    .write(toFile: QemuRunner.shared.memoryStrategyPath,
                           atomically: true, encoding: .utf8)
                // Stamp the display this machine belongs to, so a later launch
                // in the other mode cold-boots instead of restoring into a
                // device the guest does not expect.
                try? (QemuRunner.glProven ? "gl" : "sw")
                    .write(toFile: QemuRunner.shared.snapshotDisplayPath,
                           atomically: true, encoding: .utf8)
            }
            if QemuRunner.glProven {
                // Whether or not the save worked. A guest left with its
                // compositor stopped is a black screen, and a failed save is
                // not a reason to hand someone one of those.
                for svc in ["surfaceflinger", "zygote"] {
                    _ = try? GuestBridge.shared.shell("setprop ctl.start \(svc)", timeout: 60)
                }
                HuskLog.log("snap", "compositor and app processes restarted")
            }
            let done = QemuRunner.saveCompletion
            QemuRunner.saveCompletion = nil
            Task { @MainActor in
                QemuRunner.shared.isSavingState = false
                done?(ok)
            }
        }
    }

    /// Backing file for guest RAM.
    ///
    /// Guest RAM allocated the ordinary way is dirty anonymous memory, which is
    /// precisely what jetsam counts and what caps the guest at roughly 1.9 GiB
    /// on an 11.7 GiB phone. Backing it with a MAP_SHARED file instead makes
    /// those pages file-backed: the kernel can write them back and evict them,
    /// and clean external pages are not charged to phys_footprint the way
    /// anonymous ones are. That is the whole point -- it turns guest RAM into
    /// something the OS can page rather than something that must all stay
    /// resident.
    nonisolated var guestRamPath: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("guest-ram.bin").path
    }

    /// How RAM was provided on the run that took the snapshot.
    ///
    /// A snapshot records a machine shape, and changing the memory backend
    /// changes that shape. Restoring a file-backed machine into an anonymous
    /// one (or the reverse) is not something to find out about at load time.
    nonisolated var memoryStrategyPath: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk-memory-strategy").path
    }

    /// Bumped whenever the memory layout changes, to retire old snapshots.
    static var memoryStrategy: String {
        // The GPU device is part of the machine's shape, so a snapshot taken
        // with virtio-gpu-pci cannot be restored into a virtio-gpu-gl-pci
        // machine. Folding the display choice into the strategy means the first
        // launch after GL starts working boots cold once, then re-snapshots,
        // rather than failing to restore.
        GuestImage.shared.hasShippedSnapshot
            ? "shipped-snapshot-v10"
            : "file-backed-lineage-v2-" + (QemuRunner.glProven ? "gl" : "sw")
    }

    /// Whether guest RAM can be backed by a file on this device, this run.
    ///
    /// Checked rather than assumed: QEMU creates and ftruncates the backing file
    /// itself, and if that fails the machine does not start at all. Falling back
    /// to anonymous RAM costs the guest some size; failing to start costs the
    /// user the app. Evaluated once and cached, because the answer is used both
    /// to size the guest and to build the command line, and they must agree.
    private lazy var ramFileReady: Bool = {
        let needBytes = Int64(6144 + 768) * 1024 * 1024
        let dir = (guestRamPath as NSString).deletingLastPathComponent

        if let attrs = try? FileManager.default.attributesOfFileSystem(forPath: dir),
           let free = (attrs[.systemFreeSize] as? NSNumber)?.int64Value {
            let freeMiB = free / (1024 * 1024)
            guard free > needBytes else {
                HuskLog.log("qemu", "only \(freeMiB) MiB free; not backing guest RAM "
                                  + "with a file, falling back to anonymous memory")
                return false
            }
            HuskLog.log("qemu", "\(freeMiB) MiB free for the guest RAM file")
        }

        // Create it now so the no-backup flag can be set. iOS evicts nothing it
        // has been told to back up, and a multi-gigabyte scratch file has no
        // business in iCloud.
        if !FileManager.default.fileExists(atPath: guestRamPath) {
            guard FileManager.default.createFile(atPath: guestRamPath, contents: nil) else {
                HuskLog.log("qemu", "could not create \(guestRamPath); "
                                  + "falling back to anonymous memory")
                return false
            }
        }
        var url = URL(fileURLWithPath: guestRamPath)
        var values = URLResourceValues()
        values.isExcludedFromBackup = true
        try? url.setResourceValues(values)
        return true
    }()

    /// Size we are about to attempt, and the largest size known to have survived.
    ///
    /// A PROT_NONE reservation is not proof: 6144 MiB reserved cleanly and QEMU
    /// still died initialising it. So the size is written down before the
    /// attempt and only confirmed once qemu_init() returns. If a launch finds an
    /// attempt that was never confirmed, that size killed us last time and this
    /// run picks a smaller one -- which means a bad size costs one launch rather
    /// than every launch.
    nonisolated var ramAttemptPath: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk-ram-attempt").path
    }
    nonisolated var ramProvenPath: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk-ram-proven").path
    }
    /// Smallest size ever seen to fail, remembered permanently.
    ///
    /// Comparing the last attempt against the last success was not enough. One
    /// failed launch at 6144 MiB followed by a good launch at 5120 left
    /// attempt == proven, the guard disengaged, and the next launch tried 6144
    /// again -- which failed again, in exactly the same place. A size that has
    /// once killed qemu_init has to stay excluded, not be forgotten as soon as
    /// something smaller works.
    nonisolated var ramFailedPath: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk-ram-failed").path
    }

    private func readInt(_ path: String) -> Int? {
        guard let t = try? String(contentsOfFile: path, encoding: .utf8) else { return nil }
        return Int(t.trimmingCharacters(in: .whitespacesAndNewlines))
    }

    /// Largest of `candidates` that can actually be mapped the way QEMU maps it.
    ///
    /// Not a PROT_NONE reservation. QEMU creates the file, extends it to the
    /// full size, maps it MAP_SHARED read/write and then writes to it, so this
    /// does exactly that and touches both the first and last page. A reservation
    /// that succeeds where the real mapping fails is worse than no check at all,
    /// because it produces confidence and then a crash.
    private func largestMappableMiB(_ candidates: [Int]) -> Int? {
        for mib in candidates {
            let bytes = off_t(mib) * 1024 * 1024
            let fd = open(guestRamPath, O_RDWR | O_CREAT, 0o600)
            if fd < 0 {
                HuskLog.log("qemu", "RAM file check: cannot open (errno \(errno))")
                return nil
            }
            defer { close(fd) }

            guard ftruncate(fd, bytes) == 0 else {
                HuskLog.log("qemu", "RAM file check: \(mib) MiB cannot be allocated "
                                  + "on disk (errno \(errno)); trying smaller")
                continue
            }
            guard let p = mmap(nil, size_t(bytes), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0),
                  p != MAP_FAILED else {
                HuskLog.log("qemu", "RAM file check: \(mib) MiB maps no better than "
                                  + "QEMU would (errno \(errno)); trying smaller")
                continue
            }

            // Touch both ends: a mapping can be handed back and still fail on
            // first write if the backing store cannot honour it.
            let bytesPtr = p.assumingMemoryBound(to: UInt8.self)
            bytesPtr[0] = 0
            bytesPtr[Int(bytes) - 1] = 0
            munmap(p, size_t(bytes))
            HuskLog.log("qemu", "RAM file check: \(mib) MiB maps and writes cleanly")
            return mib
        }
        return nil
    }

    /// Guest resolution, shaped like the screen it will be stretched onto.
    ///
    /// The blit scales the guest texture across the whole window with no regard
    /// for aspect ratio, so a 360x640 guest (0.5625) drawn onto a 1206x2622
    /// phone (0.460) comes out visibly squashed. Keeping the width at 360 --
    /// the emulated cost is per pixel, and this is already the cheapest useful
    /// size -- and deriving the height from the real screen makes the two
    /// shapes agree, so nothing has to be letterboxed or distorted.
    ///
    /// Rounded to a multiple of 8: graphics stacks are happier with aligned
    /// strides, and the error is under half a percent of the height.
    private var guestResolution: (w: Int, h: Int) {
        let r = computedGuestResolution
        QemuRunner.lastGuestRes = r
        return r
    }

    private var computedGuestResolution: (w: Int, h: Int) {
        // A shipped snapshot fixes the resolution: it was saved against one,
        // and a machine whose display differs is a different machine.
        if GuestImage.shared.hasShippedSnapshot {
            // From the manifest the snapshot was published with, not a constant
            // compiled into this build -- an app older than a snapshot cannot
            // know the machine it was saved on.
            let pins = GuestImage.shared.snapshotPins
            return (pins.xres, pins.yres)
        }
        let size = HuskGLView.pixelSize
        guard size.width > 0, size.height > 0 else { return (360, 640) }
        let w = 360
        let exact = Double(w) * Double(size.height) / Double(size.width)
        let h = max(480, Int((exact / 8).rounded()) * 8)
        return (w, h)
    }

    private var machineCpu: String {
        GuestImage.shared.hasShippedSnapshot ? GuestImage.shared.snapshotPins.cpu
                                            : GuestImage.defaultCpu
    }
    private var machineSmp: Int {
        GuestImage.shared.hasShippedSnapshot ? GuestImage.shared.snapshotPins.smp
                                            : GuestImage.defaultSmp
    }

    private func guestMemoryMiB() -> Int {
        // Same reasoning as the resolution: the shipped snapshot was saved with
        // exactly this much RAM, and QEMU rejects a restore that differs by a
        // byte. No probing, no stepping down -- this number or nothing.
        if GuestImage.shared.hasShippedSnapshot {
            let mib = GuestImage.shared.snapshotPins.mib
            QemuRunner.shared.lastGuestMiB = mib
            HuskLog.log("qemu", "using the shipped snapshot: guest pinned to \(mib) MiB")
            return mib
        }

        // A snapshot pins the size. Restoring into a differently sized machine
        // does not work, and the adaptive budget below is derived from
        // os_proc_available_memory(), which moves with whatever else the phone
        // is doing.
        // A snapshot from a differently shaped machine is not restorable, and
        // the memory backend is part of that shape. Retire it rather than
        // discover the mismatch inside load_snapshot.
        let priorStrategy = try? String(contentsOfFile: memoryStrategyPath, encoding: .utf8)
        let strategyChanged = priorStrategy?.trimmingCharacters(in: .whitespacesAndNewlines)
            != QemuRunner.memoryStrategy
        if strategyChanged, FileManager.default.fileExists(atPath: snapshotSizePath) {
            HuskLog.log("qemu", "memory layout changed to \(QemuRunner.memoryStrategy); "
                              + "retiring the old snapshot, so this boot is a cold one")
            try? FileManager.default.removeItem(atPath: snapshotSizePath)
        }

        if !strategyChanged,
           let recorded = try? String(contentsOfFile: snapshotSizePath, encoding: .utf8),
           let mib = Int(recorded.trimmingCharacters(in: .whitespacesAndNewlines)), mib > 0 {
            HuskLog.log("qemu", "snapshot exists; using its guest size of \(mib) MiB")
            return mib
        }

        let physMiB = Int(ProcessInfo.processInfo.physicalMemory / (1024 * 1024))
        let availableMiB = Int(husk_ios_available_memory() / (1024 * 1024))

        // os_proc_available_memory() is the real ceiling, not a floor. Budgeting
        // 40% of physical RAM instead -- a 3426 MiB guest -- got the app
        // jetsammed 85 seconds in, the footprint falling cleanly from 2082 MiB
        // of headroom to 178 MiB before the log simply stopped. Death came just
        // past 3400 MiB, against the 3376 MiB this call reported at launch.
        //
        // It is accurate because a guest's RAM is dirty anonymous memory, which
        // is exactly what it measures. QEMU's resident size is a high-water mark
        // of guest page touches: once the guest dirties a page it stays resident
        // even after the guest frees it, so the peak is what kills us and it
        // cannot be walked back. Another app reaching a larger number does not
        // transfer -- clean file-backed pages are evictable and charged
        // differently.
        let jitMiB = 256          // tb-size
        let qemuOverheadMiB = 750 // measured, not guessed
        // Real margin, in megabytes rather than a fraction. A fraction of what
        // was left quietly cost ~375 MiB the guest could have had; the run that
        // booted Android peaked with ~500 MiB spare, so this is the same shape
        // of safety without the waste.
        let safetyMarginMiB = 450

        guard availableMiB > 0 else {
            HuskLog.log("qemu", "available memory unknown; falling back to 1536 MiB guest")
            return 1536
        }

        // The JIT is only subtracted if it has not been taken yet. Prewarming
        // claims it before this runs, so os_proc_available_memory() has already
        // fallen by that much -- subtracting again charged for it twice and cut
        // the guest from 1906 MiB to 1650.
        let jitStillToCome = JITBootstrap.prewarmed ? 0 : jitMiB
        let anonymousTarget = max(1024, min(6144,
            availableMiB - safetyMarginMiB - jitStillToCome - qemuOverheadMiB))

        // With a file-backed RAM block the arithmetic above is the wrong shape:
        // it measures room for pages that must stay resident, and these do not
        // have to. Ask for more than that ceiling allows, because the excess
        // lives in the page cache rather than in our footprint.
        //
        // Deliberately not the largest number that fits. Whether iOS really
        // keeps these pages out of phys_footprint is the thing this build is
        // measuring, and the footprint line logged every five seconds is the
        // evidence: if resident stays flat while the guest grows, the next
        // build can raise this. Jumping straight to 4096 would instead risk
        // regressing a configuration that currently boots.
        // Ask for as much as this process can actually map, largest first.
        //
        // 6144 MiB was not merely optimistic, it was fatal: QEMU died inside
        // qemu_init() before printing anything, because the mapping could not
        // be made. 2560 MiB had been fine. The JIT already holds about a
        // gigabyte of address space -- 512 MiB mapped twice, RW and RX -- so a
        // 6 GiB RAM block puts the total past what the process is allowed.
        //
        // Rather than hard-code whatever happens to work on one phone, find out.
        // A PROT_NONE anonymous reservation costs no physical memory and fails
        // exactly when the real mapping would, so stepping down through these
        // sizes and keeping the first that succeeds turns a crash into a
        // slightly smaller guest.
        // 6144 is deliberately absent. It passed every check that could be
        // made for it -- the address space is there, the file maps and writes
        // -- and then killed qemu_init three times regardless. Whatever objects
        // to it is not something this code can see, so it is simply not offered.
        var candidates = [5120, 4096, 3584, 3072, 2560, 2048]

        // Fold a failed attempt into the permanent floor before using it.
        if let attempted = readInt(ramAttemptPath), readInt(ramProvenPath) != attempted {
            let worst = min(attempted, readInt(ramFailedPath) ?? Int.max)
            try? String(worst).write(toFile: ramFailedPath, atomically: true, encoding: .utf8)
            HuskLog.log("qemu", "last launch attempted \(attempted) MiB and never got "
                              + "past qemu_init; remembering that permanently")
        }
        if let failed = readInt(ramFailedPath) {
            candidates = candidates.filter { $0 < failed }
            HuskLog.log("qemu", "\(failed) MiB is known to fail on this device; "
                              + "staying below it")
        }
        let fileBackedTarget = ramFileReady ? largestMappableMiB(candidates) : nil
        let target = fileBackedTarget.map { max(anonymousTarget, $0) } ?? anonymousTarget

        QemuRunner.shared.lastGuestMiB = target
        try? String(target).write(toFile: ramAttemptPath, atomically: true, encoding: .utf8)
        HuskLog.log("qemu", "memory budget: \(physMiB) MiB physical but "
                          + "\(availableMiB) MiB before jetsam -- that is the real "
                          + "ceiling; reserving \(jitMiB) MiB JIT + \(qemuOverheadMiB) "
                          + "MiB overhead + \(safetyMarginMiB) MiB margin; "
                          + "GUEST GETS \(target) MiB "
                          + (fileBackedTarget != nil
                             ? "from a file-backed block (anonymous memory could only "
                             + "have given it \(anonymousTarget) MiB)"
                             : "as anonymous memory"))
        return target
    }

    /// Phase 1 guest: LineageOS booting directly under QEMU via UEFI.
    private func phase1Arguments() -> [String] {
        let guest = GuestImage.shared
        let memMiB = guestMemoryMiB()

        // Modelled on android-lineage-qemu's own documented qemu-system line for
        // the arm64only build, because that is the configuration the image is
        // actually tested in -- including on iOS under UTM, which like us has no
        // hypervisor and runs TCG. Deviating from it is how the last three days
        // went, so the deviations here are only the ones Husk structurally needs:
        // our own display bridge, our own serial capture, and a memory budget
        // that fits iOS's jetsam ceiling rather than their flat 2048.
        return [
            "qemu-system-aarch64",
            "-M", ramFileReady ? "virt,highmem=on,memory-backend=huskram"
                               : "virt,highmem=on",

            // Their emulation line, not cortex-a72. "max" is what the image is
            // tested against and avoids guessing which ARMv8 extensions this
            // Android build assumes; pauth-impdef picks a cheap implementation
            // -defined pointer-auth algorithm instead of QARMA, which TCG
            // emulates at ruinous cost.
            // pauth-impdef=on, and do NOT turn pauth off.
            //
            // Turning it off panicked the kernel at 9.3s inside
            // __pi_scs_handle_fde_frame <- module_finalize <- load_module, with
            // 0xd503233f and 0xd50323bf in the registers -- the encodings of
            // PACIASP and AUTIASP. That is the dynamic shadow call stack
            // patcher, which rewrites pointer-auth instructions into
            // shadow-call-stack ones and which the kernel runs ONLY when the CPU
            // lacks PAC. So "pauth degrades to no-ops" holds for userspace and
            // not for this kernel: removing the feature switches a whole code
            // path ON rather than switching one off. impdef keeps PAC present
            // while picking a cheap algorithm instead of QARMA.
            // sve=off and sme=off are performance changes, not correctness ones.
            //
            // "max" advertises FEAT_SVE (with PMULL128, BitPerm, SHA3, SM4) and
            // FEAT_SME. TCG has no host SVE to map those onto, so every SVE
            // instruction becomes a helper call -- while bionic selects its
            // SVE memcpy/memset/strlen/strcmp through ifunc the moment HWCAP_SVE
            // is set. The result is that every string and memory operation in
            // the whole of Android takes the slow path. With SVE absent bionic
            // falls back to its ASIMD routines, which TCG translates onto the
            // host's own NEON.
            //
            // MTE needs no such treatment: the virt machine provides no tag
            // memory, so QEMU already downgrades ID_AA64PFR1.MTE to 1 and no
            // tag checking happens.
            // From the snapshot when there is one, because the CPU model and
            // vCPU count are as much a part of the machine as its RAM size and
            // QEMU refuses a restore into a different shape.
            "-cpu", machineCpu,
            "-smp", "\(machineSmp)",
            "-m", "\(memMiB)",
            "-accel", "tcg,tb-size=256,thread=multi,split-wx=on",

            // The balloon was written earlier and never put on the machine, so
            // nothing could ever reclaim guest memory. With it present the guest
            // can be told to hand pages back when the host gets tight.
            "-device", "virtio-balloon-pci,id=huskballoon",

            // UEFI: our bundled code volume, and the variable store that shipped
            // with the image.
            "-drive", "if=pflash,unit=0,format=raw,readonly=on,file=\(guest.firmwarePath)",
            "-drive", "if=pflash,unit=1,format=qcow2,file=\(guest.varsPath)",

            // vda is the system disk, vdb is userdata. bootindex matters: the
            // firmware must try the system disk first.
            "-device", "virtio-blk-pci,drive=vda,bootindex=0",
            "-device", "virtio-blk-pci,drive=vdb,bootindex=1",
            // detect-zeroes made QEMU scan the contents of every guest write
            // looking for runs of zeroes to punch out. That is host CPU spent
            // to save disk space on a device with 53 GB free, and host CPU is
            // the one resource this whole system is short of.
            "-drive", "file=\(guest.diskPath),if=none,id=vda,format=qcow2,discard=unmap",
            // node-name matters: save_snapshot picks where to put the VM state
            // by node name, and with nothing named it takes the FIRST
            // snapshot-capable drive in graph order -- which is the pflash
            // variable store. That is how a 64 MiB UEFI vars image grew to
            // 2.6 GB of guest RAM blobs.
            "-drive", "file=\(guest.userdataPath),if=none,id=vdb,node-name=huskvmstate,"
                    + "format=qcow2,discard=unmap",

            // Two forwards, both on loopback so nothing outside this app can
            // reach the guest.
            //
            //   5555  adbd, when it is willing to talk. It usually is not: an
            //         unprovisioned LineageOS runs adbd in trade-in mode, where
            //         every shell is refused, and provisioning it from outside
            //         is the problem this bridge exists to solve.
            //   5599  Husk's own bridge -- a plain nc listener started by init
            //         as u:r:shell:s0, which hands whatever is written to it to
            //         /system/bin/sh. That is the same authority adb shell has,
            //         obtained without adbd's cooperation.
            "-device", "virtio-net-pci,netdev=net0",
            "-netdev", "user,id=net0,hostfwd=tcp:127.0.0.1:5555-:5555,"
                     + "hostfwd=tcp:127.0.0.1:5599-:5599",
            "-L", "\(Bundle.main.bundlePath)/pc-bios",

            // 360x640 rather than 1280x800: a quarter of the pixels.
            //
            // This is the dominant term. There is no GPU, so every pixel is
            // rasterised in software by a CPU that is itself emulated, and the
            // cost is paid twice -- ~1M pixels per frame at a ~10x emulation
            // penalty lands almost exactly on the 2 FPS observed. Cutting pixel
            // count 4.4x attacks the multiplication rather than one of its
            // factors. It is also a measurement: if the frame rate does not move
            // roughly in proportion then fill is not the bottleneck and this
            // model is wrong, which is worth knowing before tuning anything else.
            // GL only if it has been proven to work on this device, because the
            // choice is not reversible: a console created for virtio-gpu-gl
            // demands a GL listener, so when GL then fails to initialise the
            // software display cannot register and QEMU aborts with "The
            // console requires a GL context". Falling back has to mean not
            // asking for the GL device in the first place.
            "-device", QemuRunner.glProven
                ? "virtio-gpu-gl-pci,xres=\(guestResolution.w),yres=\(guestResolution.h)"
                : "virtio-gpu-pci,xres=\(guestResolution.w),yres=\(guestResolution.h)",

            // USB HID rather than virtio-input, which is what their config uses.
            // Every Android kernel has usbhid; virtio-input is not guaranteed,
            // and losing input would look exactly like a hung guest.
            "-device", "qemu-xhci,id=usb-bus",
            "-device", "usb-tablet,bus=usb-bus.0",
            "-device", "usb-kbd,bus=usb-bus.0",

            // Entropy. Without it the guest stalls waiting for crng init, which
            // on a previous guest cost several seconds of boot.
            "-device", "virtio-rng-pci",

            "-chardev", "file,id=ser0,path=\(guestSerialLogPath)",
            "-serial", "chardev:ser0",
            "-display", "none",
            "-monitor", "none",
            // NOT -no-reboot. Android reboots itself on purpose, and the most
            // important case is repair: when /data is inconsistent it reboots
            // into recovery, fixes it, and reboots again. With -no-reboot that
            // self-repair became a dead VM -- init announced
            // "Rebooting into recovery, reason: init_user0_failed" and QEMU
            // simply stopped. A guest that loops will show up in the log; a
            // guest that cannot reboot cannot recover.
            "-d", "guest_errors,unimp",
        ] + (ramFileReady ? [
            // Guest RAM as a MAP_SHARED file rather than anonymous memory, so
            // the kernel can write it back and evict it instead of counting all
            // of it against our jetsam footprint. share=on is what makes the
            // mapping shared and therefore external; without it the file maps
            // private and every dirtied page becomes anonymous again, which is
            // the thing being avoided.
            //
            // prealloc is deliberately off. Touching the whole block up front
            // would make every page resident immediately and hand back exactly
            // the problem this is here to solve.
            "-object", "memory-backend-file,id=huskram,size=\(memMiB)M,"
                     + "mem-path=\(guestRamPath),share=on,prealloc=off",
        ] : [])
    }

    func start() {
        guard !isRunning else {
            HuskLog.log("qemu", "start() ignored -- already running")
            return
        }
        isRunning = true

        let t = Thread { [weak self] in self?.run() }
        t.name = "husk.qemu"
        // Darwin propagates QoS to threads a thread creates, and every vCPU is
        // created from this one. Left at the default they read as background
        // work -- CPU-saturated for the guest's whole life -- and get parked on
        // efficiency cores, which on a phone are several times slower than the
        // performance cores. The guest's speed is the app's speed, so this is
        // user-interactive by definition.
        t.qualityOfService = .userInteractive
        // QEMU's main loop is not shy with stack.
        t.stackSize = 16 * 1024 * 1024
        thread = t

        // 9p exports a directory that must already exist when QEMU starts.
        HuskBridgeFS.shared.prepare()
        HuskLog.log("qemu", "spawning QEMU thread (stack 16 MiB)")
        t.start()
        startSerialTailer()
    }

    /// Set once the GL stack has been shown to work on this device. Persisted,
    /// because the device choice happens before anything can be tested and a
    /// wrong guess costs the whole session.
    /// Software display unless the user asks otherwise.
    ///
    /// Not a retreat from the GL work -- that stack is finished and correct --
    /// but a reading of the evidence. Runs on the software display reached
    /// sys.boot_completed twice, at 281s and 321s. No run on the GPU has
    /// reached it at all, and the run that got closest spent longer after the
    /// boot animation than the software runs took in total. virtio-gpu-gl makes
    /// the guest drive Mesa's virgl driver and SurfaceFlinger's full render
    /// engine, and all of that is emulated code; what it buys back is
    /// rasterisation of a 360x640 screen, which was never the expensive part.
    /// Get a booted machine snapshotted first, then re-open the question.
    nonisolated(unsafe) static var glProven: Bool {
        // The display device is part of the machine's shape, exactly like its
        // RAM size -- and the shipped snapshot was saved against virtio-gpu-pci.
        //
        // Restoring it into virtio-gpu-gl-pci produces a machine whose guest
        // holds virgl resource IDs that the freshly created virgl context has
        // never heard of, and every scanout then fails:
        //
        //   virgl_cmd_set_scanout: illegal resource specified 57
        //   virtio_gpu_virgl_process_cmd: ctrl 0x103, error 0x1203
        //
        // Android is alive behind that -- services start, adbd runs -- but
        // nothing can ever be drawn. RAM and resolution were pinned for this
        // reason; the display was missed.
        // GPU mode replaces the blanket refusal that used to sit here.
        //
        // The refusal was right at the time: the shipped snapshot was saved
        // against virtio-gpu-pci, and restoring it into virtio-gpu-gl-pci hands
        // the guest virgl resource ids a fresh renderer has never created. What
        // was missing was not a reason to allow it but a way to tell the two
        // kinds of snapshot apart, which snapshotDisplay now does -- a machine
        // saved in the wrong display mode is cold-booted instead of restored.
        guard QemuRunner.gpuModeEnabled else { return false }
        return UserDefaults.standard.bool(forKey: "husk.glProven")
    }

    /// Try the GL stack without committing to it. Runs before the QEMU command
    /// line is built, so its answer can pick the virtio-gpu device.
    /// Create the GL context and surface, and on the first run decide whether
    /// GL is usable at all.
    ///
    /// The early-return that used to sit here -- `guard !glProven` -- meant the
    /// context and surface were only ever built on the run that proved GL
    /// worked. Every launch after that skipped straight past creation, and
    /// husk_display_gl_bind() was then asked to make current a context that had
    /// never been made, failed, and fell back to the software display. So GL
    /// worked exactly once, on the run that discovered it, and never again.
    ///
    /// Creation is unconditional now. Only the *probe* is skipped once the
    /// answer is known, because that is the part that is merely a question.
    private func probeGL() {
        HuskGLView.surfaceReady.lock()
        let deadline = Date().addingTimeInterval(5)
        while HuskGLView.layerForGL == nil, Date() < deadline {
            HuskGLView.surfaceReady.wait(until: deadline)
        }
        let layer = HuskGLView.layerForGL
        let size = HuskGLView.pixelSize
        HuskGLView.surfaceReady.unlock()

        guard let layer, size.width > 0 else {
            HuskLog.log("gl", "no layer to probe with; staying on the software display")
            return
        }

        // The surface is built once and never rebuilt, so this size is the one
        // the GPU draws at for the rest of the process. If it disagrees with
        // what is on screen later, that is the bug -- not the renderer.
        HuskLog.log("gl", "binding the EGL surface to the layer published at "
                        + "\(Int(size.width))x\(Int(size.height)) px")
        var created = false
        DispatchQueue.main.sync {
            created = husk_display_gl_create(Unmanaged.passUnretained(layer).toOpaque(),
                                             Int32(size.width), Int32(size.height))
        }
        if QemuRunner.glProven {
            HuskLog.log("gl", "GL already proven on this device; context created "
                            + "(create=\(created)), skipping the probe")
            return
        }

        let works = created && husk_display_gl_probe()
        HuskLog.log("gl", "probe: create=\(created) usable=\(works)")
        if works {
            // glProven is computed now, so only the stored fact is written here.
            UserDefaults.standard.set(true, forKey: "husk.glProven")
            HuskLog.log("gl", "GL works; this and future runs use the GPU")
        } else {
            HuskLog.log("gl", "GL not usable on this device; software display it is")
        }
    }

    private func run() {
        probeGL()
        let args = (profile == .phase0Alpine) ? phase0Arguments() : phase1Arguments()
        HuskLog.log("qemu", "profile: \(profile.rawValue)")
        HuskLog.log("qemu", "DISPLAY MODE: "
                          + (QemuRunner.glProven ? "GPU (virtio-gpu-gl)"
                                                 : "software (CPU framebuffer)"))
        if QemuRunner.glProven {
            // This used to say the GPU and fast relaunch were mutually
            // exclusive, because QEMU refuses to snapshot a virgl machine:
            // "virgl is not yet migratable". Husk now lifts that blocker and
            // saves with the Android framework stopped, so the only host GPU
            // state left is state nothing will miss. See the virtio-gpu patches
            // in scripts/integrate_husk.sh for why that is sound.
            HuskLog.log("qemu", "GPU display with snapshots: saving stops the "
                              + "compositor first, and restoring starts it again")
        }

        HuskLog.log("qemu", "---- QEMU command line (\(args.count) args) ----")
        for (i, a) in args.enumerated() {
            HuskLog.log("qemu", String(format: "  argv[%2d] = %@", i, a))
        }
        // Only phase 0 takes its -d from this setting; phase 1 hardcodes a quiet
        // one. Printing the setting regardless made it look as though Android
        // was running with page and mmu logging on, which would be ruinous.
        HuskLog.log("qemu", profile == .phase0Alpine
            ? "---- verbosity: \(verbosity) (-d \(verbosity.rawValue)) ----"
            : "---- verbosity: fixed for Android (-d guest_errors,unimp) ----")

        // Confirm the guest images are actually in the bundle before QEMU tries to
        // open them; "could not load kernel" is a far less obvious error message.
        let required: [String] = (profile == .phase0Alpine)
            ? ["\(Bundle.main.bundlePath)/vmlinuz-virt",
               "\(Bundle.main.bundlePath)/initramfs-virt"]
            : [GuestImage.shared.diskPath,
               GuestImage.shared.userdataPath,
               GuestImage.shared.firmwarePath,
               GuestImage.shared.varsPath]
        for path in required {
            let attrs = try? FileManager.default.attributesOfItem(atPath: path)
            let size = (attrs?[.size] as? NSNumber)?.intValue ?? -1
            HuskLog.log("qemu", "needs \((path as NSString).lastPathComponent): "
                              + (size >= 0 ? "\(size) bytes" : "MISSING"))
        }

        if QemuRunner.glProven {
            // Only claimed once GL has actually been shown to work, since the
            // flag is what lets virtio-gpu-gl realize.
            _ = husk_display_gl_early()
            HuskLog.log("qemu", "GL proven previously; asking for virtio-gpu-gl")
        }

        HuskLog.logFootprint("before-qemu-init")

        // qemu_init takes ownership of argv for the life of the process, so these
        // strdup'd copies are intentionally never freed.
        var argv: [UnsafeMutablePointer<CChar>?] = args.map { strdup($0) }
        argv.append(nil)

        // Waive virgl's migration blocker. Husk's patched virtio-gpu-base.c
        // keeps the blocker for everyone who does not set this, because it is
        // only safe alongside the stop-the-framework-before-saving discipline
        // in saveState().
        if QemuRunner.glProven { setenv("HUSK_VIRGL_SNAPSHOT", "1", 1) }

        HuskLog.log("qemu", "calling qemu_init() -- JIT allocation happens inside this")
        argv.withUnsafeMutableBufferPointer { buf in
            qemu_init(Int32(args.count), buf.baseAddress)
        }
        HuskLog.log("qemu", "qemu_init() returned")
        // Survived initialisation, so this size is safe to try again next launch.
        try? String(QemuRunner.shared.lastGuestMiB)
            .write(toFile: ramProvenPath, atomically: true, encoding: .utf8)

        // Before the main loop: restore the machine if we saved one. This is
        // the difference between ten minutes of Android booting and a few
        // seconds of reading RAM back from disk.
        // Two kinds of snapshot can be present, and they are recorded
        // differently: the shipped one stamps the generation it belongs to,
        // while a locally saved one records the guest size it was taken at.
        let snapshotFits: Bool
        if GuestImage.shared.hasShippedSnapshot {
            snapshotFits = (QemuRunner.shared.lastGuestMiB == GuestImage.shared.snapshotPins.mib)
        } else {
            snapshotFits = (try? String(contentsOfFile: snapshotSizePath, encoding: .utf8))
                .flatMap { Int($0.trimmingCharacters(in: .whitespacesAndNewlines)) }
                .map { $0 == QemuRunner.shared.lastGuestMiB } ?? false
        }
        if !snapshotFits {
            HuskLog.log("qemu", "no snapshot matching a \(QemuRunner.shared.lastGuestMiB) MiB "
                              + "guest; booting cold rather than failing a restore")
        }

        // The display device is part of the machine too.
        //
        // A shipped snapshot carries no display stamp and was always saved on
        // the software framebuffer, so treat a missing stamp as "sw". Restoring
        // across a mismatch is the failure that put GL behind a flag in the
        // first place: Android comes back alive and holding resource ids that
        // the new renderer never created, and nothing is ever drawn again.
        let wantDisplay = QemuRunner.glProven ? "gl" : "sw"
        let haveDisplay = QemuRunner.shared.snapshotDisplay ?? "sw"
        let displayFits = (wantDisplay == haveDisplay)
        if !displayFits {
            HuskLog.log("qemu", "saved machine is a \(haveDisplay) machine and this one is "
                              + "\(wantDisplay); booting cold and saving a new one")
        }
        let restored = snapshotFits && displayFits && husk_snapshot_load_at_startup()
        HuskLog.log("qemu", restored
            ? "restored a saved machine -- Android is already booted"
            : "no saved machine; booting Android from cold")
        DispatchQueue.main.async { QemuRunner.shared.restoredFromSnapshot = restored }
        if restored && QemuRunner.glProven {
            // A GL snapshot was necessarily taken with the compositor stopped,
            // so it restores into a machine with nothing drawing. Starting it
            // again is what turns the restore back into a running phone, and it
            // is also the moment SurfaceFlinger builds its 3D context against
            // the renderer this process just created.
            Thread.detachNewThread {
                HuskLog.log("qemu", "restored a GL machine; starting the compositor")
                for svc in ["surfaceflinger", "zygote"] {
                    _ = try? GuestBridge.shared.shell("setprop ctl.start \(svc)", timeout: 180)
                }
            }
        }
        HuskLog.logFootprint("after-qemu-init")

        // Must happen after qemu_init (the console does not exist before it) and
        // before qemu_main_loop (which does not return).
        // The surface was already created and proven during probeGL(); all that
        // is left is to register the listener, which needs console 0 and so has
        // to wait until qemu_init() has run.
        var glUp = false
        if QemuRunner.glProven {
            glUp = husk_display_gl_bind()
            HuskLog.log("qemu", glUp ? "GL display is up -- the GPU is drawing now"
                                     : "GL bind failed after a successful probe")
        }
        if !glUp {
            HuskLog.log("qemu", "using the software display")
            husk_display_init()
        }
        DispatchQueue.main.async { QemuRunner.shared.glDisplayActive = glUp }

        startMemoryWatch()

        HuskLog.log("qemu", "entering qemu_main_loop() -- this does not return until shutdown")
        qemu_main_loop()

        HuskLog.log("qemu", "qemu_main_loop() RETURNED -- guest stopped")
        HuskLog.logFootprint("after-main-loop")
        qemu_cleanup()
        HuskLog.log("qemu", "qemu_cleanup() done")
        isRunning = false
    }

    /// Sample memory every few seconds for the whole session.
    ///
    /// Without this a jetsam kill leaves no evidence at all: the log's last entry
    /// is whatever happened to be printed, and the cause is indistinguishable from
    /// a crash. With it, available-before-jetsam visibly falls towards zero.
    private func startMemoryWatch() {
        let t = Thread {
            var tick = 0
            var lastFrames: UInt64 = 0
            var steadyFrames = 0
            while true {
                Thread.sleep(forTimeInterval: 5)
                tick += 1
                HuskLog.logFootprint("t+\(tick * 5)s")

                // Frame rate, as a number rather than an impression. "Slow" is
                // not something two people can compare across builds; frames per
                // second is. The sequence counter increments once per surface
                // update the guest pushed, so this is what the guest actually
                // produced, not what we managed to draw.
                let glFrames = husk_display_gl_frames()
                let frames = glFrames > 0 ? glFrames : husk_display_sequence()
                let delta = frames >= lastFrames ? frames - lastFrames : 0
                lastFrames = frames
                HuskLog.log("perf", "guest produced \(delta) frames in 5s "
                                  + "(\(String(format: "%.1f", Double(delta) / 5.0)) fps)")

                // Save the machine once Android is genuinely up, and only once.
                //
                // The previous rule here -- six consecutive five-second windows
                // with frames in them -- fired at 31 seconds, while the boot
                // animation was still playing and the system had another five
                // minutes of work in front of it. The comment even said that
                // snapshotting mid-boot would be worse than useless, which was
                // right; the heuristic just did not implement it. Wait for the
                // guest's own boot_completed, then let it settle: the launcher
                // still has to start and draw, and a snapshot taken during that
                // saves a machine that resumes into a half-drawn screen.
                if delta > 0 { steadyFrames += 1 } else { steadyFrames = 0 }
                let settled = QemuRunner.bootCompletedAt.map {
                    Date().timeIntervalSince($0) >= 30
                } ?? false
                if settled, steadyFrames >= 3,
                   !QemuRunner.snapshotRequested,
                   !QemuRunner.shared.hasUsableSnapshot {
                    QemuRunner.snapshotRequested = true
                    // Through saveState, never husk_snapshot_save directly.
                    //
                    // This called the C function straight, which meant it also
                    // skipped everything saveState does around it -- above all
                    // stopping the compositor first. On the GPU that is not an
                    // omission but a fault: the save walked into live 3D
                    // resources and took the process down with SIGSEGV. The
                    // discipline has to live on the one path every save takes,
                    // not beside it.
                    DispatchQueue.main.async {
                        QemuRunner.shared.saveState(reason: "Android is booted and settled")
                    }
                }

                // No [guest] lines appeared at all in one session, which is either
                // the guest writing nothing or the tailer failing to read it. The
                // file's size separates the two, and guessing wrong would send the
                // next fix in entirely the wrong direction.
                // Give memory back before jetsam takes the whole app.
                //
                // A SIGKILL from jetsam is unsurvivable and silent; an inflated
                // balloon merely makes the guest tighter. So when headroom gets
                // genuinely low, shrink the guest by whatever is missing plus a
                // margin, and leave it shrunk -- re-growing it on a dip would
                // just oscillate against the same ceiling.
                let availMiB = Int(husk_ios_available_memory() / (1024 * 1024))
                let floorMiB = 350
                if availMiB > 0, availMiB < floorMiB,
                   QemuRunner.shared.lastGuestMiB > 0 {
                    let shortfall = floorMiB - availMiB + 128
                    let newSize = max(768, QemuRunner.balloonTargetMiB
                                         ?? QemuRunner.shared.lastGuestMiB) - shortfall
                    if newSize >= 768, newSize != QemuRunner.balloonTargetMiB {
                        QemuRunner.balloonTargetMiB = newSize
                        HuskLog.log("mem", "only \(availMiB) MiB before jetsam; "
                                         + "ballooning the guest down to \(newSize) MiB")
                        husk_balloon_set_bytes(Int64(newSize) * 1024 * 1024)
                    }
                }

                if tick % 6 == 0 {
                    let attrs = try? FileManager.default
                        .attributesOfItem(atPath: QemuRunner.shared.guestSerialLogPath)
                    let size = (attrs?[.size] as? NSNumber)?.intValue ?? -1
                    HuskLog.log("qemu", "guest serial log is \(size) bytes "
                                      + "(if this grows but no [guest] lines appear, "
                                      + "the tailer is at fault, not the guest)")
                }
            }
        }
        t.name = "husk.memwatch"
        t.qualityOfService = .utility
        t.start()
    }

    /// Fold the guest's kernel console into the unified log as it appears.
    ///
    /// Uses open(2)/read(2) rather than FileHandle. The FileHandle version opened
    /// the right file -- it logged "serial log opened" and the size probe showed
    /// that file growing from 51 KB to 63 KB -- yet never produced a single line.
    /// A plain fd advances its own offset on every read and returns 0 at EOF, so
    /// there is no seek/offset bookkeeping to get wrong, and it reports what it
    /// actually did.
    private func startSerialTailer() {
        let path = guestSerialLogPath
        try? FileManager.default.removeItem(atPath: path)

        let t = Thread {
            var fd: Int32 = -1
            var pending = Data()
            var buf = [UInt8](repeating: 0, count: 64 * 1024)
            var totalRead = 0
            var reportedOpen = false

            while true {
                if fd < 0 {
                    fd = open(path, O_RDONLY)
                    if fd >= 0, !reportedOpen {
                        reportedOpen = true
                        HuskLog.log("guest", "serial log opened (fd \(fd))")
                    }
                    if fd < 0 { Thread.sleep(forTimeInterval: 0.5); continue }
                }

                let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, 64 * 1024) }
                if n < 0 {
                    if errno == EINTR { continue }
                    HuskLog.log("guest", "serial read failed: errno \(errno); reopening")
                    close(fd); fd = -1
                    Thread.sleep(forTimeInterval: 1)
                    continue
                }
                if n == 0 {
                    // EOF for now; the fd keeps its offset, so appended bytes are
                    // picked up on the next read.
                    Thread.sleep(forTimeInterval: 0.25)
                    continue
                }

                totalRead += n
                pending.append(contentsOf: buf[0..<n])

                while let nl = pending.firstIndex(of: 0x0A) {
                    let lineData = pending.subdata(in: pending.startIndex..<nl)
                    pending.removeSubrange(pending.startIndex...nl)
                    // Serial consoles emit CRLF; strip the CR or every line ends
                    // with a stray carriage return.
                    var line = String(decoding: lineData, as: UTF8.self)
                    line = line.replacingOccurrences(of: "\r", with: "")
                    line = line.trimmingCharacters(in: .whitespaces)
                    if line.isEmpty { continue }

                    HuskLog.log("guest", line)

                    // Say what the guest is doing, so a long first boot does
                    // not look like a hang.
                    //
                    // Android's first boot here runs five to seven minutes:
                    // dex2oat compiles the system, and the framework watchdog
                    // kills and restarts system_server at least once along the
                    // way. All of that is invisible -- the screen holds the last
                    // frame the boot animation drew -- so it reads as a freeze
                    // and gets force-quit a minute short of finishing. It only
                    // has to succeed once, because the snapshot is taken after
                    // it, but it does have to succeed once.
                    for (needle, milestone) in QemuRunner.bootMilestones {
                        if line.contains(needle) {
                            let secs = Int(Date().timeIntervalSince(QemuRunner.bootStarted))
                            let mins = secs / 60, rem = secs % 60
                            let stamp = String(format: "%d:%02d", mins, rem)
                            Task { @MainActor in
                                QemuRunner.shared.setupMessage =
                                    "\(milestone)  ·  \(stamp) elapsed"
                            }
                            break
                        }
                    }

                    // Android announces the end of its own boot. init prints
                    // "processing action (sys.boot_completed=1)" when the
                    // property is set, which is the one unambiguous statement
                    // in the whole log that the system is up -- everything else
                    // (frames appearing, bootanim running) is true long before.
                    if QemuRunner.bootCompletedAt == nil,
                       line.contains("sys.boot_completed=1") {
                        QemuRunner.bootCompletedAt = Date()
                        HuskLog.log("snap", "Android reports boot_completed; "
                                          + "will save the machine once it settles")
                    }

                    if let r = line.range(of: "HUSK-SETUP: ") ?? line.range(of: "HUSK-UI: ") {
                        let msg = String(line[r.upperBound...])
                        Task { @MainActor in QemuRunner.shared.setupMessage = msg }
                    }
                }

                // A runaway buffer means no newline is ever arriving; say so rather
                // than growing without bound.
                if pending.count > 1_000_000 {
                    HuskLog.log("guest", "serial buffer exceeded 1 MB with no newline; discarding")
                    pending.removeAll(keepingCapacity: true)
                }
            }
        }
        t.name = "husk.serial-tail"
        t.qualityOfService = .utility
        t.start()
    }
}
