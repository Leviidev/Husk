// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import Compression

/// Manages the Android guest that lives in Documents.
///
/// The guest is LineageOS built to boot directly under QEMU
/// (github.com/jqssun/android-lineage-qemu), not Android inside a container
/// inside another Linux. That removes Debian, LXC, Waydroid, cage, dbus and
/// pipewire from underneath it -- along with every failure that only existed
/// because of them -- and the arm64only build is the one that project
/// recommends for iOS, where there is no hypervisor and QEMU is emulating.
///
/// Three files make up the guest. Only the system disk is downloaded; the other
/// two are a few hundred kilobytes each and ride in the IPA:
///
///   vda  system, 5 GiB virtual  -- downloaded, compressed clusters
///   vdb  userdata, 16 GiB virtual, empty -- seeded from the bundle
///   efi_vars  UEFI variable store (qcow2, 64 MiB virtual) -- seeded
@MainActor
final class GuestImage: ObservableObject {
    static let shared = GuestImage()

    enum State: Equatable {
        case missing
        case downloading(progress: Double, received: Int64, total: Int64)
        case installing
        case ready
        case failed(String)
    }

    @Published private(set) var state: State = .missing

    /// Published alongside the IPA. Kept as a constant here so a rebuilt guest can
    /// be rolled out without shipping a new app binary.
    /// A qcow2 with compressed clusters (`qemu-img convert -c`). QEMU reads those
    /// transparently, so there is no decompression step in the app at all -- which
    /// matters because iOS's Compression framework offers LZFSE/LZ4/LZMA/zlib and
    /// neither zstd nor bzip2, so any external container would have meant shipping
    /// a decompressor.
    /// Bumped whenever the guest image itself changes. Without this the app keeps
    /// whatever it downloaded first: the disk exists, so nothing re-fetches it, and
    /// a guest missing a newly-added component fails in ways that look like app
    /// bugs rather than a stale image.
    /// The single "Dependencies" release that holds every downloadable asset.
    ///
    /// Fixed, so bumping a generation does not need a new GitHub release --
    /// the version lives in the asset name instead.
    static let dependenciesTag = "lineage-v2"
    /// Asset generation. Bumping this re-downloads the guest and its snapshot.
    ///
    /// v5 adds an init script that marks the device provisioned. Without it
    /// Android shows no launcher and adbd refuses every shell, because an
    /// unprovisioned device runs ADB in trade-in mode.
    static let imageVersion = "v6"

    /// Whether to fetch the pre-booted snapshot rather than boot from cold.
    static var wantsSnapshot: Bool {
        UserDefaults.standard.object(forKey: "husk.downloadSnapshot") as? Bool ?? true
    }

    /// A machine that has already finished booting, gzipped.
    ///
    /// Booting Android here takes five to twelve minutes and has to survive
    /// Android's own watchdog killing system_server partway through. The same
    /// boot was done once on a Mac and saved; restoring it took 8.8 seconds.
    /// Two gigabytes of download buys that.
    static var snapshotURL: URL {
        URL(string: "https://github.com/Leviidev/Husk/releases/download/"
                  + "\(dependenciesTag)/vdb-snapshot-\(imageVersion).qcow2.gz")!
    }

    /// RAM and resolution the shipped snapshot was taken with.
    ///
    /// Not adjustable. QEMU refuses a restore whose RAM size differs by a byte
    /// -- "Size mismatch: huskram" -- so a device that probed its way to 5120
    /// could not load a snapshot saved at 4096. These are the numbers the
    /// snapshot on the release was built against.
    static let snapshotGuestMiB = 4096
    static let snapshotXres = 360
    static let snapshotYres = 800

    static var imageURL: URL {
        URL(string: "https://github.com/Leviidev/Husk/releases/download/"
                  + "\(dependenciesTag)/vda-\(imageVersion).qcow2")!
    }

    nonisolated private var versionStampPath: String {
        // Named for this guest, not the old one. It used to be
        // "husk-guest.version", which cleanUpPreviousGuest() deletes as Waydroid
        // leftovers -- so every launch wiped the stamp, decided the image was
        // unversioned, and downloaded 1.1 GB again.
        documents.appendingPathComponent("lineage-guest.version").path
    }

    // nonisolated: QEMU's own thread builds its command line from these, and they
    // are pure path arithmetic with no mutable state to protect.
    nonisolated private var documents: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
    }
    nonisolated var diskPath: String { documents.appendingPathComponent("lineage-vda.qcow2").path }
    /// Present when userdata came from the shipped pre-booted snapshot.
    nonisolated var snapshotStampPath: String {
        documents.appendingPathComponent("husk-shipped-snapshot").path
    }
    /// Whether this install is running on the shipped snapshot, which pins the
    /// machine's RAM size and resolution to what the snapshot was saved with.
    /// True only when the installed snapshot belongs to the CURRENT generation.
    ///
    /// Existence alone was not enough. The stamp survived an image version bump,
    /// so an install that already had the v1 snapshot skipped downloading the v5
    /// one -- while the userdata that snapshot lived in had just been re-seeded
    /// empty by the same bump. The app then pinned the machine for a restore
    /// that could not happen and cold-booted, which looks exactly like the
    /// snapshot feature silently not existing.
    nonisolated var hasShippedSnapshot: Bool {
        guard let stamped = try? String(contentsOfFile: snapshotStampPath, encoding: .utf8)
        else { return false }
        return stamped.trimmingCharacters(in: .whitespacesAndNewlines) == Self.imageVersion
    }

    nonisolated var userdataPath: String { documents.appendingPathComponent("lineage-vdb.qcow2").path }
    nonisolated var varsPath: String { documents.appendingPathComponent("lineage-efi-vars.fd").path }
    nonisolated var firmwarePath: String { documents.appendingPathComponent("edk2-aarch64-code.fd").path }

    private var task: URLSessionDownloadTask?

    /// Bump whenever the set or order of virtio devices in the Phase 1 command
    /// line changes. Any change renumbers the PCI bus and invalidates recorded
    /// UEFI boot entries.
    nonisolated static let deviceLayoutSignature = "v4-lineage-gpu-xhci-2blk-net-serial-rng-balloon-vmstatenode"

    /// qcow2 magic: "QFI\xfb". Checked because a download that "succeeded" is not
    /// the same as a download that produced an image -- a 404 body lands on disk
    /// just as happily as 755 MB of guest would.
    private static let qcow2Magic: [UInt8] = [0x51, 0x46, 0x49, 0xFB]

    /// Anything smaller than this is definitionally not a guest image. The real
    /// one is ~755 MB; the failure that prompted this check was 9 bytes of
    /// "Not Found".
    private static let minimumPlausibleSize: Int64 = 64 * 1024 * 1024

    /// True if the file at `path` looks like a qcow2 of plausible size.
    nonisolated static func validate(path: String) -> (size: Int64?, problem: String?) {
        let fm = FileManager.default
        guard let attrs = try? fm.attributesOfItem(atPath: path),
              let size = (attrs[.size] as? NSNumber)?.int64Value else {
            return (nil, "file is missing")
        }
        guard size >= minimumPlausibleSize else {
            // Show a little of it: when this fires the content is usually a short
            // error page, and quoting it names the real problem immediately.
            var hint = ""
            if size > 0, size < 512,
               let d = fm.contents(atPath: path),
               let text = String(data: d, encoding: .utf8) {
                hint = " — content was: \(text.trimmingCharacters(in: .whitespacesAndNewlines))"
            }
            return (nil, "only \(size) bytes, expected at least "
                        + "\(minimumPlausibleSize / (1024 * 1024)) MB\(hint)")
        }
        guard let fh = FileHandle(forReadingAtPath: path),
              let head = try? fh.read(upToCount: 4), head.count == 4 else {
            return (nil, "could not read the file header")
        }
        try? fh.close()
        guard Array(head) == qcow2Magic else {
            let hex = head.map { String(format: "%02x", $0) }.joined(separator: " ")
            return (nil, "not a qcow2 image (header was \(hex), expected 51 46 49 fb)")
        }
        return (size, nil)
    }

    func refresh() {
        if case .downloading = state { return }
        if case .installing = state { return }

        guard FileManager.default.fileExists(atPath: diskPath) else {
            state = .missing
            return
        }
        // Validate every time, not just after downloading: a previous run may have
        // installed a corrupt file before this check existed, and the symptom of
        // that ("Image is not in qcow2 format") points at QEMU rather than here.
        // A valid image of the wrong version is still the wrong image.
        let installed = try? String(contentsOfFile: versionStampPath, encoding: .utf8)
        if installed != Self.imageVersion {
            HuskLog.log("guest", "installed guest is \(installed ?? "unversioned"), "
                               + "need \(Self.imageVersion); re-downloading")
            try? FileManager.default.removeItem(atPath: diskPath)
            state = .missing
            return
        }

        let check = Self.validate(path: diskPath)
        if let why = check.problem {
            HuskLog.log("guest", "existing guest disk is INVALID (\(why)); removing it")
            try? FileManager.default.removeItem(atPath: diskPath)
            state = .failed("The runtime on disk is not a valid image (\(why)). Tap to retry.")
        } else {
            HuskLog.log("guest", "guest disk present and valid: \(check.size ?? 0) bytes")
            state = .ready
        }
    }

    /// Create the UEFI variable store beside the bundled firmware.
    ///
    /// The firmware itself ships raw in the app bundle. It is 64 MiB, but almost
    /// entirely zeros, so the IPA's own zip compresses it to about a megabyte --
    /// cheaper than carrying a decompressor for it.
    /// Remove what the previous guest architecture left in Documents.
    ///
    /// The Waydroid disk grew to over 4 GB once Android had been written into it
    /// and is now dead weight on the user's phone. Its diagnostics file is worse
    /// than dead: the bridge still folds it into the log every poll, replaying
    /// Debian and Waydroid text long after either existed, which reads exactly
    /// like a live guest saying the wrong thing.
    private func cleanUpPreviousGuest() {
        let fm = FileManager.default
        let stale = ["husk-guest.qcow2", "husk-guest.version",
                     "edk2-vars.fd", "edk2-vars.fd.layout",
                     "share/diagnostics.txt"]
        var freed: Int64 = 0
        for name in stale {
            let path = documents.appendingPathComponent(name).path
            guard fm.fileExists(atPath: path) else { continue }
            let size = ((try? fm.attributesOfItem(atPath: path))?[.size] as? NSNumber)?.int64Value ?? 0
            try? fm.removeItem(atPath: path)
            freed += size
        }
        if freed > 0 {
            HuskLog.log("guest", "removed \(freed / (1024 * 1024)) MiB left by the previous "
                               + "guest (Debian/Waydroid)")
        }
    }

    func prepareFirmware() throws {
        cleanUpPreviousGuest()
        let fm = FileManager.default
        if !fm.fileExists(atPath: firmwarePath) {
            guard let src = Bundle.main.path(forResource: "edk2-aarch64-code", ofType: "fd") else {
                throw NSError(domain: "husk", code: 1, userInfo: [
                    NSLocalizedDescriptionKey: "edk2-aarch64-code.fd missing from the app bundle"])
            }
            // Copied rather than used in place: pflash wants a plain file and the
            // bundle is read-only, and keeping both volumes together in Documents
            // makes the pair easy to reason about.
            try fm.copyItem(atPath: src, toPath: firmwarePath)
            HuskLog.log("guest", "firmware staged to Documents")
        }
        // The UEFI variable store records boot entries as PCI device paths. Adding
        // or removing a virtio device renumbers the bus, so entries written under a
        // previous device layout stop resolving -- and the firmware then falls
        // through to PXE and drops to the EFI Shell instead of booting the disk.
        // The symptom looks nothing like its cause, so the store is stamped with a
        // signature of the layout and rebuilt whenever that changes.
        let stamp = URL(fileURLWithPath: varsPath + ".layout")
        let current = Self.deviceLayoutSignature
        let previous = try? String(contentsOf: stamp, encoding: .utf8)

        if !fm.fileExists(atPath: varsPath) || previous != current {
            if previous != nil, previous != current {
                HuskLog.log("guest", "device layout changed (\(previous ?? "?") -> \(current)); "
                                   + "resetting the UEFI variable store so it re-discovers the disk")
            } else {
                HuskLog.log("guest", "staging UEFI variable store")
            }
            // Seeded from the image's own variable store rather than created
            // blank. It already holds the boot entry for this Android build,
            // which saves the firmware a discovery pass -- and if our PCI layout
            // does not match the one it was recorded under, the entry simply
            // fails to resolve and the firmware falls back to scanning for
            // \EFI\BOOT\BOOTAA64.EFI, which is where it would have ended up
            // starting from empty anyway.
            guard let seed = Bundle.main.path(forResource: "lineage-efi-vars-seed", ofType: "fd") else {
                throw NSError(domain: "husk", code: 2, userInfo: [
                    NSLocalizedDescriptionKey: "lineage-efi-vars-seed.fd missing from the app bundle"])
            }
            try? fm.removeItem(atPath: varsPath)
            try fm.copyItem(atPath: seed, toPath: varsPath)
            try? current.write(to: stamp, atomically: true, encoding: .utf8)
        }

        // Userdata. Empty on arrival -- 16 GiB virtual, 192 KB on disk -- and
        // written by Android from its first boot onward, so it is copied out of
        // the read-only bundle once and then left alone. Deliberately NOT tied
        // to the device layout signature: that changes for reasons that have
        // nothing to do with /data, and resetting it factory-resets the guest.
        //
        // It has its own version instead, bumped only when userdata is known to
        // be unusable. It is at v2 because a kernel panic killed the guest
        // partway through building /data, and what it left behind made Android
        // reboot into recovery on every subsequent start
        // ("init_user0_failed"). Nothing short of a clean partition fixes that.
        let seedStamp = URL(fileURLWithPath: userdataPath + ".seed")
        // v3: the guest image changed, so userdata built against the old /system --
        // including a multi-gigabyte snapshot of it -- has to go.
        let seedVersion = "v6"
        let seededWith = try? String(contentsOf: seedStamp, encoding: .utf8)
        if fm.fileExists(atPath: userdataPath), seededWith != seedVersion {
            HuskLog.log("guest", "userdata seed \(seededWith ?? "unversioned") -> \(seedVersion); "
                               + "starting from a clean partition")
            try? fm.removeItem(atPath: userdataPath)
        }
        if !fm.fileExists(atPath: userdataPath) {
            guard let seed = Bundle.main.path(forResource: "lineage-vdb-seed", ofType: "qcow2") else {
                throw NSError(domain: "husk", code: 3, userInfo: [
                    NSLocalizedDescriptionKey: "lineage-vdb-seed.qcow2 missing from the app bundle"])
            }
            try fm.copyItem(atPath: seed, toPath: userdataPath)
            try? seedVersion.write(to: seedStamp, atomically: true, encoding: .utf8)
            HuskLog.log("guest", "userdata disk staged to Documents")
        }
    }

    func download() {
        if case .downloading = state { return }
        state = .downloading(progress: 0, received: 0, total: 0)
        HuskLog.log("guest", "downloading guest image from \(Self.imageURL.absoluteString)")

        let delegate = DownloadDelegate(owner: self)
        let session = URLSession(configuration: .default, delegate: delegate, delegateQueue: nil)
        task = session.downloadTask(with: Self.imageURL)
        task?.resume()
    }

    /// Fetch the pre-booted machine on demand, reporting progress like any other
    /// download.
    ///
    /// This exists because the automatic fetch only ran as the tail of a fresh
    /// guest-image download. Anyone who already had the image -- which is most
    /// people, and was the case that reported this -- turned the setting on and
    /// watched nothing happen, because there was no download for it to follow.
    /// True while the current download is the snapshot rather than the image.
    @Published private(set) var isFetchingSnapshot = false

    func downloadSnapshotNow() {
        if case .downloading = state { return }
        guard !hasShippedSnapshot else {
            HuskLog.log("guest", "snapshot already installed")
            return
        }
        isFetchingSnapshot = true
        state = .downloading(progress: 0, received: 0, total: 0)
        HuskLog.log("guest", "downloading pre-booted snapshot (about 2 GB)")
        let delegate = DownloadDelegate(owner: self, isSnapshot: true)
        let session = URLSession(configuration: .default, delegate: delegate, delegateQueue: nil)
        task = session.downloadTask(with: Self.snapshotURL)
        task?.resume()
    }

    /// Unpack a downloaded snapshot over userdata.
    fileprivate func finishedSnapshot(tempURL: URL) {
        state = .installing
        HuskLog.log("guest", "unpacking snapshot (about 4 GB once expanded)")
        DispatchQueue.global(qos: .userInitiated).async {
            let dest = URL(fileURLWithPath: self.userdataPath)
            do {
                try? FileManager.default.removeItem(at: dest)
                try Self.gunzip(from: tempURL, to: dest)
                try? FileManager.default.removeItem(at: tempURL)
                try Self.imageVersion.write(toFile: self.snapshotStampPath,
                                            atomically: true, encoding: .utf8)
                let size = (try? FileManager.default
                    .attributesOfItem(atPath: dest.path)[.size] as? Int) ?? 0
                HuskLog.log("guest", "snapshot ready (\(size ?? 0) bytes); "
                                   + "Android will be restored, not booted")
                DispatchQueue.main.async { self.isFetchingSnapshot = false; self.state = .ready }
            } catch {
                HuskLog.log("guest", "snapshot unpack failed: \(error)")
                try? FileManager.default.removeItem(at: dest)
                try? FileManager.default.removeItem(atPath: self.snapshotStampPath)
                DispatchQueue.main.async {
                    self.state = .failed("Could not unpack the snapshot: \(error.localizedDescription)")
                }
            }
        }
    }

    /// Fetch the pre-booted machine and unpack it over userdata.
    ///
    /// Runs after the system image is in place, because the snapshot is only
    /// meaningful alongside the image it was booted from.
    func downloadSnapshot(completion: @escaping (Bool) -> Void) {
        HuskLog.log("guest", "downloading pre-booted snapshot (about 2 GB) from "
                           + Self.snapshotURL.absoluteString)
        let t = URLSession.shared.downloadTask(with: Self.snapshotURL) { tmp, resp, err in
            guard let tmp, err == nil,
                  (resp as? HTTPURLResponse)?.statusCode == 200 else {
                HuskLog.log("guest", "snapshot download failed: "
                                   + (err?.localizedDescription ?? "no file"))
                completion(false); return
            }
            let dest = URL(fileURLWithPath: self.userdataPath)
            do {
                try? FileManager.default.removeItem(at: dest)
                try Self.gunzip(from: tmp, to: dest)
                try? FileManager.default.removeItem(at: tmp)
                let size = (try? FileManager.default
                    .attributesOfItem(atPath: dest.path)[.size] as? Int) ?? 0
                HuskLog.log("guest", "snapshot unpacked (\(size ?? 0) bytes)")
                completion(true)
            } catch {
                HuskLog.log("guest", "snapshot unpack failed: \(error)")
                try? FileManager.default.removeItem(at: dest)
                completion(false)
            }
        }
        t.resume()
    }

    /// Streaming gunzip.
    ///
    /// Apple's Compression framework speaks raw DEFLATE, not the gzip
    /// container, so the header is parsed and skipped by hand. Streamed in
    /// chunks because the output is nearly four gigabytes and will not be held
    /// in memory on a phone.
    static func gunzip(from src: URL, to dst: URL) throws {
        let input = try FileHandle(forReadingFrom: src)
        defer { try? input.close() }
        FileManager.default.createFile(atPath: dst.path, contents: nil)
        let output = try FileHandle(forWritingTo: dst)
        defer { try? output.close() }

        // Gzip header: magic, method, flags, mtime, xfl, os -- then optional
        // extra field, name, comment and header CRC, in that order.
        var head = try input.read(upToCount: 10) ?? Data()
        guard head.count == 10, head[0] == 0x1f, head[1] == 0x8b, head[2] == 8 else {
            throw NSError(domain: "husk", code: 1, userInfo:
                [NSLocalizedDescriptionKey: "not a gzip file"])
        }
        let flg = head[3]
        if flg & 0x04 != 0 {                                  // FEXTRA
            let n = try input.read(upToCount: 2) ?? Data()
            let len = Int(n[0]) | (Int(n[1]) << 8)
            _ = try input.read(upToCount: len)
        }
        for mask in [UInt8(0x08), UInt8(0x10)] where flg & mask != 0 {   // FNAME, FCOMMENT
            while let b = try input.read(upToCount: 1), b.count == 1, b[0] != 0 {}
        }
        if flg & 0x02 != 0 { _ = try input.read(upToCount: 2) }          // FHCRC
        head = Data()

        var stream = compression_stream(dst_ptr: UnsafeMutablePointer<UInt8>(bitPattern: 1)!,
                                        dst_size: 0,
                                        src_ptr: UnsafePointer<UInt8>(bitPattern: 1)!,
                                        src_size: 0, state: nil)
        guard compression_stream_init(&stream, COMPRESSION_STREAM_DECODE,
                                      COMPRESSION_ZLIB) == COMPRESSION_STATUS_OK else {
            throw NSError(domain: "husk", code: 2, userInfo:
                [NSLocalizedDescriptionKey: "inflate init failed"])
        }
        defer { compression_stream_destroy(&stream) }

        let chunk = 1 << 20
        let outBuf = UnsafeMutablePointer<UInt8>.allocate(capacity: chunk)
        defer { outBuf.deallocate() }
        var finished = false

        while !finished {
            let data = try input.read(upToCount: chunk) ?? Data()
            let last = data.isEmpty
            try data.withUnsafeBytes { (raw: UnsafeRawBufferPointer) in
                stream.src_ptr = raw.bindMemory(to: UInt8.self).baseAddress
                    ?? UnsafePointer<UInt8>(bitPattern: 1)!
                stream.src_size = data.count
                repeat {
                    stream.dst_ptr = outBuf
                    stream.dst_size = chunk
                    let st = compression_stream_process(&stream, last ? Int32(COMPRESSION_STREAM_FINALIZE.rawValue) : 0)
                    let produced = chunk - stream.dst_size
                    if produced > 0 {
                        output.write(Data(bytes: outBuf, count: produced))
                    }
                    if st == COMPRESSION_STATUS_END { finished = true; break }
                    if st == COMPRESSION_STATUS_ERROR {
                        throw NSError(domain: "husk", code: 3, userInfo:
                            [NSLocalizedDescriptionKey: "inflate failed"])
                    }
                } while stream.src_size > 0 || (last && !finished)
            }
            if last { finished = true }
        }
    }

    func cancel() {
        task?.cancel()
        task = nil
        state = .missing
        HuskLog.log("guest", "download cancelled")
    }

    fileprivate func finished(tempURL: URL) {
        state = .installing
        HuskLog.log("guest", "download complete; installing")
        // Validate BEFORE installing, so a bad download never becomes the thing
        // QEMU is asked to boot.
        let check = Self.validate(path: tempURL.path)
        if let why = check.problem {
            try? FileManager.default.removeItem(at: tempURL)
            HuskLog.log("guest", "downloaded file rejected: \(why)")
            state = .failed("Download did not produce a disk image: \(why)")
            return
        }
        let size = check.size ?? 0
        do {
                let dest = URL(fileURLWithPath: diskPath)
                try? FileManager.default.removeItem(at: dest)
                // Moved, not copied: the image is over a gigabyte and the device
                // does not need two of them on disk at once.
                try FileManager.default.moveItem(at: tempURL, to: dest)
                try? Self.imageVersion.write(toFile: versionStampPath,
                                             atomically: true, encoding: .utf8)
                HuskLog.log("guest", "guest image ready (\(size) bytes, \(Self.imageVersion))")
                // The snapshot only makes sense next to the image it was booted
                // from, so it is fetched after, not alongside.
                if Self.wantsSnapshot, !hasShippedSnapshot {
                    state = .installing
                    downloadSnapshot { ok in
                        DispatchQueue.main.async {
                            if ok {
                                try? Self.imageVersion.write(toFile: self.snapshotStampPath,
                                                            atomically: true, encoding: .utf8)
                                HuskLog.log("guest", "pre-booted snapshot installed; "
                                                   + "first launch will restore instead of booting")
                            } else {
                                HuskLog.log("guest", "no snapshot; Android will boot from cold")
                            }
                            self.state = .ready
                        }
                    }
                } else {
                    state = .ready
                }
        } catch {
            HuskLog.log("guest", "FAIL: \(error.localizedDescription)")
            state = .failed(error.localizedDescription)
        }
    }

    fileprivate func progressed(received: Int64, total: Int64) {
        let p = total > 0 ? Double(received) / Double(total) : 0
        state = .downloading(progress: p, received: received, total: total)
    }

    fileprivate func failed(_ message: String) {
        HuskLog.log("guest", "download FAILED: \(message)")
        state = .failed(message)
    }
}

private final class DownloadDelegate: NSObject, URLSessionDownloadDelegate {
    weak var owner: GuestImage?
    /// Which file this delegate is carrying. Both report progress the same way;
    /// only what happens at the end differs.
    let isSnapshot: Bool
    init(owner: GuestImage, isSnapshot: Bool = false) {
        self.owner = owner
        self.isSnapshot = isSnapshot
    }

    func urlSession(_ s: URLSession, downloadTask: URLSessionDownloadTask,
                    didFinishDownloadingTo location: URL) {
        // A download task reports "finished" for any completed HTTP exchange,
        // including a 404 -- whose body then lands on disk as if it were the file
        // asked for. This is exactly how a 9-byte "Not Found" once became the
        // guest disk image. Check the status before treating it as the payload.
        if let http = downloadTask.response as? HTTPURLResponse,
           !(200...299).contains(http.statusCode) {
            let body = (try? String(contentsOf: location, encoding: .utf8))?
                .trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
            let detail = body.isEmpty ? "" : " — server said: \(body.prefix(200))"
            let url = downloadTask.originalRequest?.url?.absoluteString ?? "?"
            Task { @MainActor in
                self.owner?.failed("HTTP \(http.statusCode) from \(url)\(detail)")
            }
            return
        }

        // The temp file is deleted when this returns, so move it somewhere stable
        // before handing it over.
        let stable = FileManager.default.temporaryDirectory
            .appendingPathComponent(isSnapshot ? "husk-snapshot-download.gz"
                                              : "husk-guest-download.qcow2")
        try? FileManager.default.removeItem(at: stable)
        do {
            try FileManager.default.moveItem(at: location, to: stable)
        } catch {
            Task { @MainActor in
                self.owner?.failed("could not stage download: \(error.localizedDescription)")
            }
            return
        }
        Task { @MainActor in
            if self.isSnapshot { self.owner?.finishedSnapshot(tempURL: stable) }
            else               { self.owner?.finished(tempURL: stable) }
        }
    }

    func urlSession(_ s: URLSession, downloadTask: URLSessionDownloadTask,
                    didWriteData bytesWritten: Int64,
                    totalBytesWritten: Int64, totalBytesExpectedToWrite: Int64) {
        Task { @MainActor in
            self.owner?.progressed(received: totalBytesWritten,
                                   total: totalBytesExpectedToWrite)
        }
    }

    func urlSession(_ s: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        if let error {
            Task { @MainActor in self.owner?.failed(error.localizedDescription) }
        }
    }
}
