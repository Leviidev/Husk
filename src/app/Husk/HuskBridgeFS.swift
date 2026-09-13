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

    private var lastDiagnosticsStamp: Date?

    private func poll() {
        loadCatalog()
        drainResults()
        loadDiagnostics()
    }

    /// The guest writes a full state dump into the share whenever something looks
    /// wrong. Folding it into Husk's log means the user never has to open a shell
    /// and run systemctl to tell us what happened.
    private func loadDiagnostics() {
        let file = shareRoot.appendingPathComponent("diagnostics.txt")
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: file.path),
              let modified = attrs[.modificationDate] as? Date else { return }
        if let last = lastDiagnosticsStamp, last >= modified { return }
        lastDiagnosticsStamp = modified

        guard let text = try? String(contentsOf: file, encoding: .utf8) else { return }
        HuskLog.log("diag", "---- guest diagnostics ----")
        for line in text.split(separator: "\n", omittingEmptySubsequences: false) {
            let t = line.trimmingCharacters(in: .whitespaces)
            if !t.isEmpty { HuskLog.log("diag", String(t.prefix(300))) }
        }
        HuskLog.log("diag", "---- end diagnostics ----")
    }

    /// Ask the guest to re-dump its state.
    func requestDiagnostics() { send(["action": "diagnostics"]) }

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

// MARK: - Guest bridge

/// A shell inside the guest, reached without adbd's cooperation.
///
/// ADB was the obvious control plane and it does not work here. LineageOS runs
/// adbd in the `adbd_tradeinmode` SELinux domain until the device has been
/// through setup, and in that domain every shell request is refused -- so the
/// channel needed to provision the device is the one provisioning would unlock.
/// Marking the device provisioned from init did not help either: adbd decides
/// once, at start, and by then the property is not yet set.
///
/// So Husk stopped asking adbd. The guest image carries an init service that,
/// on `sys.boot_completed`, runs a plain netcat listener on port 5599 whose
/// child is `/system/bin/sh`, declared `seclabel u:r:shell:s0` -- the same
/// domain and the same authority `adb shell` would have given us:
///
///     uid=2000(shell) ... context=u:r:shell:s0
///
/// QEMU's user networking forwards 127.0.0.1:5599 into it. What arrives here is
/// a shell, so there is no protocol to implement: write a command, read what it
/// prints. Every call opens its own connection, because the listener spawns a
/// fresh shell per connection and one command can therefore never inherit
/// another's environment, working directory or half-read stdin.
enum BridgeError: LocalizedError {
    case io(String)
    case timeout(String)

    var errorDescription: String? {
        switch self {
        case .io(let m):      return m
        case .timeout(let m): return "timed out \(m)"
        }
    }
}

final class GuestBridge {
    static let shared = GuestBridge()

    /// Matches the hostfwd in QemuRunner's -netdev.
    private static let port: UInt16 = 5599

    /// Printed after a command so we know where its output ends.
    ///
    /// The shell never closes the connection between commands by itself -- it
    /// waits for more input -- so "read until EOF" would hang forever. The
    /// marker carries the exit status with it, which is the only other thing
    /// worth knowing.
    private static let marker = "__HUSK_EOF__"

    /// True once any command has succeeded. Used by the UI to decide whether the
    /// library is usable, and cleared whenever a connection fails.
    private(set) var isConnected = false

    // MARK: sockets

    private func openSocket(timeout: TimeInterval = 10) throws -> Int32 {
        let fd = socket(AF_INET, SOCK_STREAM, 0)
        guard fd >= 0 else { throw BridgeError.io("socket() failed (errno \(errno))") }

        var on: Int32 = 1
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, socklen_t(MemoryLayout<Int32>.size))
        var tv = timeval(tv_sec: Int(timeout), tv_usec: 0)
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))

        var addr = sockaddr_in()
        addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = Self.port.bigEndian
        addr.sin_addr.s_addr = inet_addr("127.0.0.1")

        let rc = withUnsafePointer(to: &addr) { raw in
            raw.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                connect(fd, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        if rc != 0 {
            let e = errno
            close(fd)
            isConnected = false
            throw BridgeError.io("connect failed (errno \(e))")
        }
        return fd
    }

    private func writeAll(_ fd: Int32, _ data: Data) throws {
        try data.withUnsafeBytes { (raw: UnsafeRawBufferPointer) in
            var off = 0
            while off < raw.count {
                let n = write(fd, raw.baseAddress!.advanced(by: off), raw.count - off)
                if n <= 0 {
                    if n < 0 && errno == EINTR { continue }
                    throw BridgeError.io("write failed (errno \(errno))")
                }
                off += n
            }
        }
    }

    // MARK: commands

    /// Run one command and return everything it wrote.
    ///
    /// stderr is folded into stdout: the listener wires only stdin and stdout to
    /// the socket, so anything on stderr would otherwise vanish into the guest's
    /// kernel log -- including the error messages that explain a failure.
    @discardableResult
    func shell(_ command: String, timeout: TimeInterval = 30) throws -> String {
        let fd = try openSocket(timeout: timeout)
        defer { close(fd) }

        try writeAll(fd, Data("\(command) 2>&1; echo \(Self.marker)$?\n".utf8))

        var out = ""
        var buf = [UInt8](repeating: 0, count: 16 * 1024)
        let deadline = Date().addingTimeInterval(timeout)
        while true {
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, $0.count) }
            if n < 0 && errno == EINTR { continue }
            if n <= 0 {
                isConnected = false
                throw BridgeError.io("guest closed the connection (errno \(errno))")
            }
            out += String(decoding: buf[0..<n], as: UTF8.self)
            if let r = out.range(of: Self.marker) {
                let status = Int(out[r.upperBound...]
                    .trimmingCharacters(in: .whitespacesAndNewlines)) ?? 0
                isConnected = true
                let body = String(out[out.startIndex..<r.lowerBound])
                if status != 0 {
                    HuskLog.log("bridge", "`\(command.prefix(60))` exit \(status)")
                }
                return body
            }
            if Date() > deadline {
                throw BridgeError.timeout("waiting for `\(command.prefix(60))`")
            }
        }
    }

    /// Run one command and return both its output and its exit status.
    ///
    /// `shell` discards the status, which is fine for reads and fatal for
    /// anything whose failure has to stop what follows.
    func run(_ command: String, timeout: TimeInterval = 30) throws -> (out: String, status: Int) {
        let fd = try openSocket(timeout: timeout)
        defer { close(fd) }
        try writeAll(fd, Data("\(command) 2>&1; echo \(Self.marker)$?\n".utf8))

        var out = ""
        var buf = [UInt8](repeating: 0, count: 16 * 1024)
        let deadline = Date().addingTimeInterval(timeout)
        while true {
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, $0.count) }
            if n < 0 && errno == EINTR { continue }
            if n <= 0 {
                isConnected = false
                throw BridgeError.io("guest closed the connection (errno \(errno))")
            }
            out += String(decoding: buf[0..<n], as: UTF8.self)
            if let r = out.range(of: Self.marker) {
                isConnected = true
                let status = Int(out[r.upperBound...]
                    .trimmingCharacters(in: .whitespacesAndNewlines)) ?? -1
                return (String(out[out.startIndex..<r.lowerBound]), status)
            }
            if Date() > deadline { throw BridgeError.timeout("waiting for `\(command.prefix(60))`") }
        }
    }

    /// Copy a local file into the guest.
    ///
    /// The data goes down its own connection rather than being quoted into a
    /// command, because APKs are binary and megabytes long. The shell is told to
    /// `exec` the receiving program, so once it acknowledges, nothing but the
    /// file's own bytes are on the wire and no shell parsing is involved.
    ///
    /// The acknowledgement matters. A shell reading commands from a socket may
    /// buffer ahead, and anything it swallows that way never reaches the
    /// program that replaces it -- so the file is sent only after the guest has
    /// answered, when there is provably nothing left for the shell to read.
    func push(_ local: URL, to remote: String,
              progress: @escaping (Double) -> Void) throws {
        let size = (try FileManager.default.attributesOfItem(atPath: local.path)[.size]
                    as? NSNumber)?.intValue ?? 0
        guard size > 0 else { throw BridgeError.io("\(local.lastPathComponent) is empty") }

        let handle = try FileHandle(forReadingFrom: local)
        defer { try? handle.close() }

        // Generous: a hundred-megabyte APK over an emulated NIC is not quick.
        let fd = try openSocket(timeout: 120)
        defer { close(fd) }

        let ready = "__HUSK_RDY__"
        try writeAll(fd, Data("echo \(ready); exec head -c \(size) > \(remote)\n".utf8))

        var seen = ""
        var buf = [UInt8](repeating: 0, count: 1024)
        while !seen.contains(ready) {
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, $0.count) }
            if n < 0 && errno == EINTR { continue }
            if n <= 0 { throw BridgeError.io("guest did not accept the file") }
            seen += String(decoding: buf[0..<n], as: UTF8.self)
        }

        var sent = 0
        while sent < size {
            guard let chunk = try handle.read(upToCount: 256 * 1024), !chunk.isEmpty else { break }
            try writeAll(fd, chunk)
            sent += chunk.count
            progress(Double(sent) / Double(size))
        }
        // head exits on its own after `size` bytes; the shutdown is what frees it
        // if the file was shorter than its own metadata claimed.
        shutdown(fd, SHUT_WR)
        HuskLog.log("bridge", "pushed \(sent) bytes to \(remote)")
        if sent != size { throw BridgeError.io("sent \(sent) of \(size) bytes") }
    }

    func disconnect() { isConnected = false }
}

// MARK: - Android host

/// The running guest, seen as something apps can be installed into and started.
///
/// Readiness is polled rather than assumed: the bridge only exists once init
/// has seen `sys.boot_completed`, and on a restored snapshot that is immediate
/// while on a cold boot it is minutes away. The UI shows that waiting honestly
/// instead of pretending the library is usable.
@MainActor
final class AndroidHost: ObservableObject {
    static let shared = AndroidHost()

    struct Package: Identifiable, Equatable {
        var id: String { name }
        let name: String
        var label: String
    }

    @Published private(set) var isReady = false
    @Published private(set) var status = "Starting Android…"
    @Published private(set) var packages: [Package] = []
    @Published private(set) var busy: String?

    private var polling = false

    /// Poll until the guest's shell answers and Android reports it has booted.
    func waitForReady() {
        guard !polling, !isReady else { return }
        polling = true
        status = "Starting Android…"
        Task.detached { [weak self] in
            var attempt = 0
            while true {
                attempt += 1
                do {
                    // One round trip proves the whole path: the forward, the
                    // listener, and that the shell it spawned can execute
                    // something. Logged the first time and then occasionally,
                    // because when this never succeeds the answer is always in
                    // what the guest said rather than in the fact that it failed.
                    if attempt == 1 || attempt % 10 == 0 {
                        let who = (try? GuestBridge.shared.shell("id")) ?? "(no answer)"
                        HuskLog.log("bridge", "guest shell: "
                                  + who.trimmingCharacters(in: .whitespacesAndNewlines))
                    }
                    let booted = try GuestBridge.shared.shell("getprop sys.boot_completed")
                        .trimmingCharacters(in: .whitespacesAndNewlines)
                    if booted == "1" {
                        await MainActor.run {
                            self?.isReady = true
                            self?.status = "Android is ready"
                            self?.polling = false
                        }
                        HuskLog.log("bridge", "guest is ready after \(attempt) attempts")
                        await self?.refreshPackages()
                        await MainActor.run { self?.dumpDiagnostics() }
                        return
                    }
                    await MainActor.run { self?.status = "Android is booting…" }
                } catch {
                    GuestBridge.shared.disconnect()
                    await MainActor.run {
                        self?.status = attempt < 4 ? "Starting Android…"
                                                   : "Waiting for Android (\(attempt * 3)s)…"
                    }
                }
                try? await Task.sleep(nanoseconds: 3_000_000_000)
            }
        }
    }

    /// Installed third-party packages -- the things a person actually put there.
    func refreshPackages() async {
        do {
            let raw = try GuestBridge.shared.shell("pm list packages -3", timeout: 60)
            let names = raw.split(separator: "\n")
                .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
                .filter { $0.hasPrefix("package:") }
                .map { String($0.dropFirst("package:".count)) }
                .filter { !$0.isEmpty }
            await MainActor.run {
                self.packages = names.map { Package(name: $0, label: Self.pretty($0)) }
            }
            HuskLog.log("bridge", "\(names.count) user package(s) installed")
        } catch {
            HuskLog.log("bridge", "could not list packages: \(error.localizedDescription)")
        }
    }

    /// "com.dotgears.flappybird" reads better as "Flappybird" until we can ask
    /// Android for the real label.
    private static func pretty(_ pkg: String) -> String {
        (pkg.split(separator: ".").last.map(String.init) ?? pkg).capitalized
    }

    /// Copy an APK into the guest and install it.
    func install(_ apk: URL) {
        let name = apk.lastPathComponent
        busy = "Installing \(name)…"
        Task.detached { [weak self] in
            // /data/local/tmp is the one directory the shell user owns outright,
            // and the one pm will read an APK from.
            let remote = "/data/local/tmp/husk-install.apk"
            do {
                // A file handed over by the document picker lives outside the
                // sandbox and is unreadable until this is claimed.
                let scoped = apk.startAccessingSecurityScopedResource()
                defer { if scoped { apk.stopAccessingSecurityScopedResource() } }

                try GuestBridge.shared.push(apk, to: remote) { p in
                    Task { @MainActor in
                        self?.busy = "Copying \(name) — \(Int(p * 100))%"
                    }
                }
                // Confirm the whole file landed before asking pm to parse it.
                // A short copy fails much later and much less clearly, as
                // "Failed to parse /data/local/tmp/husk-install.apk".
                let expected = (try FileManager.default
                    .attributesOfItem(atPath: apk.path)[.size] as? NSNumber)?.intValue ?? 0
                let landed = Int(try GuestBridge.shared.shell("wc -c < \(remote)")
                    .trimmingCharacters(in: .whitespacesAndNewlines)) ?? -1
                guard landed == expected else {
                    throw BridgeError.io("copied \(landed) of \(expected) bytes")
                }

                await MainActor.run { self?.busy = "Installing \(name)…" }
                // Installing is dex2oat's work and it is emulated, so minutes
                // rather than seconds for anything large. -t allows test-signed
                // APKs, which most sideloaded builds are.
                let out = try GuestBridge.shared.shell("pm install -r -t \(remote)",
                                                      timeout: 600)
                _ = try? GuestBridge.shared.shell("rm -f \(remote)")
                let ok = out.contains("Success")
                HuskLog.log("bridge", "install \(name): "
                          + out.trimmingCharacters(in: .whitespacesAndNewlines))
                await self?.refreshPackages()

                // Persist it, or it is gone on the next launch.
                //
                // Husk restores a saved machine instead of booting, and
                // restoring rewinds the userdata disk to the state the snapshot
                // was taken in. An app installed after that point lives only in
                // this session: the library lists it now and will not list it
                // tomorrow. Saving over the snapshot is what makes the install
                // real, so it is part of installing rather than a thing to
                // remember to do afterwards.
                if ok {
                    await MainActor.run {
                        self?.busy = QemuRunner.glProven
                            ? "Installed. GPU mode cannot save yet, so this app is gone "
                            + "when you relaunch."
                            : "Saving Android — the screen will freeze briefly"
                        QemuRunner.shared.saveState(reason: "installed \(name)") { saved in
                            self?.busy = nil
                            if !saved {
                                HuskLog.log("bridge", "\(name) is installed but the machine "
                                          + "was not saved; it will be gone next launch")
                            }
                        }
                    }
                } else {
                    await MainActor.run {
                        self?.busy = "Install failed: \(out.prefix(120))"
                        Task { try? await Task.sleep(nanoseconds: 4_000_000_000)
                               await MainActor.run { self?.busy = nil } }
                    }
                }
            } catch {
                HuskLog.log("bridge", "install failed: \(error.localizedDescription)")
                await MainActor.run { self?.busy = "Install failed: \(error.localizedDescription)" }
                Task { try? await Task.sleep(nanoseconds: 5_000_000_000)
                       await MainActor.run { self?.busy = nil } }
            }
        }
    }

    /// Ask the guest what it is actually doing, once, and put the answers in
    /// the log.
    ///
    /// Every machine-shape lever -- CPU model, vCPU count, RAM, the display
    /// device -- is frozen by the snapshot, so changing any of them costs a
    /// rebuilt snapshot and a three-gigabyte download for everyone. That is far
    /// too expensive to spend on a guess about where the frames are going.
    /// These answers are what makes the next change a decision instead.
    func dumpDiagnostics() {
        Task.detached {
            let probes: [(String, String)] = [
                ("egl driver",   "getprop ro.hardware.egl"),
                ("gralloc",      "getprop ro.hardware.gralloc"),
                ("hwui",         "getprop debug.hwui.renderer"),
                ("memtag",       "getprop ro.arm64.memtag.bootctl"),
                ("cpu features", "grep -m1 Features /proc/cpuinfo"),
                ("display size", "wm size"),
                ("density",      "wm density"),
                // The renderer is the whole question: a hardware GL string
                // means the guest found a GPU, and anything mentioning
                // SwiftShader or llvmpipe means every pixel is being drawn by
                // an emulated CPU.
                ("renderer",     "dumpsys SurfaceFlinger | grep -i -m3 'GLES\\|renderer'"),
            ]
            for (label, cmd) in probes {
                let out = (try? GuestBridge.shared.shell(cmd, timeout: 45)) ?? "(failed)"
                HuskLog.log("probe", "\(label): "
                          + out.trimmingCharacters(in: .whitespacesAndNewlines)
                               .replacingOccurrences(of: "\n", with: " | "))
            }
        }
    }

    /// Make Android draw fewer pixels.
    ///
    /// The scanout stays 360x800 because the snapshot pinned it, but `wm size`
    /// changes the *logical* display, so apps and the compositor render at the
    /// smaller size and SurfaceFlinger scales the result up. Software
    /// rasterisation costs what the pixel count costs, and none of this needs a
    /// new snapshot -- which is the whole reason it is worth trying first.
    ///
    /// Density moves with it, or every app lays out for a screen that is no
    /// longer there and the UI ends up cropped.
    func setRenderScale(_ scale: Double, then: @escaping () -> Void = {}) {
        busy = scale >= 1 ? "Restoring full resolution…" : "Reducing render size…"
        Task.detached { [weak self] in
            let base = GuestImage.shared.snapshotPins
            defer { Task { @MainActor in self?.busy = nil; then() } }

            // Physical density, so the override keeps the same physical scale.
            let densityOut = (try? GuestBridge.shared.shell("wm density")) ?? ""
            let physical = densityOut
                .split(separator: "\n")
                .compactMap { line -> Int? in
                    guard line.contains("Physical density") else { return nil }
                    return Int(line.split(separator: ":").last?
                        .trimmingCharacters(in: .whitespaces) ?? "")
                }.first ?? 240

            if scale >= 1 {
                _ = try? GuestBridge.shared.shell("wm size reset; wm density reset", timeout: 60)
                HuskLog.log("perf", "render size reset to \(base.xres)x\(base.yres)")
            } else {
                // Rounded to even numbers: odd widths give SurfaceFlinger a
                // half-pixel scale factor and a blurrier result than the size
                // reduction is worth.
                let w = max(240, Int((Double(base.xres) * scale / 2).rounded()) * 2)
                let h = max(480, Int((Double(base.yres) * scale / 2).rounded()) * 2)
                let d = max(120, Int((Double(physical) * scale).rounded()))
                _ = try? GuestBridge.shared.shell("wm size \(w)x\(h); wm density \(d)",
                                                  timeout: 60)
                HuskLog.log("perf", "render size \(w)x\(h) density \(d) "
                          + "(\(Int(scale * 100))% of \(base.xres)x\(base.yres)); "
                          + "\(Int((1 - scale * scale) * 100))% fewer pixels to rasterise")
            }

            // Animations are pure compositor work and buy nothing at 12 fps.
            for key in ["window_animation_scale", "transition_animation_scale",
                        "animator_duration_scale"] {
                _ = try? GuestBridge.shared.shell("settings put global \(key) 0")
            }
        }
    }

    /// Start an app by package name.
    ///
    /// monkey rather than `am start`, because it finds the launcher activity on
    /// its own -- we do not know the activity name and would have to resolve it.
    func launch(_ pkg: String, then: @escaping () -> Void) {
        busy = "Opening…"
        Task.detached { [weak self] in
            let out = (try? GuestBridge.shared.shell(
                "monkey -p \(pkg) -c android.intent.category.LAUNCHER 1", timeout: 60)) ?? ""
            HuskLog.log("bridge", "launch \(pkg): \(out.split(separator: "\n").last ?? "")")
            await MainActor.run { self?.busy = nil; then() }
        }
    }
}
