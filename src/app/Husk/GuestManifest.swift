// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import CryptoKit

/// What the release says the guest should be, and whether what is installed
/// matches it.
///
/// This replaces comparing version strings. A stamp saying "v10" only records
/// what the app *believed* it installed, and twice now that belief has been
/// wrong in ways nothing detected: a stamp survived a generation bump, and an
/// asset was published under a new name carrying the previous generation's
/// bytes. Both look identical from the app's side -- the version matches, so
/// there is nothing to download -- and both produce a guest that fails later,
/// somewhere unrelated.
///
/// A digest cannot be wrong in that way. The release publishes the SHA-256 of
/// every file it holds; the app records the SHA-256 of what it actually wrote
/// to disk. If the two differ, the install is out of date, whatever either side
/// calls itself.
struct GuestManifest: Codable, Equatable {
    struct Image: Codable, Equatable {
        let file: String
        let sha256: String
        let size: Int64
    }

    struct Snapshot: Codable, Equatable {
        /// In order. Concatenating them reproduces the gzip archive, whose
        /// digest is `sha256` -- GitHub refuses an asset of 2 GiB or more, so
        /// the archive is published in pieces.
        let parts: [String]
        let sha256: String
        let size: Int64
        /// The machine the snapshot was saved on. QEMU refuses a restore whose
        /// RAM differs by a byte ("Size mismatch: huskram"), so these travel
        /// with the snapshot rather than being compiled into the app -- an app
        /// built before a snapshot cannot know them otherwise.
        let guestMiB: Int
        let xres: Int
        let yres: Int
    }

    /// Only for logs and the update prompt. Nothing is decided from it.
    let generation: String
    let image: Image
    let snapshot: Snapshot

    static let manifestURL = URL(string: "https://github.com/Leviidev/Husk/releases/"
                                       + "download/\(GuestImage.dependenciesTag)/manifest.json")!

    /// Fetch the manifest, or nil if it cannot be had.
    ///
    /// Never throws to the caller. Being offline, or on a network that answers
    /// every request with a login page, must not stop someone launching a guest
    /// that is already on their phone -- an update check is not a precondition
    /// for running.
    static func fetch() async -> GuestManifest? {
        // Release downloads are served through a CDN that will happily hand back
        // a manifest from before the last publish. The app is asking precisely
        // whether anything changed, so a cached answer is the one answer it
        // cannot use.
        var request = URLRequest(url: manifestURL)
        request.cachePolicy = .reloadIgnoringLocalAndRemoteCacheData
        request.timeoutInterval = 15
        do {
            let (data, response) = try await URLSession.shared.data(for: request)
            guard let http = response as? HTTPURLResponse,
                  (200...299).contains(http.statusCode) else {
                HuskLog.log("guest", "manifest unavailable (HTTP "
                          + "\((response as? HTTPURLResponse)?.statusCode ?? 0))")
                return nil
            }
            let manifest = try JSONDecoder().decode(GuestManifest.self, from: data)
            HuskLog.log("guest", "manifest \(manifest.generation): image "
                      + "\(manifest.image.sha256.prefix(12))…, snapshot "
                      + "\(manifest.snapshot.sha256.prefix(12))…")
            return manifest
        } catch {
            HuskLog.log("guest", "manifest fetch failed: \(error.localizedDescription)")
            return nil
        }
    }

    func url(for file: String) -> URL {
        URL(string: "https://github.com/Leviidev/Husk/releases/download/"
                  + "\(GuestImage.dependenciesTag)/\(file)")!
    }

    var partURLs: [URL] { snapshot.parts.map { url(for: $0) } }
    var imageURL: URL { url(for: image.file) }
}

/// What is out of date, if anything.
enum GuestUpdate: Equatable {
    case none
    /// The system image differs. The snapshot is only valid against the image it
    /// was booted from, so this always implies the snapshot too.
    case image(bytes: Int64)
    /// Only the pre-booted snapshot differs -- or is simply not installed.
    case snapshot(bytes: Int64)

    var isSomething: Bool { self != .none }

    var title: String {
        switch self {
        case .none:     return ""
        case .image:    return "A new Android image is available"
        case .snapshot: return "A new pre-booted snapshot is available"
        }
    }

    var detail: String {
        let bytes: Int64
        switch self {
        case .none:              return ""
        case .image(let b):      bytes = b
        case .snapshot(let b):   bytes = b
        }
        let gb = Double(bytes) / 1_000_000_000
        let size = String(format: "%.1f GB", gb)
        switch self {
        case .image:
            return "Downloading it replaces the Android system and its pre-booted "
                 + "snapshot (\(size)). Apps you installed are kept on the "
                 + "userdata disk, but a snapshot only restores against the image "
                 + "it was saved on, so both are replaced together."
        case .snapshot:
            return "Without it Android boots from cold, which takes several "
                 + "minutes. The download is \(size)."
        case .none:
            return ""
        }
    }
}

/// Streaming SHA-256, so a gigabyte can be digested as it is written rather than
/// read back afterwards.
///
/// The snapshot is appended part by part and the system image arrives in one
/// piece, and neither is worth a second full pass over the file on a phone.
final class DigestWriter {
    private var hasher = SHA256()

    func update(_ data: Data) { hasher.update(data: data) }

    /// Lowercase hex, matching `shasum -a 256` and what the manifest carries.
    func finish() -> String {
        hasher.finalize().map { String(format: "%02x", $0) }.joined()
    }

    /// Digest a file that is already on disk, in chunks.
    static func ofFile(at path: String) -> String? {
        guard let handle = FileHandle(forReadingAtPath: path) else { return nil }
        defer { try? handle.close() }
        let writer = DigestWriter()
        while let chunk = try? handle.read(upToCount: 4 << 20), !chunk.isEmpty {
            writer.update(chunk)
        }
        return writer.finish()
    }
}
