// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// Takes the APKs out of an app bundle: an .xapk (APKPure), .apkm (APKMirror) or .apks (bundletool), which are zip files holding a base APK and its splits and asset packs.
///
/// Only what is needed is read: the zip's central directory, then each APK's bytes copied across in chunks. An APK inside a bundle is stored, not compressed (it is already a zip),
/// so nothing is inflated and a 200 MB asset pack costs a few megabytes of memory, not 200.
enum BundleUnpacker {
    static let extensions: Set<String> = ["xapk", "apkm", "apks"]

    enum Failure: LocalizedError {
        case notAZip, unsupported(String)
        var errorDescription: String? {
            switch self {
            case .notAZip: return localizedString("That file is not a bundle Husk can read.")
            case .unsupported(let why): return why
            }
        }
    }

    /// Whether an APK is a split or an asset pack, as the bundle formats name them, rather than the app itself.
    static func isSplit(_ name: String) -> Bool {
        let n = name.lowercased()
        if n == "base.apk" || n == "base-master.apk" { return false }
        return n.hasPrefix("config.") || n.hasPrefix("split_") || n.hasPrefix("asset_pack") || n.hasPrefix("base-") || n.hasPrefix("install_time")
    }

    /// The order an app's APKs are given to the runtime in, which takes only four: the app itself, then what it cannot run without (its 64-bit libraries, its asset pack),
    /// then the language and screen-density splits, which it does not need.
    static func rank(_ name: String) -> Int {
        let n = name.lowercased()
        if !isSplit(n) { return 0 }
        if n.contains("arm64") { return 1 }
        if n.hasPrefix("asset_pack") || n.hasPrefix("install_time") { return 2 }
        return 3
    }

    /// Copies every APK in `bundle` into `dir`. Returns their names.
    @discardableResult
    static func unpack(_ bundle: URL, into dir: URL) throws -> [String] {
        let scoped = bundle.startAccessingSecurityScopedResource()
        defer { if scoped { bundle.stopAccessingSecurityScopedResource() } }
        let file = try FileHandle(forReadingFrom: bundle)
        defer { try? file.close() }

        let size = try file.seekToEnd()
        // The end-of-central-directory record is in the last 64 KB plus 22 bytes.
        let tail = min(size, 70_000)
        try file.seek(toOffset: size - tail)
        guard let end = try file.read(upToCount: Int(tail)), end.count >= 22 else { throw Failure.notAZip }
        let bytes = [UInt8](end)
        var e = bytes.count - 22
        while e >= 0, !(bytes[e] == 0x50 && bytes[e + 1] == 0x4b && bytes[e + 2] == 5 && bytes[e + 3] == 6) { e -= 1 }
        guard e >= 0 else { throw Failure.notAZip }
        let count = Int(le16(bytes, e + 10))
        let dirSize = UInt64(le32(bytes, e + 12)), dirOffset = UInt64(le32(bytes, e + 16))
        if dirOffset == 0xffff_ffff || count == 0xffff { throw Failure.unsupported(localizedString("That bundle uses a zip format (Zip64) Husk does not read yet.")) }

        try file.seek(toOffset: dirOffset)
        guard let directory = try file.read(upToCount: Int(dirSize)), directory.count == Int(dirSize) else { throw Failure.notAZip }
        let cd = [UInt8](directory)

        var names: [String] = []
        var p = 0
        for _ in 0..<count {
            guard p + 46 <= cd.count, le32(cd, p) == 0x0201_4b50 else { break }
            let method = le16(cd, p + 10)
            let compressed = UInt64(le32(cd, p + 20))
            let nameLen = Int(le16(cd, p + 28)), extraLen = Int(le16(cd, p + 30)), commentLen = Int(le16(cd, p + 32))
            let localOffset = UInt64(le32(cd, p + 42))
            guard p + 46 + nameLen <= cd.count else { break }
            let name = String(decoding: cd[(p + 46)..<(p + 46 + nameLen)], as: UTF8.self)
            p += 46 + nameLen + extraLen + commentLen

            let base = (name as NSString).lastPathComponent
            guard base.lowercased().hasSuffix(".apk"), !name.hasSuffix("/") else { continue }
            guard method == 0 else { throw Failure.unsupported(String(localized: "\(base) is compressed inside its bundle, which Husk cannot unpack yet.")) }

            // The local header has its own name and extra lengths; the data follows them.
            try file.seek(toOffset: localOffset)
            guard let local = try file.read(upToCount: 30), local.count == 30 else { throw Failure.notAZip }
            let lh = [UInt8](local)
            let dataStart = localOffset + 30 + UInt64(le16(lh, 26)) + UInt64(le16(lh, 28))
            try file.seek(toOffset: dataStart)

            let dest = dir.appendingPathComponent(base)
            FileManager.default.createFile(atPath: dest.path, contents: nil)
            let out = try FileHandle(forWritingTo: dest)
            defer { try? out.close() }
            var left = compressed
            while left > 0 {
                let chunk = Int(min(left, 4 << 20))
                guard let data = try file.read(upToCount: chunk), !data.isEmpty else { throw Failure.notAZip }
                try out.write(contentsOf: data)
                left -= UInt64(data.count)
            }
            names.append(base)
        }
        if names.isEmpty { throw Failure.unsupported(localizedString("There are no APKs in that bundle.")) }
        return names
    }

    private static func le16(_ b: [UInt8], _ o: Int) -> UInt16 { UInt16(b[o]) | UInt16(b[o + 1]) << 8 }
    private static func le32(_ b: [UInt8], _ o: Int) -> UInt32 {
        UInt32(b[o]) | UInt32(b[o + 1]) << 8 | UInt32(b[o + 2]) << 16 | UInt32(b[o + 3]) << 24
    }
}
