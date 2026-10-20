// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// A game's saves, out of Husk and back in: one .zip with everything the game wrote, and a manifest saying whose it is.
///
/// What a game saves lives in its folder beside its APKs, in the data folder of the engine that runs it (unity-data,
/// cocos-data, ...): its files, its preferences, Geode's mods. A backup is those folders, less what the game or Husk can make
/// again (caches, unpacked engine files). The zip is stored, not compressed, so it is quick to make and the Files app opens it.
/// Restoring replaces the game's data with the backup's, after checking it is a backup of the same game.
enum SaveBackup {
    struct Failure: LocalizedError {
        let message: String
        init(_ m: String) { message = m }
        var errorDescription: String? { message }
    }

    /// Folders that are made again by the game or by Husk, and are left out of a backup.
    private static let skipped: Set<String> = ["cache", "il2cpp", "UnityShaderCache", "geode-bin", "code_cache", "tmp"]
    private static let manifestName = "husk-backup.json"

    private static func dataFolders(_ appDir: URL) -> [URL] {
        let fm = FileManager.default
        return ((try? fm.contentsOfDirectory(at: appDir, includingPropertiesForKeys: [.isDirectoryKey])) ?? []).filter {
            $0.lastPathComponent.hasSuffix("-data") && ((try? $0.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) ?? false)
        }
    }

    /// Whether the game has saved anything yet.
    static func hasData(_ app: TLApp) -> Bool { !dataFolders(TranslationLayer.root.appendingPathComponent(app.id)).isEmpty }

    // MARK: backup

    /// Write the backup to a temporary file and return it, for the share sheet or Save to Files.
    static func export(_ app: TLApp) throws -> URL {
        let appDir = TranslationLayer.root.appendingPathComponent(app.id)
        var files: [(name: String, url: URL)] = []
        for folder in dataFolders(appDir) { collect(folder, prefix: folder.lastPathComponent, into: &files) }
        guard !files.isEmpty else { throw Failure(L10n.tr("\(app.label) has not saved anything yet.")) }

        let manifest: [String: Any] = [
            "format": 1, "package": app.packageName ?? "", "label": app.label,
            "created": ISO8601DateFormatter().string(from: Date()),
            "husk": Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?",
        ]
        let stamp = Date().formatted(.iso8601.year().month().day())
        let safe = app.label.replacingOccurrences(of: "/", with: "-")
        let out = FileManager.default.temporaryDirectory.appendingPathComponent("\(safe) Saves \(stamp).zip")
        let zip = try ZipWriter(out)
        try zip.add(manifestName, data: try JSONSerialization.data(withJSONObject: manifest, options: [.prettyPrinted, .sortedKeys]))
        for f in files { try zip.add(f.name, file: f.url) }
        try zip.finish()
        HuskLog.log("saves", "backed up \(files.count) file(s) of \(app.label) to \(out.lastPathComponent)")
        return out
    }

    private static func collect(_ dir: URL, prefix: String, into files: inout [(name: String, url: URL)]) {
        let fm = FileManager.default
        for item in (try? fm.contentsOfDirectory(at: dir, includingPropertiesForKeys: [.isDirectoryKey, .isSymbolicLinkKey])) ?? [] {
            let name = item.lastPathComponent
            let values = try? item.resourceValues(forKeys: [.isDirectoryKey, .isSymbolicLinkKey])
            if values?.isSymbolicLink == true { continue }
            if values?.isDirectory == true {
                if skipped.contains(name) { continue }
                collect(item, prefix: prefix + "/" + name, into: &files)
            } else if !name.hasSuffix(".husk-new") {
                files.append((prefix + "/" + name, item))
            }
        }
    }

    // MARK: restore

    /// Replace the game's saves with a backup's. The game must not be running.
    static func restore(_ app: TLApp, from zipURL: URL) throws -> Int {
        let scoped = zipURL.startAccessingSecurityScopedResource()
        defer { if scoped { zipURL.stopAccessingSecurityScopedResource() } }
        let zip = try ZipReader(zipURL)
        guard let mdata = try zip.data(manifestName),
              let manifest = try JSONSerialization.jsonObject(with: mdata) as? [String: Any] else {
            throw Failure(L10n.tr("That file is not a Husk save backup."))
        }
        let pkg = manifest["package"] as? String ?? ""
        if let mine = app.packageName, !pkg.isEmpty, pkg != mine {
            throw Failure(L10n.tr("That backup is of \(manifest["label"] as? String ?? pkg), not \(app.label)."))
        }

        // Unpacked beside the game first, and swapped in only once all of it is out: a backup that fails halfway leaves
        // the saves that were there.
        let fm = FileManager.default
        let appDir = TranslationLayer.root.appendingPathComponent(app.id)
        let staging = appDir.appendingPathComponent(".restore", isDirectory: true)
        try? fm.removeItem(at: staging)
        try fm.createDirectory(at: staging, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: staging) }
        var count = 0
        for name in zip.names where name != manifestName {
            let parts = name.split(separator: "/")
            guard parts.count >= 2, parts[0].hasSuffix("-data"), !parts.contains(".."), !name.hasPrefix("/") else { continue }
            let dest = staging.appendingPathComponent(name)
            try fm.createDirectory(at: dest.deletingLastPathComponent(), withIntermediateDirectories: true)
            try zip.extract(name, to: dest)
            count += 1
        }
        guard count > 0 else { throw Failure(L10n.tr("That backup has no saves in it.")) }
        for folder in (try? fm.contentsOfDirectory(at: staging, includingPropertiesForKeys: nil)) ?? [] {
            let live = appDir.appendingPathComponent(folder.lastPathComponent)
            let old = appDir.appendingPathComponent(".old-" + folder.lastPathComponent)
            try? fm.removeItem(at: old)
            if fm.fileExists(atPath: live.path) { try fm.moveItem(at: live, to: old) }
            try fm.moveItem(at: folder, to: live)
            try? fm.removeItem(at: old)
        }
        HuskLog.log("saves", "restored \(count) file(s) of \(app.label) from \(zipURL.lastPathComponent)")
        return count
    }
}

// MARK: - a stored zip, written and read

private let crcTable: [UInt32] = (0..<256).map { i -> UInt32 in
    var c = UInt32(i)
    for _ in 0..<8 { c = (c & 1) != 0 ? 0xEDB8_8320 ^ (c >> 1) : c >> 1 }
    return c
}

private func crc32(_ crc: UInt32, _ data: Data) -> UInt32 {
    var c = ~crc
    data.withUnsafeBytes { (p: UnsafeRawBufferPointer) in
        for b in p { c = crcTable[Int((c ^ UInt32(b)) & 0xFF)] ^ (c >> 8) }
    }
    return ~c
}

private extension Data {
    mutating func le16(_ v: Int) { append(UInt8(v & 0xFF)); append(UInt8((v >> 8) & 0xFF)) }
    mutating func le32(_ v: UInt32) { for k in 0..<4 { append(UInt8((v >> (8 * UInt32(k))) & 0xFF)) } }
}

/// Entries stored as they are (method 0). Sizes are known before each entry is written, so the CRC goes in the local header.
private final class ZipWriter {
    private let out: FileHandle
    private var central = Data()
    private var count = 0
    private var offset: UInt64 = 0

    init(_ url: URL) throws {
        try? FileManager.default.removeItem(at: url)
        FileManager.default.createFile(atPath: url.path, contents: nil)
        out = try FileHandle(forWritingTo: url)
    }

    func add(_ name: String, data: Data) throws { try write(name, size: UInt64(data.count), crc: crc32(0, data)) { try self.out.write(contentsOf: data) } }

    func add(_ name: String, file: URL) throws {
        let input = try FileHandle(forReadingFrom: file)
        defer { try? input.close() }
        var crc: UInt32 = 0, size: UInt64 = 0
        while let chunk = try input.read(upToCount: 1 << 20), !chunk.isEmpty { crc = crc32(crc, chunk); size += UInt64(chunk.count) }
        guard size < 0xFFFF_FFFF, offset < 0xFFFF_FFFF else { throw SaveBackup.Failure(L10n.tr("A save file is too large to back up.")) }
        try input.seek(toOffset: 0)
        try write(name, size: size, crc: crc) {
            var left = size
            while left > 0, let chunk = try input.read(upToCount: Int(Swift.min(left, 1 << 20))), !chunk.isEmpty {
                try self.out.write(contentsOf: chunk); left -= UInt64(chunk.count)
            }
        }
    }

    private func write(_ name: String, size: UInt64, crc: UInt32, body: () throws -> Void) throws {
        let n = Data(name.utf8)
        var h = Data()
        h.le32(0x0403_4b50); h.le16(20); h.le16(0x0800); h.le16(0); h.le16(0); h.le16(0x21)
        h.le32(crc); h.le32(UInt32(size)); h.le32(UInt32(size)); h.le16(n.count); h.le16(0); h.append(n)
        try out.write(contentsOf: h)
        try body()

        var c = Data()
        c.le32(0x0201_4b50); c.le16(0x031E); c.le16(20); c.le16(0x0800); c.le16(0); c.le16(0); c.le16(0x21)
        c.le32(crc); c.le32(UInt32(size)); c.le32(UInt32(size)); c.le16(n.count); c.le16(0); c.le16(0); c.le16(0); c.le16(0)
        c.le32(0o100644 << 16); c.le32(UInt32(offset)); c.append(n)
        central.append(c)
        offset += UInt64(h.count) + size
        count += 1
    }

    func finish() throws {
        try out.write(contentsOf: central)
        var e = Data()
        e.le32(0x0605_4b50); e.le16(0); e.le16(0); e.le16(count); e.le16(count)
        e.le32(UInt32(central.count)); e.le32(UInt32(offset)); e.le16(0)
        try out.write(contentsOf: e)
        try out.close()
    }
}

/// Reads stored entries back -- what ZipWriter writes. A zip that went through another tool and came back compressed is
/// not read; the error says so.
private final class ZipReader {
    private struct Entry { let method: Int; let size: UInt64; let local: UInt64 }
    private let file: FileHandle
    private var entries: [String: Entry] = [:]
    private(set) var names: [String] = []

    init(_ url: URL) throws {
        file = try FileHandle(forReadingFrom: url)
        let size = try file.seekToEnd()
        let tail = min(size, 70_000)
        try file.seek(toOffset: size - tail)
        let b = [UInt8](try file.read(upToCount: Int(tail)) ?? Data())
        var e = b.count - 22
        while e >= 0, !(b[e] == 0x50 && b[e + 1] == 0x4b && b[e + 2] == 5 && b[e + 3] == 6) { e -= 1 }
        guard e >= 0 else { throw SaveBackup.Failure(L10n.tr("That file is not a zip.")) }
        let count = Int(Self.u16(b, e + 10)), dirSize = Int(Self.u32(b, e + 12)), dirOff = UInt64(Self.u32(b, e + 16))
        try file.seek(toOffset: dirOff)
        let cd = [UInt8](try file.read(upToCount: dirSize) ?? Data())
        var p = 0
        for _ in 0..<count {
            guard p + 46 <= cd.count, Self.u32(cd, p) == 0x0201_4b50 else { break }
            let nl = Int(Self.u16(cd, p + 28)), xl = Int(Self.u16(cd, p + 30)), cl = Int(Self.u16(cd, p + 32))
            guard p + 46 + nl <= cd.count else { break }
            let name = String(decoding: cd[(p + 46)..<(p + 46 + nl)], as: UTF8.self)
            if !name.hasSuffix("/") {
                entries[name] = Entry(method: Int(Self.u16(cd, p + 10)), size: UInt64(Self.u32(cd, p + 20)), local: UInt64(Self.u32(cd, p + 42)))
                names.append(name)
            }
            p += 46 + nl + xl + cl
        }
    }
    deinit { try? file.close() }

    private func start(_ e: Entry) throws -> UInt64 {
        guard e.method == 0 else { throw SaveBackup.Failure(L10n.tr("That backup was recompressed by another app; Husk reads only the backups it makes.")) }
        try file.seek(toOffset: e.local)
        let h = [UInt8](try file.read(upToCount: 30) ?? Data())
        guard h.count == 30 else { throw SaveBackup.Failure(L10n.tr("That backup is damaged.")) }
        return e.local + 30 + UInt64(Self.u16(h, 26)) + UInt64(Self.u16(h, 28))
    }

    func data(_ name: String) throws -> Data? {
        guard let e = entries[name] else { return nil }
        try file.seek(toOffset: try start(e))
        return try file.read(upToCount: Int(e.size))
    }

    func extract(_ name: String, to dest: URL) throws {
        guard let e = entries[name] else { return }
        try file.seek(toOffset: try start(e))
        FileManager.default.createFile(atPath: dest.path, contents: nil)
        let out = try FileHandle(forWritingTo: dest)
        defer { try? out.close() }
        var left = e.size
        while left > 0 {
            guard let chunk = try file.read(upToCount: Int(min(left, 1 << 20))), !chunk.isEmpty else { throw SaveBackup.Failure(L10n.tr("That backup is damaged.")) }
            try out.write(contentsOf: chunk); left -= UInt64(chunk.count)
        }
    }

    private static func u16(_ b: [UInt8], _ o: Int) -> UInt16 { UInt16(b[o]) | UInt16(b[o + 1]) << 8 }
    private static func u32(_ b: [UInt8], _ o: Int) -> UInt32 {
        UInt32(b[o]) | UInt32(b[o + 1]) << 8 | UInt32(b[o + 2]) << 16 | UInt32(b[o + 3]) << 24
    }
}
