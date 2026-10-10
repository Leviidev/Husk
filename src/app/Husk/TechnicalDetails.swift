// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

extension TLReport {
    /// Whether Husk will try to run it at all. A game with an engine Husk has a driver for runs on the native runtime; one
    /// without (Flappy Bird is plain Java with a couple of AndroidX helper libraries) runs on Husk's own Java interpreter.
    /// Only an APK with no 64-bit code, or one Husk could not read, is out of reach.
    var canRun: Bool {
        runsOnNativeRuntime || ["java", "native", "nativeWithWork"].contains(verdict)
    }

    /// What runs it, in words.
    var runnerName: String {
        if runsOnNativeRuntime { return nativeEngineName }
        return canRun ? "Java (Husk's interpreter)" : "None"
    }
}

extension TLApp {
    /// The folder the game keeps what it saves in, beside its APKs.
    var dataDirectory: URL {
        let name: String
        switch report?.nativeEngine {
        case .minecraft?: name = "minecraft-data"
        case .sdl?: name = "sdl-data"
        case .ue4?: name = "ue4-data"
        case .gta?: name = "gta-data"
        case .godot?: name = "godot-data"
        case .nativeactivity?: name = "na-data"
        case .flutter?: name = "flutter-data"
        case .cocos?: name = "cocos-data"
        case nil: name = "classic-data"
        default: name = "unity-data"
        }
        return TranslationLayer.root.appendingPathComponent(id, isDirectory: true).appendingPathComponent(name, isDirectory: true)
    }
}

/// What a game is, technically: how Husk runs it, what is in the APK, how its native libraries load, and its last run's log.
struct TLTechnicalView: View {
    let app: TLApp

    @State private var log: String?
    @State private var dataSize = "…"
    @State private var sharing = false

    private var report: TLReport? { app.report }
    private var logURL: URL { app.dataDirectory.appendingPathComponent("native-run.log") }
    private var arm64: [TLLibrary] { (report?.libraries ?? []).filter { $0.abi == "arm64-v8a" } }

    var body: some View {
        List {
            Section("Runtime") {
                row("Runs With", report?.runnerName ?? "Not scanned")
                row("Package", app.packageName ?? "—", mono: true)
                row("Architectures", report.map { $0.abis.isEmpty ? "None (Java only)" : $0.abis.joined(separator: ", ") } ?? "—")
                if let summary = report?.displaySummary {
                    Text(summary).font(.footnote).foregroundStyle(.secondary)
                }
            }

            Section("Package") {
                ForEach(app.apks, id: \.self) { apk in
                    row((apk as NSString).lastPathComponent, size(of: apk), mono: true)
                }
                if let r = report {
                    row("Java Code", r.dexCount == 0 ? "None" : "\(r.dexCount) DEX file\(r.dexCount == 1 ? "" : "s"), \(bytes(r.dexBytes))")
                    row("Android Libraries Used", "\(r.systemLibraries.count)")
                }
                row("Saved Data", dataSize)
            }

            if !arm64.isEmpty {
                Section {
                    ForEach(arm64) { lib in
                        VStack(alignment: .leading, spacing: 4) {
                            HStack {
                                Text(lib.name).font(.system(.subheadline, design: .monospaced)).lineLimit(1).truncationMode(.middle)
                                Spacer(minLength: 8)
                                Text(bytes(lib.bytes)).font(.caption).foregroundStyle(.secondary)
                            }
                            Text(libraryDetail(lib)).font(.caption).foregroundStyle(lib.status == "ok" ? Color.secondary : Color.orange)
                        }
                        .padding(.vertical, 2)
                    }
                } header: {
                    Text("Native Libraries (\(arm64.count))")
                } footer: {
                    Text("How each 64-bit library maps onto the iPhone: its relocations, the instructions Husk rewrites, and anything the loader works around.")
                }
            }

            Section {
                if let log, !log.isEmpty {
                    ScrollView(.horizontal) {
                        Text(log)
                            .font(.system(size: 10, design: .monospaced))
                            .textSelection(.enabled)
                            .fixedSize(horizontal: true, vertical: false)
                            .padding(.vertical, 4)
                    }
                    .frame(maxHeight: 320)
                    Button { sharing = true } label: { Label("Share Log", systemImage: "square.and.arrow.up") }
                } else {
                    Text(log == nil ? "Reading…" : "No log yet. It is written the first time the game runs.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            } header: {
                Text("Last Run")
            } footer: {
                Text("The end of the log from the last time the game started. Share it when reporting a problem.")
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle("Technical Details")
        .navigationBarTitleDisplayMode(.inline)
        .task {
            let url = logURL, dir = app.dataDirectory
            let (text, total) = await Task.detached(priority: .utility) { () -> (String, Int64) in
                let text = (try? String(contentsOf: url, encoding: .utf8)).map { full -> String in
                    let lines = full.split(separator: "\n", omittingEmptySubsequences: false)
                    return lines.suffix(200).joined(separator: "\n")
                } ?? ""
                var total: Int64 = 0
                let walker = FileManager.default.enumerator(at: dir, includingPropertiesForKeys: [.fileSizeKey])
                while let file = walker?.nextObject() as? URL {
                    total += Int64((try? file.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0)
                }
                return (text, total)
            }.value
            log = text
            dataSize = total == 0 ? "Nothing yet" : bytes(total)
        }
        .sheet(isPresented: $sharing) { ShareSheet(items: [logURL]) }
    }

    private func row(_ label: String, _ value: String, mono: Bool = false) -> some View {
        LabeledContent {
            Text(value)
                .font(mono ? .system(.footnote, design: .monospaced) : .body)
                .multilineTextAlignment(.trailing)
                .textSelection(.enabled)
        } label: {
            Text(label).lineLimit(2)
        }
    }

    private func libraryDetail(_ lib: TLLibrary) -> String {
        var parts: [String] = []
        if let r = lib.relocations { parts.append("\(r) relocations") }
        if let t = lib.tpidrReads, t > 0 { parts.append("\(t) thread-register reads") }
        if let s = lib.svc, s > 0 { parts.append("\(s) system calls") }
        if let c = lib.conflictPages, c > 0 { parts.append("\(c) shared code/data pages") }
        if lib.tls == true { parts.append("thread-local storage") }
        if !lib.notes.isEmpty { parts.append(contentsOf: lib.notes) }
        return parts.isEmpty ? (lib.status == "ok" ? "Loads as it is" : lib.status) : parts.joined(separator: " · ")
    }

    private func size(of path: String) -> String {
        bytes(((try? FileManager.default.attributesOfItem(atPath: path)[.size]) as? NSNumber)?.int64Value ?? 0)
    }

    private func bytes(_ n: Int64) -> String { ByteCountFormatter.string(fromByteCount: n, countStyle: .file) }
}
