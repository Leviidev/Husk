"""Exercise production import/completion/routing methods with real temporary files.

Only APK analysis, bundle extraction and unrelated app services are stubbed;
the actual add/ingest/IncomingFiles/Router methods are compiled unchanged.
"""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def method(source, signature):
    start = source.index(signature)
    end = source.index("\n    }", start) + len("\n    }")
    return source[start:end]


class ImportCompletionTests(unittest.TestCase):
    def test_completion_opens_exact_added_app_after_reload(self):
        store = (ROOT / "src/app/Husk/TranslationLayer.swift").read_text()
        router = (ROOT / "src/app/Husk/AppDetailView.swift").read_text()
        incoming = (ROOT / "src/app/Husk/IncomingFiles.swift").read_text()
        source = r'''import Foundation
import Combine

enum TranslationLayer {
    static let root = URL(fileURLWithPath: ProcessInfo.processInfo.environment["IMPORT_TEST_ROOT"]!)
}
enum HuskLog { static func log(_ a: String, _ b: String) {} }
enum BundleUnpacker {
    static let extensions: Set<String> = []
    static func unpack(_ url: URL, into: URL) throws -> [String] { [] }
}
struct App { let id: String }
enum Route: Equatable { case game(String) }
enum Tab { case home, store, library }
enum Filter { case all, games, ps4 }
@MainActor final class Router {
    static let shared = Router()
    var libraryImportID = UUID()
    var library: [Route] = [.game("old")]
    var libraryFilter = Filter.games
    var platform = Filter.ps4
    var showDownloads = true
    var showImport = true
    var tab = Tab.store
__ROUTER__
}
@MainActor final class TranslationLayerStore {
    static let shared = TranslationLayerStore()
    var busy: String?
    var lastError: String?
    var apps: [App] = []
    nonisolated static let scanVersion = 1
    func reload() {
        apps = ((try? FileManager.default.contentsOfDirectory(atPath: TranslationLayer.root.path)) ?? [])
            .filter { FileManager.default.fileExists(atPath: TranslationLayer.root.appendingPathComponent($0 + "/report.json").path) }
            .sorted().map { App(id: $0) }
    }
    func adoptDroppedAPKs() {}
    nonisolated static func orderedAPKs(_ files: [String]) -> [String] { files.sorted() }
    nonisolated static func describe(_ apks: [String], into dir: URL) {
        // An unrelated concurrent import must never become the destination of this one's completion.
        let other = TranslationLayer.root.appendingPathComponent("aaa-unrelated-" + UUID().uuidString)
        try! FileManager.default.createDirectory(at: other, withIntermediateDirectories: true)
        try! Data("{}".utf8).write(to: other.appendingPathComponent("report.json"))
    }
    nonisolated static func scan(_ apks: [String]) -> String { "{}" }
__ADD__
__INGEST__
__COPY__
}
@MainActor final class IncomingFiles {
    static let shared = IncomingFiles()
    var waiting: [URL] = []
__INCOMING__
}

@main struct ImportTests {
    @MainActor static func main() async throws {
        let fm = FileManager.default, store = TranslationLayerStore.shared
        let router = Router.shared, incoming = IncomingFiles.shared
        let input = TranslationLayer.root.appendingPathComponent("inputs")
        try fm.createDirectory(at: input, withIntermediateDirectories: true)
        let files = ["base.apk", "config.arm64_v8a.apk", "main.42.org.example.obb"].map { input.appendingPathComponent($0) }
        for file in files { try Data(file.lastPathComponent.utf8).write(to: file) }
        incoming.waiting = files
        let initialGeneration = router.libraryImportID
        incoming.sendToTranslationLayer()
        precondition(store.busy != nil && incoming.waiting.isEmpty)
        precondition(router.tab == .library && router.library.isEmpty, "Opened a card before import completed")
        precondition(!router.showDownloads && !router.showImport && router.libraryFilter == .all && router.platform == .all)
        precondition(router.libraryImportID != initialGeneration)
        while store.busy != nil { try await Task.sleep(nanoseconds: 1_000_000) }
        precondition(store.lastError == nil)
        guard case .game(let id)? = router.library.first else { preconditionFailure("Missing imported app card") }
        precondition(router.library.count == 1 && store.apps.contains { $0.id == id }, "Completion ran before reload")
        let dir = TranslationLayer.root.appendingPathComponent(id)
        precondition(!id.hasPrefix("aaa-unrelated-"), "Opened a different concurrently imported app")
        for path in ["base.apk", "config.arm64_v8a.apk", "obb/main.42.org.example.obb", "report.json"] {
            precondition(fm.fileExists(atPath: dir.appendingPathComponent(path).path), path)
        }
        precondition(files.allSatisfy { !fm.fileExists(atPath: $0.path) })

        incoming.waiting = [input.appendingPathComponent("missing.apk")]
        incoming.sendToTranslationLayer()
        while store.busy != nil { try await Task.sleep(nanoseconds: 1_000_000) }
        precondition(store.lastError != nil && router.library.isEmpty, "Failure opened a success card")

        let copy = input.appendingPathComponent("copy.apk")
        try Data("copy".utf8).write(to: copy)
        let copiedID = TranslationLayerStore.importCopy([copy])!
        precondition(fm.fileExists(atPath: TranslationLayer.root.appendingPathComponent(copiedID + "/copy.apk").path))
        precondition(fm.fileExists(atPath: copy.path))
        store.busy = "Another import"
        incoming.waiting = [copy]
        router.library = [.game(id)]
        incoming.sendToTranslationLayer()
        precondition(incoming.waiting == [copy] && router.library == [.game(id)], "Busy import consumed pending files")
        store.busy = nil; incoming.waiting = []
        incoming.sendToTranslationLayer()
        precondition(router.library == [.game(id)])
        print("PASS: completion after reload, exact app ID, split/OBB files, failure, copy import and busy/empty guards")
    }
}
'''
        for marker, code in {
            "__ADD__": method(store, "    func add(_ urls:"),
            "__INGEST__": method(store, "    nonisolated private static func ingest("),
            "__COPY__": method(store, "    nonisolated static func importCopy("),
            "__ROUTER__": method(router, "    func showLibraryAfterImport("),
            "__INCOMING__": method(incoming, "    func sendToTranslationLayer("),
        }.items():
            source = source.replace(marker, code)
        with tempfile.TemporaryDirectory(prefix="husk-import-completion-") as directory:
            work = pathlib.Path(directory)
            (work / "probe.swift").write_text(source)
            result = subprocess.run(["xcrun", "swiftc", "-parse-as-library", "-swift-version", "5",
                                     "-module-cache-path", str(work / "modules"), str(work / "probe.swift"),
                                     "-o", str(work / "probe")], capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            import os
            result = subprocess.run([str(work / "probe")], env={**os.environ, "IMPORT_TEST_ROOT": str(work / "library")},
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS:", result.stdout)


if __name__ == "__main__":
    unittest.main()
