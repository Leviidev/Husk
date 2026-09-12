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
        case phase1Waydroid = "Android (Waydroid)"
    }

    var profile: Profile = .phase1Waydroid

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

    /// Phase 1 guest: Debian arm64 running Waydroid, booted from the downloaded
    /// disk image via UEFI.
    private func phase1Arguments() -> [String] {
        let guest = GuestImage.shared

        return [
            "qemu-system-aarch64",
            // highmem=on gives the guest a 64-bit PCI window, which it needs once
            // there is real RAM behind it.
            "-M", "virt,highmem=on",
            "-cpu", "cortex-a72",
            "-smp", "4",

            // Android wants real memory. The device has 11.7 GB and Husk's own
            // footprint before the guest starts measured 13.9 MiB, so 4 GiB here
            // is comfortable rather than brave.
            "-m", "4096",

            // 256 MiB took 749 ms to prepare on device (~46 us per 16 KiB page),
            // so doubling it costs about a second and a half of one-time setup.
            // Android translates far more code than Alpine ever will, and the
            // region cannot be grown later -- StikDebug is gone by then.
            "-accel", "tcg,tb-size=512,thread=multi,split-wx=on",

            // Debian's cloud image boots through GRUB under UEFI, so the firmware
            // pair is required: read-only code volume plus a writable variable
            // store.
            "-drive", "if=pflash,format=raw,readonly=on,file=\(guest.firmwarePath)",
            "-drive", "if=pflash,format=raw,file=\(guest.varsPath)",
            "-drive", "if=virtio,format=qcow2,file=\(guest.diskPath)",

            // Unlike Phase 0, the network is not optional: `waydroid init` fetches
            // LineageOS over it during first-run setup.
            "-nic", "user,model=virtio-net-pci",
            "-L", "\(Bundle.main.bundlePath)/pc-bios",

            "-device", "virtio-gpu-pci",
            "-device", "virtio-tablet-pci",
            "-device", "virtio-keyboard-pci",

            "-chardev", "file,id=ser0,path=\(guestSerialLogPath)",
            "-serial", "chardev:ser0",

            "-display", "none",
            "-monitor", "none",
            "-no-reboot",
            "-d", verbosity.rawValue,
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
        // QEMU's main loop is not shy with stack.
        t.stackSize = 16 * 1024 * 1024
        thread = t

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
               GuestImage.shared.firmwarePath,
               GuestImage.shared.varsPath]
        for path in required {
            let attrs = try? FileManager.default.attributesOfItem(atPath: path)
            let size = (attrs?[.size] as? NSNumber)?.intValue ?? -1
            HuskLog.log("qemu", "needs \((path as NSString).lastPathComponent): "
                              + (size >= 0 ? "\(size) bytes" : "MISSING"))
        }

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
        HuskLog.logFootprint("after-qemu-init")

        // Must happen after qemu_init (the console does not exist before it) and
        // before qemu_main_loop (which does not return).
        HuskLog.log("qemu", "calling husk_display_init()")
        husk_display_init()

        HuskLog.log("qemu", "entering qemu_main_loop() -- this does not return until shutdown")
        qemu_main_loop()

        HuskLog.log("qemu", "qemu_main_loop() RETURNED -- guest stopped")
        HuskLog.logFootprint("after-main-loop")
        qemu_cleanup()
        HuskLog.log("qemu", "qemu_cleanup() done")
        isRunning = false
    }

    /// Fold the guest's kernel console into the unified log as it appears.
    private func startSerialTailer() {
        let path = guestSerialLogPath
        try? FileManager.default.removeItem(atPath: path)

        let t = Thread {
            var offset: UInt64 = 0
            var handle: FileHandle?
            var pending = ""

            while true {
                if handle == nil {
                    handle = FileHandle(forReadingAtPath: path)
                    if handle != nil { HuskLog.log("guest", "serial log opened: \(path)") }
                }
                if let h = handle {
                    try? h.seek(toOffset: offset)
                    if let data = try? h.readToEnd(), !data.isEmpty {
                        offset += UInt64(data.count)
                        pending += String(decoding: data, as: UTF8.self)
                        while let nl = pending.firstIndex(of: "\n") {
                            let line = String(pending[pending.startIndex..<nl])
                            pending = String(pending[pending.index(after: nl)...])
                            if line.isEmpty { continue }
                            HuskLog.log("guest", line)
                            if let r = line.range(of: "HUSK-SETUP: ") {
                                let msg = String(line[r.upperBound...])
                                Task { @MainActor in
                                    QemuRunner.shared.setupMessage = msg
                                }
                            }
                        }
                    }
                }
                Thread.sleep(forTimeInterval: 0.25)
            }
        }
        t.name = "husk.serial-tail"
        t.qualityOfService = .utility
        t.start()
    }
}
