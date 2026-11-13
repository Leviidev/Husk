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
    let assessment: String?
    let engineVariant: String?
}

extension TLReport {
    /// The engine the native runtime drives this app with, if it can: Unity (IL2CPP) games and cocos2d-x games.
    var nativeEngine: TLNativeEngine? {
        guard ok else { return nil }
        // Native code for other CPUs only (x86, MIPS): nothing here can run it. 32-bit ARM is translated.
        if !abis.isEmpty && !abis.contains("arm64-v8a") && !abis.contains("armeabi-v7a") && !abis.contains("armeabi") { return nil }
        // An app with Java code and no native code at all runs on Husk's own Java runtime
        if abis.isEmpty { return dexCount > 0 && webKind == nil ? .java : nil }
        // Unity 6 runs through its own Java activity (UnityPlayerForActivityOrService), on the Java runtime
        if engine == "Unity 6" { return .java }
        if engine?.hasPrefix("Unity") == true { return .unity }
        // the cocos2d-x driver is Geometry Dash's (FMOD, RobTop's activity); other cocos2d-x games run their Java activity on the Java runtime
        if engine == "Cocos" { return libraries.contains(where: { $0.name == "libfmod.so" }) ? .cocos : .java }
        if engine == "Minecraft" { return .minecraft }
        // SDL games run their own Java activity on the Java runtime: many prepare their data there first (Wesnoth unpacks it, OpenTTD
        // and Luanti copy theirs, launchers come before the game) and SDLActivity's Java drives the native side as on a phone.
        // python-for-android (Kivy) too: pyjnius needs real Java (proxies, string identity), which only the Java runtime has.
        // Except Direct3D ports drawing through DXVK (GTA V): they need Vulkan on a Metal layer of their own, which only the SDL driver gives.
        if engine == "SDL", libraries.contains(where: { $0.name.hasPrefix("libdxvk") }) { return .sdl }
        if engine == "SDL" || engine == "Python" { return .java }
        if engine == "Unreal Engine" { return .ue4 }
        if engine == "Rockstar" { return .gta }
        if engine == "Godot" { return .godot }
        if engine == "NativeActivity" { return .nativeactivity }
        // Flutter apps too: their plugins are Java (storage, paths, keystore, SQLite) and run there for real, where the Flutter
        // driver could only answer a few channels itself (Lichess waited forever, Sanmill failed on one).
        if engine == "Flutter" { return .java }
        if engine == "GameMaker" { return .gamemaker }
        // Everything else with Java code: libGDX games, ordinary apps, React Native and the like, and games whose engine is their
        // own library behind a Java activity (Hill Climb Racing, Supercell's games)
        if engine == "libGDX" || engine == "Java" || engine == "React Native" || engine == ".NET / Xamarin" { return .java }
        if engine == nil, dexCount > 0 { return .java }
        return nil
    }

    /// Games from those engines run through the native runtime, which handles what the scan flags per library
    /// (raw system calls, thread-register use, pages shared between segments). A report made before that
    /// runtime existed still says "needs work", so the app judges by the engine, not by the stored words.
    var runsOnNativeRuntime: Bool { nativeEngine != nil }

    /// "Unity" or "Cocos2d-x", for words on screen.
    var nativeEngineName: String { engine == "Python" ? "Python (Kivy)" : nativeEngine == .cocos ? "Cocos2d-x" : nativeEngine == .minecraft ? "Minecraft" : nativeEngine == .sdl ? "SDL" : nativeEngine == .ue4 ? "Unreal Engine" : nativeEngine == .gta ? "Rockstar" : nativeEngine == .godot ? "Godot" : nativeEngine == .nativeactivity ? "NativeActivity" : nativeEngine == .flutter ? "Flutter" : nativeEngine == .gamemaker ? "GameMaker" : nativeEngine == .java ? (engine == "libGDX" ? "libGDX" : engine == "SDL" ? "SDL" : engine == "Flutter" ? "Flutter" : "Android") : "Unity" }

    var displaySummary: String {
        guard runsOnNativeRuntime else { return summary }
        let flagged = libraries.filter { ($0.abi == "arm64-v8a" || $0.abi == "armeabi-v7a" || $0.abi == "armeabi") && $0.status != "ok" }.count
        let total = libraries.filter { $0.abi == "arm64-v8a" || $0.abi == "armeabi-v7a" || $0.abi == "armeabi" }.count
        var text = nativeEngine == .flutter
            ? "A Flutter app. Its engine and its compiled Dart run natively through Husk's translation layer, which loads its \(total) arm64 libraries itself."
            : nativeEngine == .java && engine != "libGDX"
            ? "An Android app. Husk runs its code on its own Java runtime" + (total > 0 ? " and loads its \(total) arm64 libraries itself." : ".")
            : "A \(nativeEngineName) game. It runs through Husk's native runtime, which loads its \(total) arm64 libraries itself."
        if nativeEngine == .cocos || nativeEngine == .minecraft || nativeEngine == .ue4 || nativeEngine == .gta { text += " It is a landscape game: Husk turns the screen for it." }
        else if nativeEngine == .sdl || nativeEngine == .nativeactivity || nativeEngine == .godot || nativeEngine == .gamemaker { text += " Husk turns the screen the way the game asks for." }
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
    let weakImports: Int?
    let importNamesComplete: Bool?
    let importNames: [String]?
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
    nonisolated static let scanVersion = 9
    private var rescanning = false

    func reload() {
        let dirs = (try? FileManager.default.contentsOfDirectory(
            at: TranslationLayer.root, includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])) ?? []
        apps = dirs.compactMap(Self.load).sorted {
            $0.label.localizedCaseInsensitiveCompare($1.label) == .orderedAscending
        }
        rescanStale()
        PS4Store.shared.reload()   // the PS4 games, which Files may have changed too
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
    /// One app's APKs (a base and its splits, or a bundle) copied in (the originals stay where they are), and the new app's folder name: the developer mass test takes its APKs
    /// from a folder of the user's this way, one per launch of Husk.
    nonisolated static func importCopy(_ urls: [URL]) -> String? {
        let fm = FileManager.default
        let before = Set((try? fm.contentsOfDirectory(atPath: TranslationLayer.root.path)) ?? [])
        guard !urls.isEmpty, ingest(urls, move: false) == nil else { return nil }
        let after = Set((try? fm.contentsOfDirectory(atPath: TranslationLayer.root.path)) ?? [])
        return after.subtracting(before).first
    }

    func add(_ urls: [URL], move: Bool = false) {
        guard !urls.isEmpty, busy == nil else { return }
        busy = urls.count == 1 ? "Adding \(urls[0].lastPathComponent)…"
                               : "Adding \(urls.count) APKs…"
        Task { @MainActor in
            let outcome = await Self.runIngestFlow(urls, move: move)
            self.busy = nil
            self.lastError = outcome.error
            self.reload()
            if outcome.error == nil, outcome.updated {
                let name = outcome.label ?? "game"
                AndroidHost.shared.say("Updated \(name)", "All saves and game data were kept.")
            }
            if outcome.error == nil, move { self.adoptDroppedAPKs() }      // the next one waiting, if several were put there
        }
    }

    /// Update a specific installed app in place from a newly picked APK/bundle while keeping saves, OBB, and settings.
    func update(_ app: TLApp, with urls: [URL], completion: @escaping @MainActor (String?, Bool) -> Void) {
        guard !urls.isEmpty, busy == nil else { return }
        busy = "Updating \(app.label)…"
        Task { @MainActor in
            let outcome = await Self.runIngestFlow(urls, move: false, targetAppID: app.id, expectedPackage: app.packageName)
            self.busy = nil
            if let err = outcome.error { self.lastError = err }
            self.reload()
            if outcome.error == nil {
                let name = outcome.label ?? app.label
                AndroidHost.shared.say("Updated \(name)", "All saves and game data were kept.")
            }
            completion(outcome.error, outcome.error == nil)
        }
    }

    @MainActor
    private static func runIngestFlow(_ urls: [URL], move: Bool, targetAppID: String? = nil, expectedPackage: String? = nil) async -> (error: String?, updated: Bool, label: String?) {
        let staged = await Task.detached(priority: .userInitiated) {
            Self.stageInputs(urls, move: move, targetAppID: targetAppID, expectedPackage: expectedPackage)
        }.value
        if let err = staged.error { return (err, false, nil) }
        guard let stagingDir = staged.stagingDir else { return ("Missing staging folder.", false, nil) }

        if let existing = staged.existingDir,
           let stopErr = await ensureNotLoadedBeforeReplace(existingDir: existing, labelHint: staged.labelHint) {
            try? FileManager.default.removeItem(at: stagingDir)
            return (stopErr, false, nil)
        }

        let committed = await Task.detached(priority: .userInitiated) {
            Self.commitIngest(stagingDir: stagingDir, existingDir: staged.existingDir, newPkg: staged.newPkg)
        }.value
        let label = committed.dir.flatMap { load($0)?.label } ?? staged.labelHint
        return (committed.error, committed.updated, label)
    }

    @MainActor
    private static func ensureNotLoadedBeforeReplace(existingDir: URL, labelHint: String?) async -> String? {
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }) else { return nil }
        let loadedPath = URL(fileURLWithPath: loaded).standardizedFileURL.path
        let existingPath = existingDir.standardizedFileURL.path
        guard loadedPath.hasPrefix(existingPath + "/") || loadedPath == existingPath else { return nil }
        if husk_native_can_stop() {
            let stopped = await TLNativeSession.shared.stop(only: loaded)
            return stopped ? nil : "Could not close \(labelHint ?? "this game") cleanly before updating. Close Husk and try again."
        }
        return "\(labelHint ?? "This game") is still loaded in this session. Close Husk and reopen it to update without losing saves."
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

    nonisolated private static func packageName(in dir: URL, apks knownAPKs: [String]? = nil) -> String? {
        let pkgFile = dir.appendingPathComponent("package.txt")
        if let saved = (try? String(contentsOf: pkgFile, encoding: .utf8))?.trimmingCharacters(in: .whitespacesAndNewlines),
           !saved.isEmpty {
            return saved
        }
        let fm = FileManager.default
        let apks = knownAPKs ?? orderedAPKs(((try? fm.contentsOfDirectory(atPath: dir.path)) ?? [])
            .filter { $0.lowercased().hasSuffix(".apk") }
            .map { dir.appendingPathComponent($0).path })
        for apk in apks {
            if let manifest = entry(apk, "AndroidManifest.xml", limit: 8 << 20),
               let pkg = ApkMetadata.packageName(manifest)?.trimmingCharacters(in: .whitespacesAndNewlines),
               !pkg.isEmpty {
                try? pkg.write(to: pkgFile, atomically: true, encoding: .utf8)
                return pkg
            }
        }
        return nil
    }

    nonisolated private static func load(_ dir: URL) -> TLApp? {
        let fm = FileManager.default
        guard let files = try? fm.contentsOfDirectory(atPath: dir.path) else { return nil }
        let names = files.filter { $0.lowercased().hasSuffix(".apk") }
        let apks = orderedAPKs(names.map { dir.appendingPathComponent($0).path })
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
                     packageName: packageName(in: dir, apks: apks))
    }

    /// Install an APK/bundle update triggered from inside a running app, preserving existing game data.
    nonisolated static func installUpdate(_ url: URL, move: Bool = false) -> (error: String?, appID: String?, label: String?) {
        let result = ingestWithTarget([url], move: move)
        guard result.error == nil, let dir = result.dir else {
            return (result.error ?? "Không thể cài đặt gói APK.", nil, nil)
        }
        let loaded = load(dir)
        return (nil, dir.lastPathComponent, loaded?.label)
    }

    /// Copy, name, scan. Returns what went wrong, if anything did.
    nonisolated private static func ingest(_ urls: [URL], move: Bool = false) -> String? {
        ingestWithTarget(urls, move: move).error
    }

    nonisolated private static func ingestWithTarget(_ urls: [URL], move: Bool = false, targetAppID: String? = nil, expectedPackage: String? = nil) -> (error: String?, dir: URL?) {
        let staged = stageInputs(urls, move: move, targetAppID: targetAppID, expectedPackage: expectedPackage)
        if let err = staged.error { return (err, nil) }
        guard let stagingDir = staged.stagingDir else { return ("Missing staging folder.", nil) }
        let committed = commitIngest(stagingDir: stagingDir, existingDir: staged.existingDir, newPkg: staged.newPkg)
        return (committed.error, committed.dir)
    }

    nonisolated private static func stageInputs(_ urls: [URL], move: Bool, targetAppID: String?, expectedPackage: String?) -> (error: String?, stagingDir: URL?, existingDir: URL?, newPkg: String?, labelHint: String?) {
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
                // An expansion file (main.<version>.<package>.obb) goes to obb/ under its own name: the runtime finds it there
                // when the game asks for Android/obb/<package>/<name>
                let isOBB = name.lowercased().hasSuffix(".obb")
                if !isOBB && !name.lowercased().hasSuffix(".apk") { name += ".apk" }
                if isOBB { try fm.createDirectory(at: dir.appendingPathComponent("obb"), withIntermediateDirectories: true) }
                let dest = isOBB ? dir.appendingPathComponent("obb").appendingPathComponent(name) : dir.appendingPathComponent(name)
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
            return ("Husk could not copy it: \(error.localizedDescription)", nil, nil, nil, nil)
        }

        let stagingApks = orderedAPKs(((try? fm.contentsOfDirectory(atPath: dir.path)) ?? [])
            .filter { $0.lowercased().hasSuffix(".apk") }
            .map { dir.appendingPathComponent($0).path })
        guard !stagingApks.isEmpty else {
            try? fm.removeItem(at: dir)
            HuskLog.log("tl", "FAILED to add: no APK found in staging folder")
            return ("Không tìm thấy gói APK hợp lệ.", nil, nil, nil, nil)
        }
        describe(stagingApks, into: dir)

        let newPkg = packageName(in: dir, apks: stagingApks)
        if let expectedPackage, !expectedPackage.isEmpty, newPkg != expectedPackage {
            let found = newPkg ?? "an unknown package"
            try? fm.removeItem(at: dir)
            HuskLog.log("tl", "update aborted: picked \(found) instead of \(expectedPackage)")
            return ("That APK belongs to \(found), not \(expectedPackage). Add it from + to install separately.", nil, nil, nil, nil)
        }

        var existingDir: URL?
        if let targetAppID {
            let candidate = TranslationLayer.root.appendingPathComponent(targetAppID, isDirectory: true)
            if fm.fileExists(atPath: candidate.path) { existingDir = candidate }
        } else if let newPkg, !newPkg.isEmpty {
            let existingDirs = (try? fm.contentsOfDirectory(at: TranslationLayer.root, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles])) ?? []
            for existing in existingDirs where existing.lastPathComponent != dir.lastPathComponent {
                if packageName(in: existing) == newPkg {
                    existingDir = existing
                    break
                }
            }
        }
        let hintTarget = existingDir ?? dir
        let hint = (try? String(contentsOf: hintTarget.appendingPathComponent("label.txt"), encoding: .utf8))?
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return (nil, dir, existingDir, newPkg, hint?.isEmpty == false ? hint : nil)
    }

    nonisolated private static func commitIngest(stagingDir dir: URL, existingDir: URL?, newPkg: String?) -> (error: String?, dir: URL?, updated: Bool) {
        let fm = FileManager.default
        var targetDir = dir
        var isUpdate = false

        if let existing = existingDir {
            let oldApks = ((try? fm.contentsOfDirectory(at: existing, includingPropertiesForKeys: nil)) ?? [])
                .filter { $0.pathExtension.lowercased() == "apk" }
            let stagedFiles = ((try? fm.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil)) ?? [])
                .filter { $0.pathExtension.lowercased() == "apk" }
            if !stagedFiles.isEmpty {
                var placedNames = Set<String>()
                for f in stagedFiles {
                    let destName = (stagedFiles.count == 1 && oldApks.count == 1 && f.lastPathComponent == "husk-update.apk")
                        ? oldApks[0].lastPathComponent
                        : f.lastPathComponent
                    let dest = existing.appendingPathComponent(destName)
                    let tmpDest = existing.appendingPathComponent(".\(UUID().uuidString)-\(destName)")
                    do {
                        try fm.moveItem(at: f, to: tmpDest)
                        if fm.fileExists(atPath: dest.path) {
                            _ = try fm.replaceItemAt(dest, withItemAt: tmpDest)
                        } else {
                            try fm.moveItem(at: tmpDest, to: dest)
                        }
                        placedNames.insert(destName.lowercased())
                    } catch {
                        try? fm.removeItem(at: tmpDest)
                        HuskLog.log("tl", "FAILED to replace \(destName) during update: \(error.localizedDescription)")
                    }
                }
                guard !placedNames.isEmpty else {
                    try? fm.removeItem(at: dir)
                    return ("Không thể ghi đè gói APK cũ khi cập nhật.", nil, false)
                }
                for old in oldApks where !placedNames.contains(old.lastPathComponent.lowercased()) {
                    try? fm.removeItem(at: old)
                }
                let stagedOBBDir = dir.appendingPathComponent("obb")
                if let obbs = try? fm.contentsOfDirectory(at: stagedOBBDir, includingPropertiesForKeys: nil), !obbs.isEmpty {
                    let targetOBBDir = existing.appendingPathComponent("obb")
                    try? fm.createDirectory(at: targetOBBDir, withIntermediateDirectories: true)
                    for obb in obbs {
                        let destOBB = targetOBBDir.appendingPathComponent(obb.lastPathComponent)
                        try? fm.removeItem(at: destOBB)
                        try? fm.moveItem(at: obb, to: destOBB)
                    }
                }
                for sub in ["java-data", "sdl-data"] {
                    let libDir = existing.appendingPathComponent(sub).appendingPathComponent("lib")
                    if let libs = try? fm.contentsOfDirectory(at: libDir, includingPropertiesForKeys: nil) {
                        for lib in libs where lib.pathExtension.lowercased() == "so" {
                            try? fm.removeItem(at: lib)
                        }
                    }
                }
                // Clear regenerable code/jar/shader caches so updated DEX/native libs don't hit stale cache,
                // while preserving saves (files/, shared_prefs/, databases/, obb/, settings).
                StorageCleaner.removeGameCaches(in: existing)
                if let pkg = newPkg, !pkg.isEmpty { try? pkgWrite(pkg, into: existing) }
                try? fm.removeItem(at: dir)
                targetDir = existing
                isUpdate = true
                HuskLog.log("tl", "updated existing app for \(newPkg ?? existing.lastPathComponent) in \(existing.lastPathComponent)")
            }
        }

        let apks = orderedAPKs(((try? fm.contentsOfDirectory(atPath: targetDir.path)) ?? [])
            .filter { $0.lowercased().hasSuffix(".apk") }
            .map { targetDir.appendingPathComponent($0).path })
        let keepExistingLabel = isUpdate && fm.fileExists(atPath: targetDir.appendingPathComponent("label.txt").path)
        describe(apks, into: targetDir, label: !keepExistingLabel)
        let json = scan(apks)
        try? Data(json.utf8).write(to: targetDir.appendingPathComponent("report.json"))
        try? "\(scanVersion)".write(to: targetDir.appendingPathComponent("scan-version.txt"), atomically: true, encoding: .utf8)
        HuskLog.log("tl", "report for \(targetDir.lastPathComponent): \(json)")
        return (nil, targetDir, isUpdate)
    }

    nonisolated private static func pkgWrite(_ pkg: String, into dir: URL) throws {
        try pkg.write(to: dir.appendingPathComponent("package.txt"), atomically: true, encoding: .utf8)
    }

    /// The app's own name and icon, from its manifest and resource table --
    /// the same reading the library does for apps inside Android, done here on
    /// the file directly.
    /// Which way of drawing icons an app's icon.png came from. A newer Husk that draws them better draws the apps it
    /// already holds again (icons only: the name may be the user's own).
    nonisolated static let iconVersion = 3

    nonisolated private static func describe(_ apks: [String], into dir: URL, label wantLabel: Bool = true) {
        // Use the same manifest-based order as launch: a large asset pack is not the base APK.
        for apk in orderedAPKs(apks) {
            guard let manifest = entry(apk, "AndroidManifest.xml", limit: 8 << 20) else { continue }
            if let package = ApkMetadata.packageName(manifest), !package.isEmpty {
                try? pkgWrite(package, into: dir)
            }
            guard let arsc = entry(apk, "resources.arsc", limit: 64 << 20) else { continue }
            let info = ApkMetadata.read(manifest: manifest, resources: arsc)
            guard info.label != nil || info.iconEntry != nil else { continue }

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

    nonisolated private static func orderedAPKs(_ paths: [String]) -> [String] {
        BundleUnpacker.orderedAPKs(paths) { entry($0, "AndroidManifest.xml", limit: 8 << 20) }
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
        HuskForm {
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
    @Environment(\.dismiss) private var dismiss
    @State private var activeApp: TLApp
    @State private var switchingTo: TLApp?
    @State private var switchStatus = ""
    @State private var switchError: String?

    init(app: TLApp) {
        self.app = app
        _activeApp = State(initialValue: app)
    }

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            if let next = switchingTo {
                VStack(spacing: 12) {
                    ProgressView().tint(.white)
                    Text(switchStatus.isEmpty ? "Switching to \(next.label)…" : switchStatus)
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(.white)
                        .multilineTextAlignment(.center)
                }
                .padding(24)
            } else {
                content(for: activeApp)
                    .id(activeApp.id)
                    .onAppear {
                        TranslationLayerStore.shared.markPlayed(activeApp)
                        Playtime.begin("tl:" + activeApp.id)
                    }
                    .onDisappear { Playtime.end("tl:" + activeApp.id) }
            }
        }
        .alert("Could Not Switch Game", isPresented: Binding(get: { switchError != nil }, set: { if !$0 { switchError = nil } })) {
            Button("OK", role: .cancel) { switchError = nil }
        } message: {
            Text(switchError ?? "")
        }
    }

    @ViewBuilder
    private func content(for game: TLApp) -> some View {
        // Every game the native runtime drives -- Unity included -- gets the same full-screen game screen.
        if game.report?.webKind != nil {
            WebAppScreenView(app: game)
        } else if game.report?.runsOnNativeRuntime == true {
            TLCocosAttemptView(app: game, onSwitch: { switchTo($0) }, onClose: { dismiss() })
        } else {
            TLClassicAttemptView(app: game, onSwitch: { switchTo($0) }, onClose: { dismiss() })
        }
    }

    @MainActor
    private func switchTo(_ next: TLApp) {
        guard switchingTo == nil, next.id != activeApp.id else { return }
        if activeApp.report?.runsOnNativeRuntime != true && activeApp.report?.webKind == nil {
            husk_tl_attempt_stop()
        }
        let closingLabel = activeApp.label
        switchingTo = next
        switchStatus = "Closing \(closingLabel)…"
        Task { @MainActor in
            // Allow onDisappear cleanup and background stop task to start first.
            await Task.yield()
            let stopped: Bool
            if let nextApk = next.apks.first, next.report?.runsOnNativeRuntime == true {
                stopped = await TLNativeSession.shared.prepare(for: nextApk)
            } else {
                stopped = await TLNativeSession.shared.stop()
            }
            guard stopped else {
                switchingTo = nil
                switchStatus = ""
                switchError = TLNativeSession.shared.lastError ?? "The previous game has not finished closing."
                return
            }
            if JITLess.enabled, SigningCertificate.credentials() != nil {
                switchStatus = "Preparing libraries for \(next.label)…"
                do {
                    try await JITLess.prepare(apks: next.apks) { text in
                        Task { @MainActor in switchStatus = text }
                    }
                } catch {
                    switchingTo = nil
                    switchStatus = ""
                    switchError = error.localizedDescription
                    return
                }
            }
            if TLAppSettings.load(next.id).geode, case .ready = GeodeSupport.shared.current(next) {
                // Already ready.
            } else if TLAppSettings.load(next.id).geode {
                switchStatus = "Preparing Geode…"
                await GeodeSupport.shared.prepare(next)
            }
            activeApp = next
            switchingTo = nil
            switchStatus = ""
        }
    }
}

/// A game Husk runs on its own Java interpreter (Flappy Bird): full screen like every other game, with the same bar, the
/// same three-finger way to hide it, and the game's own settings.
struct TLClassicAttemptView: View {
    let app: TLApp
    var onSwitch: ((TLApp) -> Void)? = nil
    var onClose: (() -> Void)? = nil
    @StateObject private var runner = TLAttemptRunner()
    @ObservedObject private var store = TranslationLayerStore.shared
    @Environment(\.dismiss) private var dismiss
    @AppStorage("husk.tl.showLog") private var showLogSetting = false
    @AppStorage(TranslationLayer.devInfoKey) private var devInfo = false
    @State private var settings: TLAppSettings
    @State private var uiHidden: Bool
    @State private var hint = false
    @State private var showSwitcher = false
    private let cleanLayout: Bool
    private var showLog: Bool { get { showLogSetting && devInfo } nonmutating set { showLogSetting = newValue } }

    init(app: TLApp, onSwitch: ((TLApp) -> Void)? = nil, onClose: (() -> Void)? = nil) {
        self.app = app
        self.onSwitch = onSwitch
        self.onClose = onClose
        let loaded = TLAppSettings.load(app.id, packageName: app.packageName)
        _settings = State(initialValue: loaded)
        _uiHidden = State(initialValue: loaded.cleanView)
        cleanLayout = loaded.cleanView
    }

    private var switchCandidates: [TLApp] {
        store.apps.filter { $0.id != app.id && $0.packageName != GeodeSupport.launcherPackage }
    }

    private func closeScreen() {
        runner.stop()
        if let onClose { onClose() } else { dismiss() }
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
            if uiHidden {
                TLFloatingReopenHandle(action: toggleInterface)
            }
            if hint {
                VStack {
                    Spacer()
                    Text("Tap eye at top right or 3-finger tap to show controls")
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
        .huskHideSystemOverlays()
        .huskDefersSystemGestures(on: .all)
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
        .sheet(isPresented: $showSwitcher) {
            TLGameSwitcherSheet(current: app, apps: switchCandidates) { next in
                showSwitcher = false
                onSwitch?(next)
            }
        }
    }

    private var bar: some View {
        HStack(spacing: 12) {
            Button { closeScreen() } label: {
                Label("Close", systemImage: "xmark").font(.system(size: 13, weight: .semibold))
            }
            .tint(.white)
            if onSwitch != nil, !switchCandidates.isEmpty {
                Button { showSwitcher = true } label: {
                    Label("Switch", systemImage: "arrow.left.arrow.right").font(.system(size: 12, weight: .semibold))
                }
                .tint(.white)
            }
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

