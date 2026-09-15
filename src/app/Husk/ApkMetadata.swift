// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// An app's real name and real icon, read the way Android reads them.
///
/// Husk used to guess both. The name was the last component of the package with
/// a capital letter on it, so `com.example.test` became "Test"; the icon was the
/// largest PNG in the APK whose path happened to contain "launcher" or "icon".
/// The second guess fails outright on any modern app: since Android 8 the
/// launcher icon is usually an adaptive XML drawable pointing at WebP bitmaps,
/// and a search for PNGs finds either nothing or something that was never the
/// icon — a notification glyph, a splash asset, whatever sorted first.
///
/// What Android does instead is look the answer up. `AndroidManifest.xml` says
/// which resource is the label and which is the icon; `resources.arsc` says what
/// those resources are, per language and per screen density. Both files are in
/// every APK, both are readable without root, and parsing them is what `aapt`
/// does. That is what this is: enough of those two formats to answer two
/// questions, with every length and offset checked, because the input is a file
/// someone else built.
enum ApkMetadata {
    struct Info {
        var label: String?
        /// Path inside the APK of the best bitmap for the icon.
        var iconEntry: String?
    }

    /// Attribute ids from `android.R.attr`. These are fixed for all time —
    /// they are the framework's own resource table, not the app's.
    private static let attrLabel: UInt32 = 0x0101_0001
    private static let attrIcon: UInt32 = 0x0101_0002
    private static let attrRoundIcon: UInt32 = 0x0101_052C

    static func read(manifest: Data, resources: Data) -> Info {
        var info = Info()
        guard let refs = applicationAttributes(manifest) else { return info }

        // A literal label ("android:label=Test") needs no table at all.
        if case .literal(let text)? = refs[attrLabel] { info.label = clean(text) }

        guard let table = ResourceTable(resources) else { return info }

        if info.label == nil, case .reference(let id)? = refs[attrLabel] {
            info.label = clean(table.string(for: id))
        }

        // The round icon first when there is one: on a launcher that asks for
        // round icons it is the artwork the app actually wants shown, and when
        // there is none this falls straight through.
        for attr in [attrRoundIcon, attrIcon] {
            guard info.iconEntry == nil, case .reference(let id)? = refs[attr] else { continue }
            info.iconEntry = table.bitmap(for: id)
        }
        return info
    }

    /// A label that could plausibly be shown to someone. A mis-parse produces
    /// control characters or a hundred lines of XML, not a name.
    private static func clean(_ text: String?) -> String? {
        guard let text else { return nil }
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty, trimmed.count <= 60,
              !trimmed.contains(where: { $0.asciiValue.map { $0 < 0x20 } ?? false })
        else { return nil }
        return trimmed
    }

    /// The drawable an adaptive icon draws in front.
    ///
    /// Since Android 8 an app's icon is usually not a picture but an XML
    /// saying "compose these two layers and let the launcher mask them". There
    /// is no picture to take, so this takes the foreground layer — the part
    /// that is the app's mark — and lets it stand alone. It is the same
    /// artwork, minus a coloured backdrop the launcher would have supplied.
    static func adaptiveLayer(_ xml: Data) -> UInt32? {
        var background: UInt32?
        for element in elements(xml) {
            guard element.name == "foreground" || element.name == "background" else { continue }
            for attr in element.attributes where attr.name == "drawable" {
                guard attr.dataType == 0x01 else { continue }   // TYPE_REFERENCE
                if element.name == "foreground" { return attr.data }
                background = background ?? attr.data
            }
        }
        return background
    }

    // MARK: - AndroidManifest.xml

    enum Attribute {
        case reference(UInt32)
        case literal(String)
    }

    struct Element {
        let name: String
        let attributes: [Attr]
    }

    struct Attr {
        /// The attribute's own name, e.g. "label" or "drawable".
        let name: String
        /// Its framework resource id, when the file carries the mapping.
        let id: UInt32
        let dataType: UInt8
        let data: UInt32
        /// Its literal text, for the attributes that carry one outright.
        let value: String?
    }

    /// Walk a binary XML file's start elements.
    ///
    /// Both files this reads are the same format — AndroidManifest.xml and an
    /// adaptive icon's XML are both compiled resources — so the walk is written
    /// once and the callers decide what they are looking for.
    static func elements(_ data: Data) -> [Element] {
        let r = Reader(data)
        guard r.u16(0) == 0x0003 else { return [] }   // RES_XML_TYPE

        var pool: StringPool?
        var resourceIds: [UInt32] = []
        var out: [Element] = []
        var p = Int(r.u16(2))

        while p + 8 <= data.count {
            let type = r.u16(p)
            let headerSize = Int(r.u16(p + 2))
            let size = Int(r.u32(p + 4))
            guard size >= 8, headerSize >= 8, p + size <= data.count else { break }

            switch type {
            case 0x0001:                                // string pool
                pool = StringPool(r, at: p)
            case 0x0180:                                // attribute name -> id
                var q = p + headerSize
                while q + 4 <= p + size { resourceIds.append(r.u32(q)); q += 4 }
            case 0x0102:                                // start element
                guard let pool, let name = pool.string(Int(r.u32(p + 20))) else { break }
                let attrStart = p + 16 + Int(r.u16(p + 24))
                let attrSize = Int(r.u16(p + 26))
                let count = Int(r.u16(p + 28))
                guard attrSize >= 20 else { break }

                var attrs: [Attr] = []
                for i in 0..<count {
                    let a = attrStart + i * attrSize
                    guard a + 20 <= p + size else { break }
                    let nameIndex = Int(r.u32(a + 4))
                    let id = nameIndex < resourceIds.count ? resourceIds[nameIndex] : 0
                    let rawIndex = Int(r.u32(a + 8))
                    let typed = r.u32(a + 16)
                    attrs.append(Attr(name: pool.string(nameIndex) ?? "",
                                      id: id,
                                      dataType: r.u8(a + 15),
                                      data: typed,
                                      value: pool.string(rawIndex)
                                             ?? pool.string(Int(typed))))
                }
                out.append(Element(name: name, attributes: attrs))
            default:
                break
            }
            p += size
        }
        return out
    }

    /// The `<application>` element's attributes, keyed by their framework id.
    private static func applicationAttributes(_ data: Data) -> [UInt32: Attribute]? {
        guard let app = elements(data).first(where: { $0.name == "application" })
        else { return nil }

        var found: [UInt32: Attribute] = [:]
        let wanted: Set<UInt32> = [attrLabel, attrIcon, attrRoundIcon]
        for attr in app.attributes where wanted.contains(attr.id) {
            if attr.dataType == 0x01 {
                found[attr.id] = .reference(attr.data)
            } else if attr.dataType == 0x03, let text = attr.value {
                found[attr.id] = .literal(text)
            }
        }
        return found
    }

    /// Resolve a resource id the caller found inside another resource — an
    /// adaptive icon's foreground layer, in practice.
    static func bitmap(for id: UInt32, resources: Data) -> String? {
        ResourceTable(resources)?.bitmap(for: id)
    }
}

// MARK: - resources.arsc

/// Just enough of the resource table to resolve one id at a time.
private final class ResourceTable {
    private let r: Reader
    private let values: StringPool
    /// Every type chunk in the file: which package and type it belongs to, and
    /// the density and locale of the configuration it describes.
    private var chunks: [(packageId: Int, typeId: Int, density: Int, locale: UInt32,
                          offset: Int)] = []

    init?(_ data: Data) {
        let r = Reader(data)
        guard r.u16(0) == 0x0002 else { return nil }    // RES_TABLE_TYPE
        self.r = r

        let headerSize = Int(r.u16(2))
        var p = headerSize
        guard p + 8 <= data.count, r.u16(p) == 0x0001,
              let pool = StringPool(r, at: p) else { return nil }
        values = pool
        p += Int(r.u32(p + 4))

        // Packages, then the type chunks inside each one.
        while p + 8 <= data.count {
            let type = r.u16(p)
            let size = Int(r.u32(p + 4))
            guard size >= 8, p + size <= data.count else { break }
            if type == 0x0200 { scanPackage(at: p, size: size) }
            p += size
        }
        if chunks.isEmpty { return nil }
    }

    private func scanPackage(at start: Int, size: Int) {
        let headerSize = Int(r.u16(start + 2))
        let packageId = Int(r.u32(start + 8))
        var p = start + headerSize

        while p + 8 <= start + size {
            let type = r.u16(p)
            let chunkSize = Int(r.u32(p + 4))
            guard chunkSize >= 8, p + chunkSize <= start + size else { break }

            if type == 0x0201 {                          // RES_TABLE_TYPE_TYPE
                let typeId = Int(r.u8(p + 8))
                // ResTable_config begins at +20; its first word is its own
                // size, the locale is at +8 and the density at +14.
                let locale = r.u32(p + 20 + 8)
                let density = Int(r.u16(p + 20 + 14))
                chunks.append((packageId, typeId, density, locale, p))
            } else if type == 0x0202 || type == 0x0203 {
                // Type spec and library chunks carry nothing this needs.
            }
            p += chunkSize
        }
    }

    /// The string a resource id resolves to, preferring the default locale.
    func string(for id: UInt32) -> String? {
        let found = entries(for: id).filter { $0.dataType == 0x03 }
        guard !found.isEmpty else { return nil }
        let best = found.first { $0.locale == 0 } ?? found[0]
        return values.string(Int(best.data))
    }

    /// The best bitmap a resource id resolves to.
    ///
    /// Densities, not names: the same id exists once per screen density, and
    /// the highest is the one that still looks right scaled down. XML entries
    /// are skipped — an adaptive icon is a description of how to compose two
    /// other drawables, and rendering it is Android's job, not ours. Every app
    /// that ships one also ships the flat bitmap that older launchers use.
    func bitmap(for id: UInt32) -> String? {
        var best: (density: Int, path: String)?
        for entry in entries(for: id) where entry.dataType == 0x03 {
            guard let path = values.string(Int(entry.data)),
                  path.hasPrefix("res/") || path.hasPrefix("assets/") else { continue }
            let lower = path.lowercased()
            guard lower.hasSuffix(".png") || lower.hasSuffix(".webp")
                    || lower.hasSuffix(".jpg") || lower.hasSuffix(".jpeg") else { continue }
            // ANY (0xFFFE) is what an adaptive icon's XML is filed under; when a
            // bitmap lands there it is a fallback, so rank it below real ones.
            let density = entry.density == 0xFFFE ? 1 : entry.density
            if best == nil || density > best!.density { best = (density, path) }
        }
        if let best { return best.path }

        // No bitmap at any density: an adaptive icon, then. Hand back the XML
        // so the caller can follow it to the layer underneath.
        for entry in entries(for: id) where entry.dataType == 0x03 {
            if let path = values.string(Int(entry.data)), path.hasSuffix(".xml") {
                return path
            }
        }
        return nil
    }


    private struct Entry {
        let dataType: UInt8
        let data: UInt32
        let density: Int
        let locale: UInt32
    }

    /// Every configuration's value for one resource id.
    private func entries(for id: UInt32) -> [Entry] {
        let packageId = Int((id >> 24) & 0xFF)
        let typeId = Int((id >> 16) & 0xFF)
        let index = Int(id & 0xFFFF)
        var out: [Entry] = []

        for chunk in chunks where chunk.packageId == packageId && chunk.typeId == typeId {
            let p = chunk.offset
            let headerSize = Int(r.u16(p + 2))
            let chunkSize = Int(r.u32(p + 4))
            let flags = r.u8(p + 9)
            let entryCount = Int(r.u32(p + 12))
            let entriesStart = Int(r.u32(p + 16))
            let sparse = flags & 0x01 != 0
            let offset16 = flags & 0x02 != 0

            var entryOffset: Int?
            if sparse {
                // A sorted (index, offset/4) pair per present entry.
                var q = p + headerSize
                for _ in 0..<entryCount {
                    guard q + 4 <= p + chunkSize else { break }
                    if Int(r.u16(q)) == index { entryOffset = Int(r.u16(q + 2)) * 4; break }
                    q += 4
                }
            } else if index < entryCount {
                if offset16 {
                    let q = p + headerSize + index * 2
                    let raw = r.u16(q)
                    if raw != 0xFFFF { entryOffset = Int(raw) * 4 }
                } else {
                    let q = p + headerSize + index * 4
                    let raw = r.u32(q)
                    if raw != 0xFFFF_FFFF { entryOffset = Int(raw) }
                }
            }

            guard let offset = entryOffset else { continue }
            let e = p + entriesStart + offset
            guard e + 8 <= p + chunkSize else { continue }

            let entryFlags = r.u16(e + 2)
            if entryFlags & 0x0008 != 0 {
                // Compact entry (Android 14 and later): the key takes the first
                // half-word, the value's type rides in the top byte of the
                // flags, and the data follows immediately.
                out.append(Entry(dataType: UInt8((entryFlags >> 8) & 0xFF),
                                 data: r.u32(e + 4),
                                 density: chunk.density, locale: chunk.locale))
                continue
            }
            // A complex entry is a style or an array, not a value.
            guard entryFlags & 0x0001 == 0 else { continue }

            let valueAt = e + Int(r.u16(e))
            guard valueAt + 8 <= p + chunkSize else { continue }
            out.append(Entry(dataType: r.u8(valueAt + 3),
                             data: r.u32(valueAt + 4),
                             density: chunk.density, locale: chunk.locale))
        }
        return out
    }
}

// MARK: - the two primitives both formats are built from

/// Bounds-checked little-endian reads. Every accessor returns zero rather than
/// trapping: this parses files built by other people's tools, and a truncated
/// APK should produce no answer, not a crash.
private struct Reader {
    let d: [UInt8]

    init(_ data: Data) { d = [UInt8](data) }

    var count: Int { d.count }

    func u8(_ i: Int) -> UInt8 { i >= 0 && i < d.count ? d[i] : 0 }

    func u16(_ i: Int) -> UInt16 {
        guard i >= 0, i + 1 < d.count else { return 0 }
        return UInt16(d[i]) | UInt16(d[i + 1]) << 8
    }

    func u32(_ i: Int) -> UInt32 {
        guard i >= 0, i + 3 < d.count else { return 0 }
        return UInt32(d[i]) | UInt32(d[i + 1]) << 8
             | UInt32(d[i + 2]) << 16 | UInt32(d[i + 3]) << 24
    }
}

/// A resource string pool: a count, an offset per string, and the bytes.
private struct StringPool {
    private let strings: [String?]

    func string(_ i: Int) -> String? {
        i >= 0 && i < strings.count ? strings[i] : nil
    }

    init?(_ r: Reader, at chunk: Int) {
        guard r.u16(chunk) == 0x0001 else { return nil }
        let headerSize = Int(r.u16(chunk + 2))
        let chunkSize = Int(r.u32(chunk + 4))
        let count = Int(r.u32(chunk + 8))
        let flags = r.u32(chunk + 16)
        let dataStart = chunk + Int(r.u32(chunk + 20))
        let utf8 = flags & 0x0100 != 0
        guard count > 0, count < 500_000, chunk + chunkSize <= r.count else { return nil }

        var out = [String?](repeating: nil, count: count)
        for i in 0..<count {
            let offset = Int(r.u32(chunk + headerSize + i * 4))
            let at = dataStart + offset
            guard at >= 0, at < chunk + chunkSize else { continue }
            out[i] = utf8 ? StringPool.utf8(r, at, limit: chunk + chunkSize)
                          : StringPool.utf16(r, at, limit: chunk + chunkSize)
        }
        strings = out
    }

    /// Two varints — the length in UTF-16 units, then the length in bytes —
    /// each one or two bytes depending on its high bit.
    private static func utf8(_ r: Reader, _ at: Int, limit: Int) -> String? {
        var p = at
        func varint() -> Int {
            let b = Int(r.u8(p)); p += 1
            if b & 0x80 == 0 { return b }
            let b2 = Int(r.u8(p)); p += 1
            return ((b & 0x7F) << 8) | b2
        }
        _ = varint()
        let bytes = varint()
        guard bytes >= 0, p + bytes <= limit else { return nil }
        return String(bytes: r.d[p..<(p + bytes)], encoding: .utf8)
    }

    private static func utf16(_ r: Reader, _ at: Int, limit: Int) -> String? {
        var p = at
        var length = Int(r.u16(p)); p += 2
        if length & 0x8000 != 0 {
            length = ((length & 0x7FFF) << 16) | Int(r.u16(p)); p += 2
        }
        guard length >= 0, p + length * 2 <= limit else { return nil }
        var units: [UInt16] = []
        units.reserveCapacity(length)
        for i in 0..<length { units.append(r.u16(p + i * 2)) }
        return String(decoding: units, as: UTF16.self)
    }
}
