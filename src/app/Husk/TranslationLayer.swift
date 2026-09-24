// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit
import UniformTypeIdentifiers

/// Experimental: Android apps without booting Android.
///
/// Husk runs an APK by booting a whole Android system under QEMU and showing
/// one app's surface. That works, and it costs a kernel, an init, a system
/// server and minutes of emulated CPU before the first frame. Android
/// Translation Layer, on Linux, shows the other way: keep only the app's own
/// code -- its Dex and its native libraries -- and run it against a rewrite of
/// the Android framework on the host itself, so there is nothing to boot.
///
/// Doing that on iOS is a long road, and docs/04-translation-layer.md is the
/// map. What exists so far is the first stretch: apps can be added here and
/// each gets a report of what running it would take, and the phone can be
/// asked the questions the design rests on. Nothing opens an app yet, and
/// nothing here changes how the rest of Husk runs.
enum TranslationLayer {
    static let enabledKey = "husk.translationLayer"

    static var isEnabled: Bool { UserDefaults.standard.bool(forKey: enabledKey) }

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

    func reload() {
        let dirs = (try? FileManager.default.contentsOfDirectory(
            at: TranslationLayer.root, includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles])) ?? []
        apps = dirs.compactMap(Self.load).sorted {
            $0.label.localizedCaseInsensitiveCompare($1.label) == .orderedAscending
        }
    }

    /// Keep a copy of one app -- one APK, or a base APK and its splits,
    /// picked together -- and report on it.
    func add(_ urls: [URL]) {
        guard !urls.isEmpty, busy == nil else { return }
        busy = urls.count == 1 ? "Adding \(urls[0].lastPathComponent)…"
                               : "Adding \(urls.count) APKs…"
        Task.detached(priority: .userInitiated) {
            let failure = Self.ingest(urls)
            await MainActor.run {
                self.busy = nil
                self.lastError = failure
                self.reload()
            }
        }
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
        let apks = files.filter { $0.lowercased().hasSuffix(".apk") }.sorted()
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
    nonisolated private static func ingest(_ urls: [URL]) -> String? {
        let fm = FileManager.default
        let dir = TranslationLayer.root.appendingPathComponent(UUID().uuidString,
                                                               isDirectory: true)
        do {
            try fm.createDirectory(at: dir, withIntermediateDirectories: true)
            for url in urls {
                // Security-scoped: the picker's URL is only readable inside this pair.
                let scoped = url.startAccessingSecurityScopedResource()
                defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                var name = url.lastPathComponent
                if !name.lowercased().hasSuffix(".apk") { name += ".apk" }
                try fm.copyItem(at: url, to: dir.appendingPathComponent(name))
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

struct TranslationLayerSettings: View {
    @ObservedObject private var store = TranslationLayerStore.shared
    @State private var enabled = TranslationLayer.isEnabled
    @State private var importing = false

    var body: some View {
        Form {
            Section {
                Toggle("Android Translation Layer", isOn: $enabled)
                    .onChange(of: enabled) { v in
                        UserDefaults.standard.set(v, forKey: TranslationLayer.enabledKey)
                        HuskLog.log("ui", v ? "translation layer on" : "translation layer off")
                    }
            } header: {
                Text("Experimental")
            } footer: {
                Text("Runs an app's own code directly, against a rewrite of Android's "
                   + "framework, instead of booting a whole Android system -- the "
                   + "approach of Android Translation Layer on Linux, rebuilt for iOS. "
                   + "It cannot open apps yet. For now it reports what each app would "
                   + "need, and checks this iPhone for what the design depends on. "
                   + "Android itself is unaffected either way.")
            }

            if enabled {
                appsSection
                checksSection
                progressSection
            }
        }
        .huskForm()
        .navigationTitle("Translation Layer")
        .fileImporter(isPresented: $importing, allowedContentTypes: [.item],
                      allowsMultipleSelection: true) { result in
            if case .success(let urls) = result, !urls.isEmpty {
                HuskLog.log("ui", "translation layer: adding \(urls.count) file(s): "
                          + urls.map(\.lastPathComponent).joined(separator: ", "))
                store.add(urls)
            }
        }
        .alert("Could not add the app", isPresented: Binding(
                get: { store.lastError != nil },
                set: { if !$0 { store.lastError = nil } })) {
            Button("OK", role: .cancel) { store.lastError = nil }
        } message: {
            Text(store.lastError ?? "")
        }
    }

    private var appsSection: some View {
        Section {
            ForEach(store.apps) { app in
                NavigationLink {
                    TLAppReportView(app: app)
                } label: {
                    TLAppRow(app: app)
                }
            }
            if let busy = store.busy {
                HStack(spacing: 10) {
                    ProgressView()
                    Text(busy).foregroundStyle(Theme.textDim)
                }
            }
            Button {
                importing = true
            } label: {
                Label("Add APK", systemImage: "plus")
            }
            .disabled(store.busy != nil)
        } header: {
            Text("Apps")
        } footer: {
            Text("Husk keeps its own copy, apart from Android's. Pick a base APK and "
               + "its split pieces together to add them as one app.")
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
            DetailRow(label: "Library loader", value: "not started", mono: false)
            DetailRow(label: "Android runtime", value: "not started", mono: false)
            DetailRow(label: "Opening apps", value: "not yet", mono: false)
        } header: {
            Text("Where it stands")
        } footer: {
            Text("The plan, in order, is docs/04-translation-layer.md in Husk's source.")
        }
    }
}

/// A verdict, as words and a colour.
struct TLVerdict {
    let title: String
    let tint: Color

    init(_ report: TLReport?) {
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

private struct TLAppRow: View {
    let app: TLApp

    var body: some View {
        let verdict = TLVerdict(app.report)
        HStack(spacing: 12) {
            AppIcon(path: app.iconPath, size: 36)
            VStack(alignment: .leading, spacing: 3) {
                Text(app.label).foregroundStyle(Theme.text).lineLimit(1)
                Text(verdict.title)
                    .font(.system(size: 12))
                    .foregroundStyle(verdict.tint)
            }
        }
        .padding(.vertical, 2)
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

    var body: some View {
        let verdict = TLVerdict(app.report)
        Form {
            Section {
                HStack(spacing: 14) {
                    AppIcon(path: app.iconPath, size: 56)
                    VStack(alignment: .leading, spacing: 4) {
                        Text(app.label)
                            .font(.system(size: 20, weight: .semibold))
                            .foregroundStyle(Theme.text)
                        Text(verdict.title)
                            .font(.system(size: 13, weight: .medium))
                            .foregroundStyle(verdict.tint)
                    }
                }
                .padding(.vertical, 4)
                if let report = app.report {
                    Text(report.summary)
                        .font(.system(size: 14))
                        .foregroundStyle(Theme.text)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }

            if let report = app.report {
                Section {
                    if let engine = report.engine {
                        DetailRow(label: "Made with", value: engine, mono: false)
                    }
                    DetailRow(label: "Dex", value: dex(report), mono: false)
                    DetailRow(label: "ABIs", value: report.abis.isEmpty
                              ? "none" : report.abis.joined(separator: ", "))
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
                        Text("Its own libraries link against these, and it does not "
                           + "carry them. Each is something the translation layer has "
                           + "to provide.")
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
                Text("Deletes Husk's copy of the APKs. Anything installed in Android is "
                   + "untouched.")
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
    }

    private func dex(_ report: TLReport) -> String {
        let files = report.dexCount == 1 ? "1 file" : "\(report.dexCount) files"
        let size = ByteCountFormatter.string(fromByteCount: report.dexBytes, countStyle: .file)
        return "\(files), \(size)"
    }
}

private struct TLLibraryRows: View {
    let lib: TLLibrary

    private var statusTitle: String {
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
            DetailRow(label: "Thread-local storage", value: "yes", mono: false)
        }
    }
}
