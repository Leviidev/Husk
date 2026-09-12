// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import UIKit

/// The host half of the host/guest bridge.
///
/// Husk shares a folder with the guest over virtio-9p. Everything crossing the
/// boundary is a file: APKs go into `inbox/`, commands are JSON in `commands/`,
/// and the guest writes back `catalog/apps.json`, the icons beside it, and a
/// result per command.
///
/// File-based rather than a socket protocol on purpose. Every message survives as
/// something both sides can inspect afterwards, which on a phone with no debugger
/// attached is worth more than the latency a poll costs.
@MainActor
final class HuskBridgeFS: ObservableObject {
    static let shared = HuskBridgeFS()

    struct AndroidApp: Identifiable, Equatable {
        let package: String
        let name: String
        let iconPath: String?
        var id: String { package }
    }

    @Published private(set) var apps: [AndroidApp] = []
    @Published private(set) var lastAgentMessage: String?
    @Published private(set) var pendingInstalls: Set<String> = []

    private var timer: Timer?

    nonisolated var shareRoot: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("share", isDirectory: true)
    }
    nonisolated var inbox: URL    { shareRoot.appendingPathComponent("inbox", isDirectory: true) }
    nonisolated var commands: URL { shareRoot.appendingPathComponent("commands", isDirectory: true) }
    nonisolated var results: URL  { shareRoot.appendingPathComponent("results", isDirectory: true) }
    nonisolated var catalog: URL  { shareRoot.appendingPathComponent("catalog", isDirectory: true) }

    /// Create the shared tree before QEMU starts -- 9p exports a directory that
    /// has to already exist.
    nonisolated func prepare() {
        for dir in [shareRoot, inbox, commands, results, catalog] {
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        }
    }

    func startWatching() {
        guard timer == nil else { return }
        HuskLog.log("bridge", "watching \(catalog.path)")
        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
    }

    func stopWatching() {
        timer?.invalidate()
        timer = nil
    }

    private func poll() {
        loadCatalog()
        drainResults()
    }

    private func loadCatalog() {
        let file = catalog.appendingPathComponent("apps.json")
        guard let data = try? Data(contentsOf: file),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let list = obj["apps"] as? [[String: Any]] else { return }

        let parsed: [AndroidApp] = list.compactMap { entry in
            guard let pkg = entry["package"] as? String else { return nil }
            let name = (entry["name"] as? String) ?? pkg
            var icon: String?
            if let rel = entry["icon"] as? String {
                let p = catalog.appendingPathComponent(rel).path
                if FileManager.default.fileExists(atPath: p) { icon = p }
            }
            return AndroidApp(package: pkg, name: name, iconPath: icon)
        }
        .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }

        if parsed != apps {
            HuskLog.log("bridge", "catalog updated: \(parsed.count) apps")
            apps = parsed
        }
    }

    private func drainResults() {
        guard let files = try? FileManager.default.contentsOfDirectory(
            at: results, includingPropertiesForKeys: nil) else { return }
        for f in files where f.pathExtension == "json" {
            if let data = try? Data(contentsOf: f),
               let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
                let ok = (obj["ok"] as? Bool) ?? false
                let detail = (obj["detail"] as? String) ?? ""
                let id = (obj["id"] as? String) ?? f.lastPathComponent
                HuskLog.log("bridge", "result \(id): \(ok ? "ok" : "FAILED") \(detail)")
                if !ok, !detail.isEmpty { lastAgentMessage = detail }
                if id.hasPrefix("install-") {
                    pendingInstalls.remove(String(id.dropFirst("install-".count)))
                }
            }
            try? FileManager.default.removeItem(at: f)
        }
    }

    // MARK: - Sending

    /// Copy an APK into the inbox. The guest installs anything that appears there.
    func install(apkAt url: URL) {
        let name = url.lastPathComponent
        // Security-scoped: the document picker hands back a URL we only have
        // permission to read inside this pair of calls.
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }

        // Write beside the inbox and move into place, so the guest's poller can
        // never see a half-copied APK and try to install it.
        let staging = shareRoot.appendingPathComponent(".incoming-\(name)")
        let dest = inbox.appendingPathComponent(name)
        do {
            try? FileManager.default.removeItem(at: staging)
            try FileManager.default.copyItem(at: url, to: staging)
            try? FileManager.default.removeItem(at: dest)
            try FileManager.default.moveItem(at: staging, to: dest)
            pendingInstalls.insert(name)
            HuskLog.log("bridge", "queued \(name) for install "
                               + "(\((try? FileManager.default.attributesOfItem(atPath: dest.path)[.size] as? NSNumber)??.intValue ?? 0) bytes)")
        } catch {
            HuskLog.log("bridge", "FAILED to queue \(name): \(error.localizedDescription)")
            lastAgentMessage = "Could not add \(name): \(error.localizedDescription)"
        }
    }

    func launch(package: String) { send(["action": "launch", "package": package]) }
    func refreshCatalog()        { send(["action": "refresh"]) }

    private func send(_ payload: [String: Any]) {
        let id = "\(Int(Date().timeIntervalSince1970 * 1000))-\(UInt32.random(in: 0...9999))"
        let tmp = shareRoot.appendingPathComponent(".cmd-\(id)")
        let dest = commands.appendingPathComponent("\(id).json")
        do {
            let data = try JSONSerialization.data(withJSONObject: payload)
            try data.write(to: tmp)
            try FileManager.default.moveItem(at: tmp, to: dest)
            HuskLog.log("bridge", "sent \(payload)")
        } catch {
            HuskLog.log("bridge", "could not send \(payload): \(error.localizedDescription)")
        }
    }
}
