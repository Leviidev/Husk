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

// MARK: - ADB

/// A minimal ADB client, speaking the protocol directly over the forwarded port.
///
/// The QEMU command line maps 127.0.0.1:5555 into the guest, and the guest image
/// now sets `service.adb.tcp.port=5555` with `ro.adb.secure=0`, so adbd listens
/// there and asks for no key. That is the whole reason for the custom guest
/// image: this channel is how APKs get in and how apps get launched.
///
/// Only the parts Husk needs are implemented -- connect, run a shell command,
/// push a file. Not a general ADB implementation.
final class Adb {
    static let shared = Adb()

    private let queue = DispatchQueue(label: "husk.adb")
    private var fd: Int32 = -1
    private var nextLocalId: UInt32 = 1

    private enum Cmd: UInt32 {
        case cnxn = 0x4e584e43, open = 0x4e45504f, okay = 0x59414b4f
        case clse = 0x45534c43, wrte = 0x45545257, auth = 0x48545541
    }

    var isConnected: Bool { fd >= 0 }

    // MARK: framing

    private func send(_ cmd: Cmd, _ arg0: UInt32, _ arg1: UInt32, _ payload: Data = Data()) throws {
        var header = Data()
        func put(_ v: UInt32) { withUnsafeBytes(of: v.littleEndian) { header.append(contentsOf: $0) } }
        put(cmd.rawValue); put(arg0); put(arg1); put(UInt32(payload.count))
        // The checksum is a plain byte sum, not a CRC, despite the field name.
        put(payload.reduce(UInt32(0)) { $0 &+ UInt32($1) })
        put(cmd.rawValue ^ 0xffff_ffff)
        try writeAll(header)
        if !payload.isEmpty { try writeAll(payload) }
    }

    private func writeAll(_ data: Data) throws {
        try data.withUnsafeBytes { (raw: UnsafeRawBufferPointer) in
            var off = 0
            while off < raw.count {
                let n = write(fd, raw.baseAddress!.advanced(by: off), raw.count - off)
                if n <= 0 { throw AdbError.io("write failed (errno \(errno))") }
                off += n
            }
        }
    }

    private func readAll(_ count: Int) throws -> Data {
        var out = Data(); out.reserveCapacity(count)
        var buf = [UInt8](repeating: 0, count: 64 * 1024)
        while out.count < count {
            let want = min(count - out.count, buf.count)
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, want) }
            if n <= 0 { throw AdbError.io("read failed (errno \(errno))") }
            out.append(contentsOf: buf[0..<n])
        }
        return out
    }

    private func recv() throws -> (cmd: UInt32, arg0: UInt32, arg1: UInt32, data: Data) {
        let h = try readAll(24)
        func u32(_ i: Int) -> UInt32 {
            h.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: i * 4, as: UInt32.self).littleEndian }
        }
        let len = Int(u32(3))
        let body = len > 0 ? try readAll(len) : Data()
        return (u32(0), u32(1), u32(2), body)
    }

    enum AdbError: Error, LocalizedError {
        case io(String), refused(String)
        var errorDescription: String? {
            switch self { case .io(let m), .refused(let m): return m }
        }
    }

    // MARK: connection

    func connect(timeoutSeconds: Int = 3) throws {
        disconnect()
        let s = socket(AF_INET, SOCK_STREAM, 0)
        guard s >= 0 else { throw AdbError.io("socket() failed") }
        var addr = sockaddr_in()
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = UInt16(5555).bigEndian
        addr.sin_addr.s_addr = inet_addr("127.0.0.1")
        var tv = timeval(tv_sec: timeoutSeconds, tv_usec: 0)
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))
        let ok = withUnsafePointer(to: &addr) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                Darwin.connect(s, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }
        guard ok == 0 else { close(s); throw AdbError.refused("nothing listening on 5555") }
        fd = s

        // Handshake. The device answers CNXN when adbd is not demanding a key,
        // which is what ro.adb.secure=0 in the guest image buys us.
        let banner = "host::features=cmd,shell_v2\0".data(using: .utf8)!
        try send(.cnxn, 0x0100_0000, 256 * 1024, banner)
        let r = try recv()
        if r.cmd == Cmd.auth.rawValue {
            disconnect()
            throw AdbError.refused("adbd wants key authentication")
        }
        guard r.cmd == Cmd.cnxn.rawValue else {
            disconnect()
            throw AdbError.refused("unexpected reply 0x\(String(r.cmd, radix: 16))")
        }
        HuskLog.log("adb", "connected: \(String(decoding: r.data, as: UTF8.self).prefix(120))")
    }

    func disconnect() {
        if fd >= 0 { close(fd); fd = -1 }
    }

    // MARK: services

    /// Run a shell command and return everything it printed.
    /// Log every frame of one shell exchange.
    ///
    /// Set for the first command of a session. An empty result is ambiguous --
    /// adbd refusing the service and closing the stream looks identical to this
    /// code losing the output -- and the frames tell the two apart.
    var traceNextShell = false

    func shell(_ command: String) throws -> String {
        let local = nextLocalId; nextLocalId &+= 1
        let trace = traceNextShell
        traceNextShell = false
        if trace { HuskLog.log("adb", "OPEN shell:\(command) (local=\(local))") }
        try send(.open, local, 0, ("shell:" + command + "\0").data(using: .utf8)!)
        var remote: UInt32 = 0
        var out = Data()
        while true {
            let r = try recv()
            if trace {
                let name = ["4e584e43": "CNXN", "4e45504f": "OPEN", "59414b4f": "OKAY",
                            "45534c43": "CLSE", "45545257": "WRTE", "48545541": "AUTH"]
                    [String(r.cmd, radix: 16)] ?? "0x\(String(r.cmd, radix: 16))"
                HuskLog.log("adb", "  <- \(name) arg0=\(r.arg0) arg1=\(r.arg1) "
                                 + "len=\(r.data.count) \(String(decoding: r.data.prefix(80), as: UTF8.self).debugDescription)")
            }
            switch r.cmd {
            case Cmd.okay.rawValue:
                remote = r.arg0
            case Cmd.wrte.rawValue:
                out.append(r.data)
                try send(.okay, local, remote)
            case Cmd.clse.rawValue:
                try? send(.clse, local, remote)
                return String(decoding: out, as: UTF8.self)
            default:
                throw AdbError.io("unexpected 0x\(String(r.cmd, radix: 16)) during shell")
            }
        }
    }

    /// Push a local file into the guest via the sync service.
    func push(_ local: URL, to remotePath: String, mode: Int = 0o644,
              progress: ((Double) -> Void)? = nil) throws {
        let total = (try? FileManager.default
            .attributesOfItem(atPath: local.path)[.size] as? Int) ?? 0
        let handle = try FileHandle(forReadingFrom: local)
        defer { try? handle.close() }

        let localId = nextLocalId; nextLocalId &+= 1
        try send(.open, localId, 0, "sync:\0".data(using: .utf8)!)
        var remoteId: UInt32 = 0
        while remoteId == 0 {
            let r = try recv()
            if r.cmd == Cmd.okay.rawValue { remoteId = r.arg0 }
            else if r.cmd == Cmd.clse.rawValue { throw AdbError.io("sync refused") }
        }

        func syncPacket(_ id: String, _ payload: Data) -> Data {
            var d = id.data(using: .ascii)!
            withUnsafeBytes(of: UInt32(payload.count).littleEndian) { d.append(contentsOf: $0) }
            d.append(payload)
            return d
        }
        func sendSync(_ d: Data) throws {
            try send(.wrte, localId, remoteId, d)
            while true {                                  // wait for flow control
                let r = try recv()
                if r.cmd == Cmd.okay.rawValue { return }
                if r.cmd == Cmd.wrte.rawValue { try send(.okay, localId, remoteId) }
                if r.cmd == Cmd.clse.rawValue { throw AdbError.io("sync closed early") }
            }
        }

        let target = "\(remotePath),\(mode)"
        try sendSync(syncPacket("SEND", target.data(using: .utf8)!))

        var sent = 0
        while true {
            let chunk = try handle.read(upToCount: 64 * 1024) ?? Data()
            if chunk.isEmpty { break }
            try sendSync(syncPacket("DATA", chunk))
            sent += chunk.count
            if total > 0 { progress?(Double(sent) / Double(total)) }
        }
        var done = "DONE".data(using: .ascii)!
        withUnsafeBytes(of: UInt32(Date().timeIntervalSince1970).littleEndian) {
            done.append(contentsOf: $0)
        }
        try sendSync(done)

        // The reply is OKAY or FAIL; either arrives as WRTE payload.
        var reply = Data()
        while reply.count < 8 {
            let r = try recv()
            if r.cmd == Cmd.wrte.rawValue {
                reply.append(r.data); try send(.okay, localId, remoteId)
            } else if r.cmd == Cmd.clse.rawValue { break }
        }
        try? send(.clse, localId, remoteId)
        let tag = String(decoding: reply.prefix(4), as: UTF8.self)
        if tag == "FAIL" { throw AdbError.io("push rejected: \(String(decoding: reply.dropFirst(8), as: UTF8.self))") }
        HuskLog.log("adb", "pushed \(sent) bytes to \(remotePath)")
    }
}

// MARK: - Android host

/// The running guest, seen as something apps can be installed into and started.
///
/// Everything here goes over ADB, which only works once the guest has finished
/// coming up -- so readiness is polled rather than assumed, and the UI shows
/// that waiting honestly instead of pretending the library is usable.
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

    /// Poll until adbd answers and Android reports it has finished booting.
    func waitForReady() {
        guard !polling, !isReady else { return }
        polling = true
        status = "Starting Android…"
        Task.detached { [weak self] in
            var attempt = 0
            while true {
                attempt += 1
                do {
                    try Adb.shared.connect()
                    // Report what the shell can actually do, once.
                    //
                    // The connection succeeds and then readiness never arrives,
                    // which means a command is failing rather than the transport.
                    // The guest log shows adbd running as u:r:adbd_tradeinmode:s0
                    // -- Android's restricted trade-in ADB domain -- so the
                    // question is whether shell works at all, not whether the
                    // property is set. Ask it three things and print the answers
                    // verbatim.
                    if attempt == 1 || attempt % 10 == 0 {
                        Adb.shared.traceNextShell = true
                        for probe in ["echo husk-ok", "id", "getprop sys.boot_completed"] {
                            do {
                                let r = try Adb.shared.shell(probe)
                                HuskLog.log("adb", "[\(probe)] -> \(r.debugDescription.prefix(200))")
                            } catch {
                                HuskLog.log("adb", "[\(probe)] FAILED: \(error.localizedDescription)")
                            }
                        }
                    }
                    let booted = try Adb.shared.shell("getprop sys.boot_completed")
                        .trimmingCharacters(in: .whitespacesAndNewlines)
                    if booted == "1" {
                        await MainActor.run {
                            self?.isReady = true
                            self?.status = "Android is ready"
                            self?.polling = false
                        }
                        HuskLog.log("adb", "guest is ready after \(attempt) attempts")
                        await self?.refreshPackages()
                        return
                    }
                    await MainActor.run { self?.status = "Android is booting…" }
                } catch {
                    Adb.shared.disconnect()
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
        guard Adb.shared.isConnected else { return }
        do {
            let raw = try Adb.shared.shell("pm list packages -3")
            let names = raw.split(separator: "\n")
                .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
                .filter { $0.hasPrefix("package:") }
                .map { String($0.dropFirst("package:".count)) }
                .filter { !$0.isEmpty }
            await MainActor.run {
                self.packages = names.map { Package(name: $0, label: Self.pretty($0)) }
            }
            HuskLog.log("adb", "\(names.count) user package(s) installed")
        } catch {
            HuskLog.log("adb", "could not list packages: \(error.localizedDescription)")
        }
    }

    /// "com.dotgears.flappybird" reads better as "Flappybird" until we can ask
    /// Android for the real label.
    private static func pretty(_ pkg: String) -> String {
        (pkg.split(separator: ".").last.map(String.init) ?? pkg).capitalized
    }

    /// Push an APK into the guest and install it.
    func install(_ apk: URL) {
        guard Adb.shared.isConnected else { return }
        let name = apk.lastPathComponent
        busy = "Installing \(name)…"
        Task.detached { [weak self] in
            let remote = "/data/local/tmp/husk-install.apk"
            do {
                let scoped = apk.startAccessingSecurityScopedResource()
                defer { if scoped { apk.stopAccessingSecurityScopedResource() } }
                try Adb.shared.push(apk, to: remote) { p in
                    Task { @MainActor in
                        self?.busy = "Copying \(name) — \(Int(p * 100))%"
                    }
                }
                await MainActor.run { self?.busy = "Installing \(name)…" }
                let out = try Adb.shared.shell("pm install -r -t \(remote)")
                _ = try? Adb.shared.shell("rm -f \(remote)")
                let ok = out.contains("Success")
                HuskLog.log("adb", "install \(name): \(out.trimmingCharacters(in: .whitespacesAndNewlines))")
                await self?.refreshPackages()
                await MainActor.run {
                    self?.busy = ok ? nil : "Install failed: \(out.prefix(120))"
                    if !ok {
                        Task { try? await Task.sleep(nanoseconds: 4_000_000_000)
                               await MainActor.run { self?.busy = nil } }
                    }
                }
            } catch {
                HuskLog.log("adb", "install failed: \(error.localizedDescription)")
                await MainActor.run { self?.busy = "Install failed: \(error.localizedDescription)" }
            }
        }
    }

    /// Start an app by package name.
    ///
    /// monkey rather than `am start`, because it finds the launcher activity on
    /// its own -- we do not know the activity name and would have to resolve it.
    func launch(_ pkg: String, then: @escaping () -> Void) {
        guard Adb.shared.isConnected else { return }
        busy = "Opening…"
        Task.detached { [weak self] in
            let out = (try? Adb.shared.shell(
                "monkey -p \(pkg) -c android.intent.category.LAUNCHER 1")) ?? ""
            HuskLog.log("adb", "launch \(pkg): \(out.split(separator: "\n").last ?? "")")
            await MainActor.run { self?.busy = nil; then() }
        }
    }
}
