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
    /// Set once a save has been requested, so it is only ever done once.
    nonisolated(unsafe) static var snapshotRequested = false
    nonisolated(unsafe) static var pendingSnapshotMiB = 0
    /// Guest size actually handed to QEMU, recorded alongside any snapshot.
    nonisolated(unsafe) var lastGuestMiB = 0

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

    nonisolated var hasSnapshot: Bool {
        FileManager.default.fileExists(atPath: snapshotSizePath)
    }

    private func guestMemoryMiB() -> Int {
        // A snapshot pins the size. Restoring into a differently sized machine
        // does not work, and the adaptive budget below is derived from
        // os_proc_available_memory(), which moves with whatever else the phone
        // is doing.
        if let recorded = try? String(contentsOfFile: snapshotSizePath, encoding: .utf8),
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

        let target = max(1024, min(6144,
            availableMiB - safetyMarginMiB - jitMiB - qemuOverheadMiB))

        QemuRunner.shared.lastGuestMiB = target
        HuskLog.log("qemu", "memory budget: \(physMiB) MiB physical but "
                          + "\(availableMiB) MiB before jetsam -- that is the real "
                          + "ceiling; reserving \(jitMiB) MiB JIT + \(qemuOverheadMiB) "
                          + "MiB overhead + \(safetyMarginMiB) MiB margin; "
                          + "guest gets \(target) MiB")
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
            "-M", "virt,highmem=on",

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
            "-cpu", "max,pauth-impdef=on",
            "-smp", "4",
            "-m", "\(memMiB)",
            "-accel", "tcg,tb-size=256,thread=multi,split-wx=on",

            // UEFI: our bundled code volume, and the variable store that shipped
            // with the image.
            "-drive", "if=pflash,unit=0,format=raw,readonly=on,file=\(guest.firmwarePath)",
            "-drive", "if=pflash,unit=1,format=qcow2,file=\(guest.varsPath)",

            // vda is the system disk, vdb is userdata. bootindex matters: the
            // firmware must try the system disk first.
            "-device", "virtio-blk-pci,drive=vda,bootindex=0",
            "-device", "virtio-blk-pci,drive=vdb,bootindex=1",
            "-drive", "file=\(guest.diskPath),if=none,id=vda,format=qcow2,discard=unmap,detect-zeroes=unmap",
            "-drive", "file=\(guest.userdataPath),if=none,id=vdb,format=qcow2,discard=unmap,detect-zeroes=unmap",

            // ADB is the control plane now: pm install and am start replace the
            // 9p share and the guest agent. The forward is bound to loopback --
            // only this app should be able to reach the guest's adbd.
            "-device", "virtio-net-pci,netdev=net0",
            "-netdev", "user,id=net0,hostfwd=tcp:127.0.0.1:5555-:5555",
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
            // virtio-gpu-GL: the guest's GL commands go to virglrenderer and are
            // executed on the phone's GPU, instead of Android rasterising every
            // pixel in software on a CPU that is itself emulated.
            "-device", "virtio-gpu-gl-pci,xres=360,yres=640",

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
        ]
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

    private func run() {
        let args = (profile == .phase0Alpine) ? phase0Arguments() : phase1Arguments()
        HuskLog.log("qemu", "profile: \(profile.rawValue)")

        HuskLog.log("qemu", "---- QEMU command line (\(args.count) args) ----")
        for (i, a) in args.enumerated() {
            HuskLog.log("qemu", String(format: "  argv[%2d] = %@", i, a))
        }
        HuskLog.log("qemu", "---- verbosity: \(verbosity) (-d \(verbosity.rawValue)) ----")

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

        // Before qemu_init(), not after: virtio-gpu-gl is realized inside it and
        // refuses unless display_opengl is already set. Only EGL and the flag
        // are needed this early; the surface comes later, once UIKit has a
        // layer to give us.
        let glEarly = husk_display_gl_early()
        HuskLog.log("qemu", glEarly
            ? "EGL up before device creation; virtio-gpu-gl can realize"
            : "EGL early init FAILED -- virtio-gpu-gl will refuse to start")

        HuskLog.logFootprint("before-qemu-init")

        // qemu_init takes ownership of argv for the life of the process, so these
        // strdup'd copies are intentionally never freed.
        var argv: [UnsafeMutablePointer<CChar>?] = args.map { strdup($0) }
        argv.append(nil)

        HuskLog.log("qemu", "calling qemu_init() -- JIT allocation happens inside this")
        argv.withUnsafeMutableBufferPointer { buf in
            qemu_init(Int32(args.count), buf.baseAddress)
        }
        HuskLog.log("qemu", "qemu_init() returned")

        // Before the main loop: restore the machine if we saved one. This is
        // the difference between ten minutes of Android booting and a few
        // seconds of reading RAM back from disk.
        let restored = husk_snapshot_load_at_startup()
        HuskLog.log("qemu", restored
            ? "restored a saved machine -- Android is already booted"
            : "no saved machine; booting Android from cold")
        DispatchQueue.main.async { QemuRunner.shared.restoredFromSnapshot = restored }
        HuskLog.logFootprint("after-qemu-init")

        // Must happen after qemu_init (the console does not exist before it) and
        // before qemu_main_loop (which does not return).
        // The GL path needs a CAMetalLayer, which only exists once SwiftUI has
        // laid the view out -- and this is the QEMU thread, which cannot reach
        // UIKit. Wait briefly for the view to publish it, then fall back to the
        // software path rather than showing nothing at all.
        HuskGLView.surfaceReady.lock()
        var deadline = Date().addingTimeInterval(10)
        while HuskGLView.layerForGL == nil, Date() < deadline {
            HuskGLView.surfaceReady.wait(until: deadline)
        }
        let layer = HuskGLView.layerForGL
        let size = HuskGLView.pixelSize
        HuskGLView.surfaceReady.unlock()
        _ = deadline

        var glUp = false
        if let layer {
            HuskLog.log("qemu", "setting up GL on a "
                              + "\(Int(size.width))x\(Int(size.height)) layer")
            // Created on the main thread: ANGLE is building a surface against a
            // CAMetalLayer, and CALayer is not thread-safe. Bound here, because
            // a context belongs to whichever thread made it current and the
            // guest renders from this one.
            // Logged through HuskLog at each stage, not left to the C side's
            // fprintf. Raw stderr reaches the log through a pipe and arrives
            // out of order against the timestamped lines, which made it
            // impossible to tell which half had failed.
            var created = false
            DispatchQueue.main.sync {
                created = husk_display_gl_create(Unmanaged.passUnretained(layer).toOpaque(),
                                                 Int32(size.width), Int32(size.height))
            }
            HuskLog.log("qemu", "husk_display_gl_create (main thread) -> \(created)")

            var bound = false
            if created {
                bound = husk_display_gl_bind()
                HuskLog.log("qemu", "husk_display_gl_bind (qemu thread) -> \(bound)")
            }
            glUp = created && bound
            HuskLog.log("qemu", glUp ? "GL display is up -- the GPU is drawing now"
                                     : "GL display unavailable; using the software display")
        } else {
            HuskLog.log("qemu", "no CAMetalLayer after 10s; falling back to the software display")
        }

        if !glUp {
            HuskLog.log("qemu", "calling husk_display_init() (software path)")
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
                // "Up" is inferred from the display rather than asked of the
                // guest, because asking needs ADB and this does not: six
                // consecutive five-second windows with frames in them means the
                // UI is running and settled, not that the boot animation
                // flickered. Snapshotting mid-boot would save a machine that
                // still has all its work ahead of it, which is worse than
                // useless -- every later launch would resume into it.
                if delta > 0 { steadyFrames += 1 } else { steadyFrames = 0 }
                if steadyFrames >= 6,
                   !QemuRunner.snapshotRequested,
                   !QemuRunner.shared.hasSnapshot {
                    QemuRunner.snapshotRequested = true
                    // The size goes in a static rather than being captured: the
                    // callback crosses into C, and a C function pointer cannot
                    // carry context.
                    QemuRunner.pendingSnapshotMiB = QemuRunner.shared.lastGuestMiB
                    HuskLog.log("snap", "Android has been drawing for 30s; saving the machine "
                                      + "(guest \(QemuRunner.pendingSnapshotMiB) MiB). The picture "
                                      + "will freeze while RAM is written to disk.")
                    husk_snapshot_save { ok, what in
                        let label = what.map { String(cString: $0) } ?? "save"
                        HuskLog.log("snap", "\(label) \(ok ? "succeeded" : "FAILED")")
                        if ok {
                            try? String(QemuRunner.pendingSnapshotMiB)
                                .write(toFile: QemuRunner.shared.snapshotSizePath,
                                       atomically: true, encoding: .utf8)
                            HuskLog.log("snap", "next launch will restore instead of booting")
                        }
                    }
                }

                // No [guest] lines appeared at all in one session, which is either
                // the guest writing nothing or the tailer failing to read it. The
                // file's size separates the two, and guessing wrong would send the
                // next fix in entirely the wrong direction.
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
