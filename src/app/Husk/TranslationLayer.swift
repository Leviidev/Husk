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
        if engine == "NativeActivity" { return .nativeactivity }
        return nil
    }

    /// Games from those engines run through the native runtime, which handles what the scan flags per library
    /// (raw system calls, thread-register use, pages shared between segments). A report made before that
    /// runtime existed still says "needs work", so the app judges by the engine, not by the stored words.
    var runsOnNativeRuntime: Bool { nativeEngine != nil }

    /// "Unity" or "Cocos2d-x", for words on screen.
    var nativeEngineName: String { nativeEngine == .cocos ? "Cocos2d-x" : nativeEngine == .minecraft ? "Minecraft" : nativeEngine == .sdl ? "SDL" : nativeEngine == .ue4 ? "Unreal Engine" : nativeEngine == .gta ? "Rockstar" : nativeEngine == .nativeactivity ? "NativeActivity" : "Unity" }

    var displaySummary: String {
        guard runsOnNativeRuntime else { return localizedString(summary) }
        let flagged = libraries.filter { $0.abi == "arm64-v8a" && $0.status != "ok" }.count
        let total = libraries.filter { $0.abi == "arm64-v8a" }.count
        var text = String(localized: "A \(nativeEngineName) game. It runs through Husk's native runtime, which loads its \(total) arm64 libraries itself.")
        if nativeEngine == .cocos || nativeEngine == .minecraft || nativeEngine == .ue4 || nativeEngine == .gta { text += " " + String(localized: "It is a landscape game: Husk turns the screen for it.") }
        else if nativeEngine == .sdl || nativeEngine == .nativeactivity { text += " " + String(localized: "Husk turns the screen the way the game asks for.") }
        if flagged > 0 {
            text += " " + String(localized: "\(flagged) of them use tricks the older loader could not handle; the native runtime handles those too, except for optional anti-tamper code, which it leaves out.")
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

    nonisolated private static func scanStamp(_ id: String) -> Int {
        let url = TranslationLayer.root.appendingPathComponent(id, isDirectory: true).appendingPathComponent("scan-version.txt")
        return (try? String(contentsOf: url, encoding: .utf8)).flatMap { Int($0.trimmingCharacters(in: .whitespacesAndNewlines)) } ?? 0
    }

    private func rescanStale() {
        guard !rescanning else { return }
        let stale = apps.filter { Self.scanStamp($0.id) != Self.scanVersion }
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
        busy = urls.count == 1 ? String(localized: "Adding \(urls[0].lastPathComponent)…")
                               : String(localized: "Adding \(urls.count) APKs…")
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
        let apks = files.filter { $0.lowercased().hasSuffix(".apk") }
            .sorted { a, b in
                // The base first: it is the one a game is started from, and the others are its splits and packs.
                let ra = BundleUnpacker.rank(a), rb = BundleUnpacker.rank(b)
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
        return TLApp(id: dir.lastPathComponent,
                     label: label.isEmpty ? (first as NSString).lastPathComponent : label,
                     iconPath: fm.fileExists(atPath: icon) ? icon : nil,
                     apks: apks, report: report)
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
            return String(localized: "Husk could not copy it: \(error.localizedDescription)")
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
    nonisolated private static func describe(_ apks: [String], into dir: URL) {
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

            if let label = info.label {
                try? label.write(to: dir.appendingPathComponent("label.txt"),
                                 atomically: true, encoding: .utf8)
            }
            // An adaptive icon is an instruction, not a picture: follow it to
            // the layer it draws in front.
            var iconEntry = info.iconEntry
            if let xml = iconEntry, xml.hasSuffix(".xml") {
                iconEntry = nil
                if let data = entry(apk, xml, limit: 4 << 20),
                   let layer = ApkMetadata.adaptiveLayer(data),
                   let bitmap = ApkMetadata.bitmap(for: layer, resources: arsc),
                   !bitmap.hasSuffix(".xml") {
                    iconEntry = bitmap
                }
            }
            // Stored as PNG whatever it was, so the icon view needs no WebP.
            if let iconEntry, let data = entry(apk, iconEntry, limit: 16 << 20),
               let png = UIImage(data: data)?.pngData() {
                try? png.write(to: dir.appendingPathComponent("icon.png"))
            }
            HuskLog.log("tl", "\((apk as NSString).lastPathComponent): "
                            + "label=\(info.label ?? "?") icon=\(iconEntry ?? "?")")
            return
        }
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

struct TranslationLayerTab: View {
    @ObservedObject private var store = TranslationLayerStore.shared
    @State private var importing = false
    @AppStorage(TranslationLayer.devInfoKey) private var devInfo = false
    /// Where a tap on a game goes: its page, or its settings.
    private enum Destination: Hashable { case detail(String), settings(String) }
    @State private var destination: Destination?
    /// A game started from its tile's menu, without opening its page first.
    @State private var playing: TLApp?

    var body: some View {
        NavigationStack {
            Form {
                // Everything here needs JIT, and StikJIT -- built into Husk -- is the way to get it.
                Section {
                    JITCard()
                }
                .listRowInsets(EdgeInsets())
                .listRowBackground(Color.clear)

                appsSection
                if devInfo {
                    checksSection
                    progressSection
                }
            }
            .navigationTitle("Translation Layer")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button { importing = true } label: { Label("Add App", systemImage: "plus") }
                        .disabled(store.busy != nil)
                }
            }
            .huskFilePicker(isPresented: $importing) { urls in
                HuskLog.log("ui", "translation layer: adding \(urls.count) file(s): "
                          + urls.map(\.lastPathComponent).joined(separator: ", "))
                store.add(urls)
            }
            .navigationDestination(isPresented: Binding(get: { destination != nil }, set: { if !$0 { destination = nil } })) {
                switch destination {
                case .detail(let id)?: if let app = store.apps.first(where: { $0.id == id }) { TLAppReportView(app: app) }
                case .settings(let id)?: if let app = store.apps.first(where: { $0.id == id }) { TLAppSettingsView(app: app) }
                case nil: EmptyView()
                }
            }
            // A native-runtime game is swiped, and a sheet takes a swipe down for itself: it goes full screen.
            .fullScreenCover(item: $playing) { TLAttemptView(app: $0) }
            .onAppear { store.adoptDroppedAPKs() }
            .alert("Could not add the app", isPresented: Binding(
                    get: { store.lastError != nil },
                    set: { if !$0 { store.lastError = nil } })) {
                Button("OK", role: .cancel) { store.lastError = nil }
            } message: {
                Text(store.lastError ?? "")
            }
        }
    }

    private let columns = [GridItem(.adaptive(minimum: 104), spacing: 12)]

    private var appsSection: some View {
        Section {
            // A grid, as the Library has for the apps inside Android. Each tile is a plain button in a custom style, so a row of
            // them does not turn into one big tap target the way buttons in a list do.
            LazyVGrid(columns: columns, spacing: 12) {
                ForEach(store.apps) { app in
                    Button { destination = .detail(app.id) } label: { TLAppTile(app: app) }
                        .buttonStyle(CardButtonStyle())
                        .contextMenu {
                            if app.report?.runsOnNativeRuntime == true {
                                Button { playing = app } label: { Label("Play", systemImage: "play.fill") }
                            }
                            Button { destination = .detail(app.id) } label: { Label("Details", systemImage: "info.circle") }
                            Button { destination = .settings(app.id) } label: { Label("Settings", systemImage: "gearshape") }
                        }
                }
                Button { importing = true } label: { TLAddTile() }
                    .buttonStyle(CardButtonStyle())
                    .disabled(store.busy != nil)
            }
            .padding(.vertical, 4)
            .listRowInsets(EdgeInsets(top: 4, leading: 16, bottom: 4, trailing: 16))
            .listRowBackground(Color.clear)
            if let busy = store.busy {
                HStack(spacing: 10) {
                    ProgressView()
                    Text(busy).foregroundStyle(Theme.textDim)
                }
            }
        } header: {
            Text("Apps")
        } footer: {
            Text(localizedKey("Runs Android games straight on your iPhone, without starting Android. Add an APK, or a bundle "
               + "(.xapk, .apkm, .apks) — or pick a base APK and its split pieces together. "
               + "Husk keeps its own copy, apart from Android's."))
        }
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
                Label(localizedKey(store.checking ? "Checking…"
                                   : store.checks.isEmpty ? "Run checks" : "Run again"),
                      systemImage: "stethoscope")
            }
            .disabled(store.checking)
        } header: {
            Text("This iPhone")
        } footer: {
            Text(localizedKey("Android's native code expects things of the processor and of memory "
               + "that only the phone can answer -- above all, whether a library's code "
               + "can run with its data writable right beside it. Enable JIT first, or "
               + "the memory checks are skipped. The results go to the console too."))
        }
    }

    private var progressSection: some View {
        Section {
            DetailRow(label: "App reports", value: localizedString("working"), mono: false)
            DetailRow(label: "Device checks", value: localizedString("working"), mono: false)
            DetailRow(label: "Library loader", value: localizedString("working"), mono: false)
            DetailRow(label: "Android runtime", value: "Unity, cocos2d-x, GameActivity, SDL3", mono: false)
            DetailRow(label: "Opening apps", value: localizedString("Unity, cocos2d-x, Minecraft and SDL3 games"), mono: false)
        } header: {
            Text("Where it stands")
        } footer: {
            Text("The plan, in order, is docs/04-translation-layer.md in Husk's source.")
        }
    }
}

/// A verdict, as words and a colour.
struct TLVerdict {
    let title: LocalizedStringKey
    let tint: Color

    init(_ report: TLReport?) {
        if report?.runsOnNativeRuntime == true {
            title = "\(report!.nativeEngineName): native runtime"; tint = Theme.good
            return
        }
        switch report?.verdict {
        case "java"?:           title = "Java only";                    tint = Theme.good
        case "native"?:         title = "Native code, maps cleanly";    tint = Theme.good
        case "nativeWithWork"?: title = "Native code, needs work";      tint = .orange
        case "noArm64"?:        title = "No arm64 code";                tint = .red
        case "unreadable"?:     title = "Could not be read";            tint = .red
        default:                title = "Not scanned";                  tint = Theme.textDim
        }
    }
}

/// What a person who is not debugging needs to know about an app: will it run here, or may it not.
struct TLPlainStatus {
    let title: LocalizedStringKey
    let tint: Color

    init(_ report: TLReport?) {
        if report?.runsOnNativeRuntime == true { title = "Ready to run"; tint = Theme.good }
        else { title = "May not run"; tint = Theme.textDim }
    }
}

/// One game in the grid: its icon, its name, and whether it will run.
private struct TLAppTile: View {
    let app: TLApp
    @AppStorage(TranslationLayer.devInfoKey) private var devInfo = false

    var body: some View {
        let verdict = TLVerdict(app.report)
        let plain = TLPlainStatus(app.report)
        VStack(spacing: 8) {
            AppIcon(path: app.iconPath, size: 56)
            Text(app.label)
                .font(.footnote.weight(.semibold))
                .foregroundStyle(.primary)
                .lineLimit(2)
                .multilineTextAlignment(.center)
                .frame(maxWidth: .infinity, minHeight: 34, alignment: .top)
            Text(devInfo ? verdict.title : plain.title)
                .font(.caption2)
                .foregroundStyle(devInfo ? verdict.tint : plain.tint)
                .lineLimit(1)
        }
        .frame(maxWidth: .infinity)
        .padding(12)
        .huskCard()
    }
}

/// The last tile: add another.
private struct TLAddTile: View {
    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: "plus")
                .font(.system(size: 22, weight: .semibold))
                .foregroundStyle(Theme.accent)
                .frame(width: 56, height: 56)
                .background(Theme.accentSoft, in: RoundedRectangle(cornerRadius: 56 * 0.225, style: .continuous))
            Text("Add")
                .font(.footnote.weight(.semibold))
                .foregroundStyle(.primary)
                .frame(maxWidth: .infinity, minHeight: 34, alignment: .top)
            Text("APK or bundle")
                .font(.caption2)
                .foregroundStyle(Theme.textDim)
                .lineLimit(1)
        }
        .frame(maxWidth: .infinity)
        .padding(12)
        .huskCard()
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

struct TLAppReportView: View {
    let app: TLApp

    @ObservedObject private var store = TranslationLayerStore.shared
    @Environment(\.dismiss) private var dismiss
    @State private var confirmRemove = false
    @State private var showAttempt = false
    @AppStorage(TranslationLayer.devInfoKey) private var devInfo = false

    var body: some View {
        let verdict = TLVerdict(app.report)
        let plain = TLPlainStatus(app.report)
        Form {
            Section {
                HStack(spacing: 14) {
                    AppIcon(path: app.iconPath, size: 56)
                    VStack(alignment: .leading, spacing: 4) {
                        Text(app.label)
                            .font(.system(size: 20, weight: .semibold))
                            .foregroundStyle(Theme.text)
                        Text(devInfo ? verdict.title : plain.title)
                            .font(.system(size: 13, weight: .medium))
                            .foregroundStyle(devInfo ? verdict.tint : plain.tint)
                    }
                }
                .padding(.vertical, 4)
                if devInfo, let report = app.report {
                    Text(report.displaySummary)
                        .font(.system(size: 14))
                        .foregroundStyle(Theme.text)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }

            Section {
                Button {
                    showAttempt = true
                } label: {
                    HStack {
                        Label(localizedKey(devInfo ? "Run Translation Layer Attempt" : "Play"), systemImage: "play.circle.fill")
                            .font(.system(size: 15, weight: .semibold))
                            .foregroundStyle(Theme.accent)
                        Spacer()
                        Image(systemName: "chevron.right")
                            .font(.caption.bold())
                            .foregroundStyle(Theme.textDim.opacity(0.5))
                    }
                }
                NavigationLink {
                    TLAppSettingsView(app: app)
                } label: {
                    Label("Settings", systemImage: "gearshape")
                }
            } footer: {
                if devInfo {
                    Text(localizedKey("Loads arm64 native code into JIT memory on Apple Silicon and drives "
                       + "a NativeActivity lifecycle. Apps with Java/Dex require ART (milestone 2)."))
                } else {
                    Text("Turn on JIT first. Close the game with Close at the top.")
                }
            }

            if devInfo, let report = app.report {
                Section {
                    if let engine = report.engine {
                        DetailRow(label: "Made with", value: engine, mono: false)
                    }
                    DetailRow(label: "Dex", value: dex(report), mono: false)
                    DetailRow(label: "ABIs", value: report.abis.isEmpty
                              ? localizedString("none") : report.abis.joined(separator: ", "))
                    DetailRow(label: "APKs", value: "\(app.apks.count)", mono: false)
                } header: {
                    Text("What it is")
                }

                if !report.systemLibraries.isEmpty {
                    Section {
                        Text(report.systemLibraries.joined(separator: "  "))
                            .font(.technical(12))
                            .foregroundStyle(Theme.text)
                            .textSelection(.enabled)
                    } header: {
                        Text("Android libraries it needs")
                    } footer: {
                        Text(localizedKey("Its own libraries link against these, and it does not "
                           + "carry them. Each is something the translation layer has "
                           + "to provide."))
                    }
                }

                ForEach(report.libraries) { lib in
                    Section {
                        TLLibraryRows(lib: lib)
                    } header: {
                        Text(lib.name).textCase(nil)
                    }
                }
            }

            Section {
                Button(role: .destructive) { confirmRemove = true } label: {
                    Label("Remove", systemImage: "trash")
                }
            } footer: {
                Text(localizedKey("Deletes Husk's copy of the APKs. Anything installed in Android is "
                   + "untouched."))
            }
        }
        .huskForm()
        .navigationTitle(app.label)
        .confirmationDialog("Remove \(app.label)?", isPresented: $confirmRemove,
                            titleVisibility: .visible) {
            Button("Remove", role: .destructive) {
                store.remove(app)
                dismiss()
            }
            Button("Cancel", role: .cancel) { }
        }
        // A native-runtime game is swiped, and a sheet takes a swipe down for itself: it goes full screen.
        .sheet(isPresented: Binding(get: { showAttempt && app.report?.runsOnNativeRuntime != true },
                                    set: { showAttempt = $0 })) {
            TLAttemptView(app: app)
        }
        .fullScreenCover(isPresented: Binding(get: { showAttempt && app.report?.runsOnNativeRuntime == true },
                                              set: { showAttempt = $0 })) {
            TLAttemptView(app: app)
        }
    }

    private func dex(_ report: TLReport) -> String {
        let files = report.dexCount == 1 ? localizedString("1 file")
                                         : String(localized: "\(report.dexCount) files")
        let size = ByteCountFormatter.string(fromByteCount: report.dexBytes, countStyle: .file)
        return String(localized: "\(files), \(size)")
    }
}

private struct TLLibraryRows: View {
    let lib: TLLibrary

    private var statusTitle: LocalizedStringKey {
        switch lib.status {
        case "ok":      return "Maps as it is"
        case "work":    return "Needs loader work"
        default:        return "Cannot load"
        }
    }

    private func relocationText(_ count: Int) -> String {
        guard let packing = lib.packing, packing != "none" else { return "\(count)" }
        return "\(count) (\(packing))"
    }

    private var statusTint: Color {
        switch lib.status {
        case "ok":      return Theme.good
        case "work":    return .orange
        default:        return .red
        }
    }

    var body: some View {
        HStack {
            Tag(text: statusTitle, tint: statusTint)
            Spacer()
            Text(ByteCountFormatter.string(fromByteCount: lib.bytes, countStyle: .file))
                .foregroundStyle(Theme.textDim)
        }
        ForEach(lib.notes, id: \.self) { note in
            Text(note)
                .font(.system(size: 13))
                .foregroundStyle(Theme.text)
                .fixedSize(horizontal: false, vertical: true)
        }
        if let layout = lib.layout {
            DetailRow(label: "16 KiB pages", value: layout)
        }
        if let relocations = lib.relocations {
            DetailRow(label: "Relocations", value: relocationText(relocations))
        }
        if let imports = lib.imports {
            DetailRow(label: "Imported symbols", value: "\(imports)")
        }
        if let svc = lib.svc, svc > 0 {
            DetailRow(label: "System calls", value: "\(svc)")
        }
        if let reads = lib.tpidrReads {
            DetailRow(label: "Thread register reads", value: "\(reads)")
        }
        if lib.tls == true {
            DetailRow(label: "Thread-local storage", value: localizedString("yes"), mono: false)
        }
    }
}

// MARK: - Live Attempt Execution & Screen

@MainActor
final class TLAttemptRunner: ObservableObject {
    @Published var isRunning = false
    @Published var isDone = false
    @Published var exitCode: Int? = nil
    @Published var frameCount = 0
    @Published var logText: String = ""

    private var timer: Timer?

    var statusText: LocalizedStringKey {
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

    var subStatusText: LocalizedStringKey {
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
        if app.report?.nativeEngine == .cocos || app.report?.nativeEngine == .minecraft || app.report?.nativeEngine == .sdl || app.report?.nativeEngine == .ue4 || app.report?.nativeEngine == .gta || app.report?.nativeEngine == .nativeactivity {
            TLCocosAttemptView(app: app)
        } else if app.report?.runsOnNativeRuntime == true {
            TLUnityAttemptView(app: app)
        } else {
            TLClassicAttemptView(app: app)
        }
    }
}

struct TLClassicAttemptView: View {
    let app: TLApp
    @StateObject private var runner = TLAttemptRunner()
    @Environment(\.dismiss) private var dismiss
    /// Whether the log is open. Remembered, so a game opened again comes back the
    /// way it was left.
    @AppStorage("husk.tl.showLog") private var showLogSetting = true
    @AppStorage(TranslationLayer.devInfoKey) private var devInfo = false
    /// The log is detail: without developer info it stays shut and its bar is not shown at all.
    private var showLog: Bool { get { showLogSetting && devInfo } nonmutating set { showLogSetting = newValue } }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                HStack {
                    VStack(alignment: .leading, spacing: 3) {
                        Text(runner.statusText)
                            .font(.system(size: 15, weight: .semibold))
                            .foregroundStyle(runner.statusColor)
                        Text(runner.subStatusText)
                            .font(.system(size: 12))
                            .foregroundStyle(Theme.textDim)
                    }
                    Spacer()
                    if runner.isRunning {
                        Button("Stop") {
                            runner.stop()
                        }
                        .buttonStyle(.bordered)
                        .tint(.red)
                    } else {
                        Button("Rerun") {
                            runner.start(apks: app.apks)
                        }
                        .buttonStyle(.borderedProminent)
                    }
                }
                .padding()
                .background(Theme.surface)

                Divider()

                // The game takes whatever the log leaves. With the log open it is a
                // fixed 380 points; closed, it fills the rest of the screen.
                if runner.isRunning || runner.frameCount > 0 {
                    TLScreenView()
                        .frame(maxWidth: .infinity, maxHeight: showLog ? 380 : .infinity)
                        .background(Color.black)
                    Divider()
                }

                if devInfo {
                // The log's bar, always there: it is the way back in once the log
                // is closed, so it cannot be part of what closes.
                HStack(spacing: 10) {
                    Button {
                        withAnimation(.snappy(duration: 0.25)) { showLog.toggle() }
                    } label: {
                        HStack(spacing: 6) {
                            Image(systemName: showLog ? "chevron.down" : "chevron.right")
                                .font(.system(size: 11, weight: .bold))
                                .frame(width: 12)
                            Text("ATTEMPT LOG")
                                .font(.technical(11, weight: .bold))
                        }
                        .foregroundStyle(Theme.textDim)
                    }
                    .buttonStyle(.plain)
                    Spacer()
                    if showLog {
                        Button {
                            UIPasteboard.general.string = runner.logText
                        } label: {
                            Label("Copy", systemImage: "doc.on.doc")
                                .font(.system(size: 12))
                        }
                    } else {
                        Text("tap to show")
                            .font(.system(size: 11))
                            .foregroundStyle(Theme.textDim.opacity(0.7))
                    }
                }
                .padding(.horizontal)
                .padding(.vertical, 8)
                .contentShape(Rectangle())
                .onTapGesture {
                    if !showLog { withAnimation(.snappy(duration: 0.25)) { showLog = true } }
                }

                }

                if showLog {
                    ScrollViewReader { proxy in
                        ScrollView {
                            Text(localizedKey(runner.logText.isEmpty ? "Starting attempt..." : runner.logText))
                                .font(.technical(11))
                                .foregroundStyle(Theme.text)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(12)
                                .textSelection(.enabled)
                                .id("bottom")
                        }
                        .background(Theme.bg)
                        .onChange(of: runner.logText) { _ in
                            proxy.scrollTo("bottom", anchor: .bottom)
                        }
                    }
                    .transition(.opacity)
                }
            }
            .navigationTitle(app.label)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Close") {
                        runner.stop()
                        dismiss()
                    }
                }
            }
            .onAppear {
                runner.start(apks: app.apks)
            }
            .onDisappear {
                runner.stop()
            }
        }
    }
}

