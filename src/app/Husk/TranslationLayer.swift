// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit
import UniformTypeIdentifiers

/// Android apps without booting Android.
///
/// Husk runs an APK in one of two ways. It can boot a whole Android system under QEMU and show one app's surface:
/// that runs anything, and costs a kernel, an init, a system server and minutes of emulated CPU before the first frame.
/// Or -- this -- it can keep only the app's own code, its Dex and its native libraries, and run it against a rewrite of the
/// Android framework on the phone itself, so there is nothing to boot. Games built on Unity, cocos2d-x, Minecraft's
/// GameActivity and SDL3 start in seconds this way. docs/04-translation-layer.md is the design.
enum TranslationLayer {
    /// Whether the technical detail is shown: library reports, device checks, logs. Off, the screens carry
    /// only what is needed to add an app and run it. Set in Settings > About.
    static let devInfoKey = "husk.devInfo"

    /// One folder per app. Kept apart from Android's apps: those live on the
    /// guest's disk, and this runtime has no guest.
    static var root: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("TranslationLayer", isDirectory: true)
    }

    /// What games see as Android's shared storage (/sdcard, outside their own Android/data). In Documents, so it shows in Files and
    /// Finder: a game that wants its data in a folder of its own there (a PC port's game files, say) is given it by copying it in.
    static var sharedStorage: URL {
        let url = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("Shared Storage", isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }
}

// MARK: - What the C side reports

/// One app, as src/translation-layer/husk-tl-scan.c sees it.
struct TLReport: Decodable {
    let ok: Bool
    let error: String?
    let verdict: String
    let summary: String
    let engine: String?
    let hasManifest: Bool
    let dexCount: Int
    let dexBytes: Int64
    let abis: [String]
    let systemLibraries: [String]
    let libraries: [TLLibrary]
}

extension TLReport {
    /// The engine the native runtime drives this app with, if it can: Unity (IL2CPP) games and cocos2d-x games.
    var nativeEngine: TLNativeEngine? {
        guard ok, !abis.isEmpty, abis.contains("arm64-v8a") else { return nil }
        if engine?.hasPrefix("Unity") == true { return .unity }
        if engine == "Cocos" { return .cocos }
        if engine == "Minecraft" { return .minecraft }
        if engine == "SDL" { return .sdl }
        if engine == "Unreal Engine" { return .ue4 }
        if engine == "Rockstar" { return .gta }
        if engine == "Godot" { return .godot }
        if engine == "NativeActivity" { return .nativeactivity }
        if engine == "Flutter" { return .flutter }
        return nil
    }

    /// Games from those engines run through the native runtime, which handles what the scan flags per library
    /// (raw system calls, thread-register use, pages shared between segments). A report made before that
    /// runtime existed still says "needs work", so the app judges by the engine, not by the stored words.
    var runsOnNativeRuntime: Bool { nativeEngine != nil }

    /// "Unity" or "Cocos2d-x", for words on screen.
    var nativeEngineName: String { nativeEngine == .cocos ? "Cocos2d-x" : nativeEngine == .minecraft ? "Minecraft" : nativeEngine == .sdl ? "SDL" : nativeEngine == .ue4 ? "Unreal Engine" : nativeEngine == .gta ? "Rockstar" : nativeEngine == .godot ? "Godot" : nativeEngine == .nativeactivity ? "NativeActivity" : nativeEngine == .flutter ? "Flutter" : "Unity" }

    var displaySummary: String {
        guard runsOnNativeRuntime else { return summary }
        let flagged = libraries.filter { $0.abi == "arm64-v8a" && $0.status != "ok" }.count
        let total = libraries.filter { $0.abi == "arm64-v8a" }.count
        var text = nativeEngine == .flutter
            ? "A Flutter app. Its engine and its compiled Dart run natively through Husk's translation layer, which loads its \(total) arm64 libraries itself."
            : "A \(nativeEngineName) game. It runs through Husk's native runtime, which loads its \(total) arm64 libraries itself."
        if nativeEngine == .cocos || nativeEngine == .minecraft || nativeEngine == .ue4 || nativeEngine == .gta { text += " It is a landscape game: Husk turns the screen for it." }
        else if nativeEngine == .sdl || nativeEngine == .nativeactivity || nativeEngine == .godot { text += " Husk turns the screen the way the game asks for." }
        if flagged > 0 {
            text += " \(flagged) of them use tricks the older loader could not handle; the native runtime handles those too, "
                  + "except for optional anti-tamper code, which it leaves out."
        }
        return text
    }
}

/// One arm64 library. Everything past `notes` is absent when the file could
/// not be read as an arm64 library at all.
struct TLLibrary: Decodable, Identifiable {
    let name: String
    let abi: String
    let apk: String
    let bytes: Int64
    let compressed: Bool
    let status: String
    let notes: [String]
    let maxAlign: Int?
    let pages: Int?
    let execPages: Int?
    let writePages: Int?
    let conflictPages: Int?
    let layout: String?
    let svc: Int?
    let tpidrReads: Int?
    let tpidrWrites: Int?
    let tls: Bool?
    let textrel: Bool?
    let relocations: Int?
    let tlsRelocations: Int?
    let unsupportedRelocations: Int?
    let packing: String?
    let imports: Int?
    let soname: String?
    let needed: [String]?

    var id: String { "\(apk)/\(abi)/\(name)" }
}

/// One answer from husk-tl-probe.c.
struct TLCheck: Decodable, Identifiable {
    let id: String
    let title: String
    let status: String
    let detail: String
}

struct TLApp: Identifiable {
    /// The folder's name.
    let id: String
    var label: String
    var iconPath: String?
    var apks: [String]
    var report: TLReport?
    /// When it was last started from Husk.
    var lastPlayed: Date? = nil
    /// Its Android package name, once read from the APK.
    var packageName: String? = nil

    /// Whether its manifest files it as a game (android:appCategory="game" or isGame): read for Flutter apps, which are apps
    /// unless they say otherwise.
    var isGameCategory: Bool { apks.first.map { husk_apk_is_game($0) != 0 } ?? false }
}

// MARK: - Store

/// The apps added for the translation layer, and the phone's answers.
@MainActor
final class TranslationLayerStore: ObservableObject {
    static let shared = TranslationLayerStore()

    @Published private(set) var apps: [TLApp] = []
    /// What is being done right now, in words.
    @Published private(set) var busy: String?
    @Published private(set) var checks: [TLCheck] = []
    @Published private(set) var checking = false
    @Published var lastError: String?

    private init() { reload() }

    /// Which reading of an app's libraries its report came from. A newer Husk that recognises more (an engine, a kind of game) reads
    /// the apps it already holds again, so they are not left with what the old one knew.
    nonisolated static let scanVersion = 4
    private var rescanning = false

    func reload() {
        let dirs = (try? FileManager.default.contentsOfDirectory(
            at: TranslationLayer.root, includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])) ?? []
        apps = dirs.compactMap(Self.load).sorted {
            $0.label.localizedCaseInsensitiveCompare($1.label) == .orderedAscending
        }
        rescanStale()
    }

    /// The apps read again from disk, and nothing else started: after work that only changed what is on disk.
    private func reloadQuietly() {
        let dirs = (try? FileManager.default.contentsOfDirectory(
            at: TranslationLayer.root, includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])) ?? []
        apps = dirs.compactMap(Self.load).sorted {
            $0.label.localizedCaseInsensitiveCompare($1.label) == .orderedAscending
        }
    }

    /// Icons are drawn again once per run of Husk at most: a newly added app already has the current kind.
    private var iconsRefreshed = false

    nonisolated private static func scanStamp(_ id: String) -> Int {
        let url = TranslationLayer.root.appendingPathComponent(id, isDirectory: true).appendingPathComponent("scan-version.txt")
        return (try? String(contentsOf: url, encoding: .utf8)).flatMap { Int($0.trimmingCharacters(in: .whitespacesAndNewlines)) } ?? 0
    }

    private func rescanStale() {
        guard !rescanning else { return }
        let stale = apps.filter { Self.scanStamp($0.id) != Self.scanVersion }
        let icons = iconsRefreshed ? [] : apps
        iconsRefreshed = true
        if !icons.isEmpty { Task.detached(priority: .utility) {
            var changed = false
            for app in icons {
                let before = app.iconPath.flatMap { try? FileManager.default.attributesOfItem(atPath: $0)[.modificationDate] as? Date }
                Self.refreshIcon(app)
                let path = TranslationLayer.root.appendingPathComponent(app.id).appendingPathComponent("icon.png").path
                let after = try? FileManager.default.attributesOfItem(atPath: path)[.modificationDate] as? Date
                if before != after { changed = true }
            }
            if changed { await MainActor.run { self.reloadQuietly() } }
        } }
        guard !stale.isEmpty else { return }
        rescanning = true
        HuskLog.log("tl", "reading \(stale.count) app(s) again with the newer scanner")
        Task.detached(priority: .utility) {
            for app in stale {
                let dir = TranslationLayer.root.appendingPathComponent(app.id, isDirectory: true)
                try? Data(Self.scan(app.apks).utf8).write(to: dir.appendingPathComponent("report.json"))
                try? "\(Self.scanVersion)".write(to: dir.appendingPathComponent("scan-version.txt"), atomically: true, encoding: .utf8)
            }
            await MainActor.run {
                self.rescanning = false
                self.reload()
            }
        }
    }

    /// Remember that a game was just started, for Home.
    func markPlayed(_ app: TLApp) {
        let dir = TranslationLayer.root.appendingPathComponent(app.id, isDirectory: true)
        try? "\(Date().timeIntervalSince1970)".write(to: dir.appendingPathComponent("last-played.txt"), atomically: true, encoding: .utf8)
        reloadQuietly()
    }

    /// Call the app something else. The name Android gave it is only where it started.
    func rename(_ app: TLApp, to name: String) {
        let dir = TranslationLayer.root.appendingPathComponent(app.id, isDirectory: true)
        try? name.write(to: dir.appendingPathComponent("label.txt"), atomically: true, encoding: .utf8)
        HuskLog.log("tl", "renamed \(app.label) to \(name)")
        reload()
    }

    /// Keep a copy of one app -- one APK, or a base APK and its splits,
    /// picked together -- and report on it.
    func add(_ urls: [URL], move: Bool = false) {
        guard !urls.isEmpty, busy == nil else { return }
        busy = urls.count == 1 ? "Adding \(urls[0].lastPathComponent)…"
                               : "Adding \(urls.count) APKs…"
        Task.detached(priority: .userInitiated) {
            let failure = Self.ingest(urls, move: move)
            await MainActor.run {
                self.busy = nil
                self.lastError = failure
                self.reload()
                if failure == nil, move { self.adoptDroppedAPKs() }      // the next one waiting, if several were put there
            }
        }
    }

    /// APKs put straight into Husk's folder (Files > On My iPhone > Husk, which the app shares) are taken in as apps, one each, and
    /// moved rather than copied, so a big one costs no second copy's worth of storage. This is the way in that needs no file picker.
    func adoptDroppedAPKs() {
        guard busy == nil, let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first else { return }
        let dropped = ((try? FileManager.default.contentsOfDirectory(at: docs, includingPropertiesForKeys: nil,
                                                                      options: [.skipsHiddenFiles])) ?? [])
            .filter { $0.pathExtension.lowercased() == "apk" || BundleUnpacker.extensions.contains($0.pathExtension.lowercased()) }
        guard let first = dropped.first else { return }
        HuskLog.log("tl", "adopting \(dropped.count) APK(s) found in the Husk folder")
        add([first], move: true)
    }

    func remove(_ app: TLApp) {
        let dir = TranslationLayer.root.appendingPathComponent(app.id, isDirectory: true)
        try? FileManager.default.removeItem(at: dir)
        HuskLog.log("tl", "removed \(app.label)")
        reload()
    }

    func runChecks() {
        guard !checking else { return }
        checking = true
        Task.detached(priority: .userInitiated) {
            // Generated code only runs where MAP_JIT is already known to
            // execute -- the same measurement Settings > JIT shows.
            let mayExecute = JITBootstrap.mapJITWorks
            HuskLog.log("tl", "running device checks (may execute: \(mayExecute))")
            let raw = husk_tl_run_checks(mayExecute)
            let json = raw.map { String(cString: $0) } ?? "[]"
            husk_tl_free(raw)
            let parsed = (try? JSONDecoder().decode([TLCheck].self, from: Data(json.utf8))) ?? []
            HuskLog.log("tl", "device checks: \(json)")
            await MainActor.run {
                self.checks = parsed
                self.checking = false
            }
        }
    }

    // MARK: files

    nonisolated private static func load(_ dir: URL) -> TLApp? {
        let fm = FileManager.default
        guard let files = try? fm.contentsOfDirectory(atPath: dir.path) else { return nil }
        let names = files.filter { $0.lowercased().hasSuffix(".apk") }
        let apks = names
            .sorted { a, b in
                // The base first: it is the one a game is started from, and the others are its splits and packs.
                let ra = BundleUnpacker.rank(a, siblings: names), rb = BundleUnpacker.rank(b, siblings: names)
                return ra != rb ? ra < rb : a < b
            }
            .map { dir.appendingPathComponent($0).path }
        guard let first = apks.first else { return nil }

        let saved = try? String(contentsOf: dir.appendingPathComponent("label.txt"),
                                encoding: .utf8)
        let label = saved?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let icon = dir.appendingPathComponent("icon.png").path
        let report = (try? Data(contentsOf: dir.appendingPathComponent("report.json")))
            .flatMap { try? JSONDecoder().decode(TLReport.self, from: $0) }
        let played = (try? String(contentsOf: dir.appendingPathComponent("last-played.txt"), encoding: .utf8))
            .flatMap { TimeInterval($0.trimmingCharacters(in: .whitespacesAndNewlines)) }
            .map { Date(timeIntervalSince1970: $0) }
        return TLApp(id: dir.lastPathComponent,
                     label: label.isEmpty ? (first as NSString).lastPathComponent : label,
                     iconPath: fm.fileExists(atPath: icon) ? icon : nil,
                     apks: apks, report: report, lastPlayed: played,
                     packageName: (try? String(contentsOf: dir.appendingPathComponent("package.txt"), encoding: .utf8))?
                        .trimmingCharacters(in: .whitespacesAndNewlines))
    }

    /// Copy, name, scan. Returns what went wrong, if anything did.
    nonisolated private static func ingest(_ urls: [URL], move: Bool = false) -> String? {
        let fm = FileManager.default
        let dir = TranslationLayer.root.appendingPathComponent(UUID().uuidString,
                                                               isDirectory: true)
        do {
            try fm.createDirectory(at: dir, withIntermediateDirectories: true)
            for url in urls {
                // Security-scoped: the picker's URL is only readable inside this pair.
                let scoped = url.startAccessingSecurityScopedResource()
                defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                // A bundle (.xapk, .apkm, .apks) is a zip of an app's APKs: take them out, and do not keep the bundle itself.
                if BundleUnpacker.extensions.contains(url.pathExtension.lowercased()) {
                    let names = try BundleUnpacker.unpack(url, into: dir)
                    HuskLog.log("tl", "unpacked \(url.lastPathComponent): \(names.joined(separator: ", "))")
                    if move { try? fm.removeItem(at: url) }
                    continue
                }
                var name = url.lastPathComponent
                if !name.lowercased().hasSuffix(".apk") { name += ".apk" }
                let dest = dir.appendingPathComponent(name)
                if move {
                    try fm.moveItem(at: url, to: dest)
                } else {
                    // Coordinated, so a file that lives in iCloud and is not on the phone yet is downloaded first
                    // (a plain copy of it fails, or finds nothing).
                    var coordinationError: NSError?
                    var copyError: Error?
                    NSFileCoordinator().coordinate(readingItemAt: url, options: [], error: &coordinationError) { readable in
                        do { try fm.copyItem(at: readable, to: dest) } catch { copyError = error }
                    }
                    if let failure = coordinationError ?? copyError { throw failure }
                }
            }
        } catch {
            try? fm.removeItem(at: dir)
            HuskLog.log("tl", "FAILED to add: \(error.localizedDescription)")
            return "Husk could not copy it: \(error.localizedDescription)"
        }

        let apks = ((try? fm.contentsOfDirectory(atPath: dir.path)) ?? [])
            .filter { $0.hasSuffix(".apk") }.sorted()
            .map { dir.appendingPathComponent($0).path }
        describe(apks, into: dir)
        let json = scan(apks)
        try? Data(json.utf8).write(to: dir.appendingPathComponent("report.json"))
        try? "\(scanVersion)".write(to: dir.appendingPathComponent("scan-version.txt"), atomically: true, encoding: .utf8)
        HuskLog.log("tl", "report for \(dir.lastPathComponent): \(json)")
        return nil
    }

    /// The app's own name and icon, from its manifest and resource table --
    /// the same reading the library does for apps inside Android, done here on
    /// the file directly.
    /// Which way of drawing icons an app's icon.png came from. A newer Husk that draws them better draws the apps it
    /// already holds again (icons only: the name may be the user's own).
    nonisolated static let iconVersion = 3

    nonisolated private static func describe(_ apks: [String], into dir: URL, label wantLabel: Bool = true) {
        // The base APK carries the label and the icon; a config split's
        // manifest names neither. The base is usually called base.apk, and
        // otherwise usually the biggest.
        func size(_ path: String) -> Int {
            ((try? FileManager.default.attributesOfItem(atPath: path)[.size]) as? NSNumber)?
                .intValue ?? 0
        }
        let ordered = apks.sorted { a, b in
            let aBase = (a as NSString).lastPathComponent == "base.apk"
            let bBase = (b as NSString).lastPathComponent == "base.apk"
            return aBase != bBase ? aBase : size(a) > size(b)
        }
        for apk in ordered {
            guard let manifest = entry(apk, "AndroidManifest.xml", limit: 8 << 20),
                  let arsc = entry(apk, "resources.arsc", limit: 64 << 20) else { continue }
            let info = ApkMetadata.read(manifest: manifest, resources: arsc)
            guard info.label != nil || info.iconEntry != nil else { continue }

            if let package = ApkMetadata.packageName(manifest) {
                try? package.write(to: dir.appendingPathComponent("package.txt"), atomically: true, encoding: .utf8)
            }
            if wantLabel, let label = info.label {
                try? label.write(to: dir.appendingPathComponent("label.txt"),
                                 atomically: true, encoding: .utf8)
            }
            // Stored as PNG whatever it was, so the icon view needs no WebP.
            var drawn = "?"
            if let iconEntry = info.iconEntry {
                if iconEntry.hasSuffix(".xml") {
                    if let image = adaptiveIcon(apk, xml: iconEntry, resources: arsc), let png = image.pngData() {
                        try? png.write(to: dir.appendingPathComponent("icon.png"))
                        drawn = "\(iconEntry) (adaptive, composed)"
                    }
                } else if let data = entry(apk, iconEntry, limit: 16 << 20), let png = UIImage(data: data)?.pngData() {
                    try? png.write(to: dir.appendingPathComponent("icon.png"))
                    drawn = iconEntry
                }
            }
            try? "\(iconVersion)".write(to: dir.appendingPathComponent("icon-version.txt"), atomically: true, encoding: .utf8)
            HuskLog.log("tl", "\((apk as NSString).lastPathComponent): "
                            + "label=\(info.label ?? "?") icon=\(drawn)")
            return
        }
    }

    /// An adaptive icon, drawn the way a launcher draws it: the background layer (a picture or a colour) under the
    /// foreground, both on Android's 108-unit canvas, cut to the 72 units in the middle that a launcher shows. The
    /// foreground alone -- what this used to keep -- is the app's mark floating on nothing.
    nonisolated private static func adaptiveIcon(_ apk: String, xml: String, resources: Data) -> UIImage? {
        guard let data = entry(apk, xml, limit: 4 << 20) else { return nil }
        let layers = ApkMetadata.adaptiveLayers(data)
        func picture(_ id: UInt32?) -> UIImage? {
            guard let id, let path = ApkMetadata.bitmap(for: id, resources: resources), !path.hasSuffix(".xml"),
                  let bytes = entry(apk, path, limit: 16 << 20) else { return nil }
            return UIImage(data: bytes)
        }
        let fore = picture(layers.foreground)
        let back = picture(layers.background)
        var color = layers.backgroundColor
        if color == nil, back == nil, let id = layers.background { color = ApkMetadata.color(for: id, resources: resources) }
        guard fore != nil || back != nil else { return nil }

        let side: CGFloat = 432                                       // 108 units at 4 px each
        let visible = side * 72 / 108
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        format.opaque = back != nil || color.map { $0 >> 24 == 0xFF } == true
        let renderer = UIGraphicsImageRenderer(size: CGSize(width: visible, height: visible), format: format)
        return renderer.image { ctx in
            let canvas = CGRect(x: -(side - visible) / 2, y: -(side - visible) / 2, width: side, height: side)
            if let color {
                UIColor(red: CGFloat((color >> 16) & 0xFF) / 255, green: CGFloat((color >> 8) & 0xFF) / 255,
                        blue: CGFloat(color & 0xFF) / 255, alpha: CGFloat(color >> 24) / 255).setFill()
                ctx.fill(CGRect(x: 0, y: 0, width: visible, height: visible))
            } else if back == nil {
                // A foreground with nothing said about what is behind it: the white a launcher would use.
                UIColor.white.setFill()
                ctx.fill(CGRect(x: 0, y: 0, width: visible, height: visible))
            }
            back?.draw(in: canvas)
            fore?.draw(in: canvas)
        }
    }

    /// Draw the icons of apps added by an older Husk again, the current way.
    nonisolated static func refreshIcon(_ app: TLApp) {
        let dir = TranslationLayer.root.appendingPathComponent(app.id, isDirectory: true)
        let stamp = (try? String(contentsOf: dir.appendingPathComponent("icon-version.txt"), encoding: .utf8))
            .flatMap { Int($0.trimmingCharacters(in: .whitespacesAndNewlines)) } ?? 0
        guard stamp != iconVersion else { return }
        describe(app.apks, into: dir, label: false)
    }

    nonisolated static func entry(_ apk: String, _ name: String, limit: Int) -> Data? {
        var length = 0
        guard let bytes = husk_tl_read_entry(apk, name, limit, &length) else { return nil }
        defer { husk_tl_free(bytes) }
        return Data(bytes: bytes, count: length)
    }

    nonisolated static func scan(_ paths: [String]) -> String {
        let owned = paths.map { strdup($0) }
        defer { owned.forEach { free($0) } }
        let pointers = owned.map { UnsafePointer<CChar>($0) }
        let raw = pointers.withUnsafeBufferPointer {
            husk_tl_scan($0.baseAddress, Int32($0.count))
        }
        defer { husk_tl_free(raw) }
        return raw.map { String(cString: $0) } ?? "{}"
    }
}

// MARK: - Settings

/// What the phone answers about running Android's native code, and where the translation layer stands: for
/// developers, from Settings when developer information is on.
struct TLChecksView: View {
    @ObservedObject private var store = TranslationLayerStore.shared

    var body: some View {
        Form {
            checksSection
            progressSection
        }
        .huskForm()
        .navigationTitle("Device Checks")
    }

    private var checksSection: some View {
        Section {
            ForEach(store.checks) { check in
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Text(check.title).foregroundStyle(Theme.text)
                        Spacer(minLength: 8)
                        TLCheckTag(status: check.status)
                    }
                    Text(check.detail)
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.textDim)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding(.vertical, 3)
            }
            Button {
                store.runChecks()
            } label: {
                Label(store.checking ? "Checking…"
                      : store.checks.isEmpty ? "Run checks" : "Run again",
                      systemImage: "stethoscope")
            }
            .disabled(store.checking)
        } header: {
            Text("This iPhone")
        } footer: {
            Text("Android's native code expects things of the processor and of memory "
               + "that only the phone can answer -- above all, whether a library's code "
               + "can run with its data writable right beside it. Enable JIT first, or "
               + "the memory checks are skipped. The results go to the console too.")
        }
    }

    private var progressSection: some View {
        Section {
            DetailRow(label: "App reports", value: "working", mono: false)
            DetailRow(label: "Device checks", value: "working", mono: false)
            DetailRow(label: "Library loader", value: "working", mono: false)
            DetailRow(label: "Android runtime", value: "Unity, cocos2d-x, GameActivity, SDL3", mono: false)
            DetailRow(label: "Opening apps", value: "Unity, cocos2d-x, Minecraft and SDL3 games", mono: false)
        } header: {
            Text("Where it stands")
        } footer: {
            Text("The plan, in order, is docs/04-translation-layer.md in Husk's source.")
        }
    }
}



private struct TLCheckTag: View {
    let status: String

    var body: some View {
        switch status {
        case "pass":    Tag(text: "Pass", tint: Theme.good)
        case "fail":    Tag(text: "Fail", tint: .red)
        case "warn":    Tag(text: "Partly", tint: .orange)
        case "skip":    Tag(text: "Skipped")
        default:        Tag(text: "Info", tint: Theme.accent)
        }
    }
}

// MARK: - One app's report



// MARK: - Live Attempt Execution & Screen

@MainActor
final class TLAttemptRunner: ObservableObject {
    @Published var isRunning = false
    @Published var isDone = false
    @Published var exitCode: Int? = nil
    @Published var frameCount = 0
    @Published var logText: String = ""

    private var timer: Timer?

    var statusText: String {
        if isRunning { return "Running Attempt..." }
        guard let code = exitCode else { return "Ready" }
        switch code {
        case 2:  return "Success: The guest drew frames!"
        case 1:  return "Loaded: Native libraries loaded (no draw)"
        default: return "Refused: Could not execute"
        }
    }

    var statusColor: Color {
        if isRunning { return Theme.accent }
        guard let code = exitCode else { return Theme.textDim }
        switch code {
        case 2:  return Theme.good
        case 1:  return .orange
        default: return .red
        }
    }

    var subStatusText: String {
        if isRunning {
            return "\(frameCount) frame(s) posted · driving lifecycle"
        }
        if let code = exitCode {
            return "Exit code \(code) · \(frameCount) frame(s) drawn"
        }
        return "Not started"
    }

    private var pollTicks: Int = 0

    /// `seconds` of 0 is no limit: the run goes on until it is stopped. It used to stop itself after two minutes.
    func start(apks: [String], seconds: Int = 0) {
        guard !isRunning else { return }
        isRunning = true
        isDone = false
        exitCode = nil
        frameCount = 0
        logText = ""
        pollTicks = 0

        DispatchQueue.global(qos: .userInitiated).async {
            let owned = apks.map { strdup($0) }
            defer { owned.forEach { free($0) } }
            let pointers = owned.map { UnsafePointer<CChar>($0) }
            let rc = pointers.withUnsafeBufferPointer {
                husk_tl_attempt_start($0.baseAddress, Int32($0.count), Int32(seconds))
            }
            if rc != 0 {
                Task { @MainActor in
                    self.isRunning = false
                    self.isDone = true
                    self.exitCode = -1
                    self.poll()
                }
                return
            }

            Task { @MainActor in
                // Ten times a second is plenty for a status line and a log. The
                // screen no longer comes through here: it presents itself from a
                // display link, so this timer is no longer on the frame's path.
                self.timer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
                    self?.poll()
                }
            }
        }
    }

    func stop() {
        husk_tl_attempt_stop()
        poll()
    }

    private func poll() {
        pollTicks += 1
        if pollTicks % 5 == 0 {
            updateLog()
        }
        let n = Int(husk_tl_attempt_frames())
        if n != frameCount { frameCount = n }

        var code: Int32 = 0
        if husk_tl_attempt_done(&code) {
            timer?.invalidate()
            timer = nil
            isRunning = false
            isDone = true
            exitCode = Int(code)
            updateLog()
            frameCount = Int(husk_tl_attempt_frames())
        }
    }

    private func updateLog() {
        if let cLog = husk_tl_attempt_log() {
            let text = String(cString: cLog)
            free(cLog)
            if text != self.logText {
                self.logText = text
            }
        }
    }
}

/// Unity and cocos2d-x games run through the native runtime (src/translation-layer-next); everything else through
/// the older prototype loader.
struct TLAttemptView: View {
    let app: TLApp

    var body: some View {
        content.onAppear { TranslationLayerStore.shared.markPlayed(app) }
    }

    @ViewBuilder
    private var content: some View {
        // Every game the native runtime drives -- Unity included -- gets the same full-screen game screen.
        if app.report?.runsOnNativeRuntime == true {
            TLCocosAttemptView(app: app)
        } else {
            TLClassicAttemptView(app: app)
        }
    }
}

/// A game Husk runs on its own Java interpreter (Flappy Bird): full screen like every other game, with the same bar, the
/// same three-finger way to hide it, and the game's own settings.
struct TLClassicAttemptView: View {
    let app: TLApp
    @StateObject private var runner = TLAttemptRunner()
    @Environment(\.dismiss) private var dismiss
    @AppStorage("husk.tl.showLog") private var showLogSetting = false
    @AppStorage(TranslationLayer.devInfoKey) private var devInfo = false
    @State private var settings: TLAppSettings
    @State private var uiHidden: Bool
    @State private var hint = false
    private let cleanLayout: Bool
    private var showLog: Bool { get { showLogSetting && devInfo } nonmutating set { showLogSetting = newValue } }

    init(app: TLApp) {
        self.app = app
        let loaded = TLAppSettings.load(app.id)
        _settings = State(initialValue: loaded)
        _uiHidden = State(initialValue: loaded.cleanView)
        cleanLayout = loaded.cleanView
    }

    /// These games draw a portrait picture (Flappy Bird's is 540 x 960), unless the game's settings say otherwise.
    private var portrait: Bool { settings.orientation != .landscape }

    private func toggleInterface() {
        withAnimation(.easeInOut(duration: 0.15)) { uiHidden.toggle() }
        if uiHidden { showHint() }
    }

    private func showHint() {
        hint = true
        DispatchQueue.main.asyncAfter(deadline: .now() + 3) { withAnimation(.easeOut(duration: 0.4)) { hint = false } }
    }

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 0) {
                if !cleanLayout {
                    bar.opacity(uiHidden ? 0 : 1).allowsHitTesting(!uiHidden)
                }
                ZStack {
                    TLScreenView(showsStats: settings.showStats && !uiHidden, onThreeFingerTap: { toggleInterface() })
                    if !runner.isRunning, runner.frameCount == 0 {
                        VStack(spacing: 10) {
                            if runner.isDone {
                                Text("The game stopped").font(.headline).foregroundStyle(.white)
                                Button("Run Again") { runner.start(apks: app.apks) }.buttonStyle(.borderedProminent)
                            } else {
                                ProgressView().tint(.white)
                                Text("Starting \(app.label)…").font(.subheadline).foregroundStyle(.white.opacity(0.7))
                            }
                        }
                    }
                }
                .ignoresSafeArea(.container, edges: cleanLayout ? .all : [.horizontal, .bottom])
                if showLog, !uiHidden { logPanel.frame(height: 220) }
            }
            if cleanLayout, !uiHidden {
                VStack(spacing: 0) { bar; Spacer() }
            }
            if hint {
                VStack {
                    Spacer()
                    Text("Tap with three fingers to show the interface")
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 14).padding(.vertical, 8)
                        .background(.black.opacity(0.65), in: Capsule())
                        .padding(.bottom, 26)
                }
                .allowsHitTesting(false)
                .transition(.opacity)
            }
        }
        .statusBarHidden(true)
        .persistentSystemOverlays(.hidden)
        .defersSystemGestures(on: .all)
        .onAppear {
            HuskOrientation.set(portrait ? .portrait : .landscape)
            UIApplication.shared.isIdleTimerDisabled = settings.keepAwake
            if cleanLayout { showHint() }
            CrashReport.gameStarted(app)
            runner.start(apks: app.apks)
        }
        .onDisappear {
            CrashReport.gameEnded()
            runner.stop()
            UIApplication.shared.isIdleTimerDisabled = false
            HuskOrientation.set(HuskOrientation.standard)
        }
    }

    private var bar: some View {
        HStack(spacing: 12) {
            Button { runner.stop(); dismiss() } label: {
                Label("Close", systemImage: "xmark").font(.system(size: 13, weight: .semibold))
            }
            .tint(.white)
            Circle().fill(runner.statusColor).frame(width: 7, height: 7)
            Text(runner.isRunning ? app.label : runner.statusText)
                .font(.system(size: 12, weight: .medium)).foregroundStyle(.white.opacity(0.85)).lineLimit(1)
            Spacer()
            if devInfo {
                Button { withAnimation(.snappy(duration: 0.25)) { showLog.toggle() } } label: {
                    Text(showLog ? "Hide log" : "Log").font(.system(size: 12, weight: .semibold))
                }
                .tint(.white)
            }
            Button { toggleInterface() } label: {
                Label("Hide", systemImage: "eye.slash").font(.system(size: 12, weight: .semibold))
            }
            .tint(.white)
        }
        .padding(.horizontal, 14)
        .frame(height: 30)
        .background(Color(white: 0.08))
    }

    private var logPanel: some View {
        ScrollViewReader { proxy in
            ScrollView {
                Text(runner.logText.isEmpty ? "Starting…" : runner.logText)
                    .font(.technical(10))
                    .foregroundStyle(.white.opacity(0.85))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(10)
                    .textSelection(.enabled)
                    .id("bottom")
            }
            .background(Color(white: 0.06))
            .onChange(of: runner.logText) { _ in proxy.scrollTo("bottom", anchor: .bottom) }
        }
    }
}

