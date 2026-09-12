// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// Manages the Linux+Waydroid guest disk that lives in Documents.
///
/// The guest is downloaded at first run rather than bundled. A provisioned
/// Debian+Waydroid image is ~1.2 GB, which is not something to put in an IPA --
/// and Android itself is not in it at all: `waydroid init` fetches LineageOS on
/// the device, through Waydroid's own setup path, so Husk never redistributes it.
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
    static let imageURL = URL(string:
        "https://github.com/Leviidev/Husk/releases/download/guest-v1/husk-guest.qcow2")!

    // nonisolated: QEMU's own thread builds its command line from these, and they
    // are pure path arithmetic with no mutable state to protect.
    nonisolated private var documents: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
    }
    nonisolated var diskPath: String { documents.appendingPathComponent("husk-guest.qcow2").path }
    nonisolated var varsPath: String { documents.appendingPathComponent("edk2-vars.fd").path }
    nonisolated var firmwarePath: String { documents.appendingPathComponent("edk2-aarch64-code.fd").path }

    private var task: URLSessionDownloadTask?

    func refresh() {
        let exists = FileManager.default.fileExists(atPath: diskPath)
        if exists, case .ready = state {} else if exists {
            state = .ready
            HuskLog.log("guest", "guest disk present: \(diskPath)")
        } else if case .downloading = state {
            // leave it alone
        } else {
            state = .missing
        }
    }

    /// Create the UEFI variable store beside the bundled firmware.
    ///
    /// The firmware itself ships raw in the app bundle. It is 64 MiB, but almost
    /// entirely zeros, so the IPA's own zip compresses it to about a megabyte --
    /// cheaper than carrying a decompressor for it.
    func prepareFirmware() throws {
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
        if !fm.fileExists(atPath: varsPath) {
            // UEFI wants a 64 MiB writable variable store next to the code volume.
            HuskLog.log("guest", "creating UEFI variable store")
            let vars = Data(count: 64 * 1024 * 1024)
            try vars.write(to: URL(fileURLWithPath: varsPath))
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

    func cancel() {
        task?.cancel()
        task = nil
        state = .missing
        HuskLog.log("guest", "download cancelled")
    }

    fileprivate func finished(tempURL: URL) {
        state = .installing
        HuskLog.log("guest", "download complete; installing")
        do {
            let dest = URL(fileURLWithPath: diskPath)
            try? FileManager.default.removeItem(at: dest)
            // Moved, not copied: the image is over a gigabyte and the device does
            // not need two of them on disk at once.
            try FileManager.default.moveItem(at: tempURL, to: dest)
            let attrs = try? FileManager.default.attributesOfItem(atPath: diskPath)
            let size = (attrs?[.size] as? NSNumber)?.int64Value ?? 0
            HuskLog.log("guest", "guest image ready (\(size) bytes)")
            state = .ready
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
    init(owner: GuestImage) { self.owner = owner }

    func urlSession(_ s: URLSession, downloadTask: URLSessionDownloadTask,
                    didFinishDownloadingTo location: URL) {
        // The temp file is deleted when this returns, so copy it somewhere stable
        // before handing it over.
        let stable = FileManager.default.temporaryDirectory
            .appendingPathComponent("husk-guest-download.zst")
        try? FileManager.default.removeItem(at: stable)
        try? FileManager.default.moveItem(at: location, to: stable)
        Task { @MainActor in self.owner?.finished(tempURL: stable) }
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
