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

    /// Bumped per command, so each one's marker is unique.
    ///
    /// With a single fixed marker, a command that timed out left its answer in
    /// the socket and the next command read that instead of its own -- so the
    /// only safe response to a timeout was to throw the connection away. Which
    /// meant opening a new one, which is the exact operation that stops working
    /// part-way through a session. The bridge was destroying the thing that was
    /// keeping it alive, every time a command ran long.
    ///
    /// A sequence number in the marker makes a late answer harmless: it is
    /// preamble to the next command's marker and is discarded on the way past.
    private var sequence: UInt64 = 0

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

    // MARK: the held connection

    /// One connection, opened early and kept.
    ///
    /// Every command used to open its own, which is clean but turned out to be
    /// the thing that breaks. The health watch caught the failure exactly:
    ///
    ///   [111112ms] health: the shell STOPPED answering -- 5599 (husk_agent):
    ///             connected, then silence until timeout
    ///
    /// A completed TCP handshake followed by silence has two causes and they are
    /// indistinguishable from outside. Either a firewall rule appeared and is
    /// dropping new connections, or `husk_agent` serves one client at a time and
    /// is wedged, leaving new connections to complete the handshake into a
    /// backlog nothing will ever accept. init reports the service alive
    /// throughout ("already running, flags: 4, pid: 620"), which fits the second
    /// reading; netd installing netfilter rules around the same time fits the
    /// first.
    ///
    /// Holding one connection answers both without having to know which. An
    /// established connection survives a firewall rule appearing -- those match
    /// on new ones -- and if the listener only ever serves one client, being
    /// that client means never competing for accept() again.
    private var control: Int32 = -1
    private let controlLock = NSLock()

    /// The held connection, opening it if this is the first use or the last one
    /// broke. Caller must hold controlLock.
    private func controlFD(timeout: TimeInterval) throws -> Int32 {
        if control >= 0 {
            var tv = timeval(tv_sec: Int(timeout), tv_usec: 0)
            setsockopt(control, SOL_SOCKET, SO_RCVTIMEO, &tv,
                       socklen_t(MemoryLayout<timeval>.size))
            setsockopt(control, SOL_SOCKET, SO_SNDTIMEO, &tv,
                       socklen_t(MemoryLayout<timeval>.size))
            return control
        }
        let fd = try openSocket(timeout: timeout)
        // Keepalives, so a connection that has been idle for minutes is known to
        // be dead before a two-hundred-megabyte transfer is started on it.
        var on: Int32 = 1
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, socklen_t(MemoryLayout<Int32>.size))
        control = fd
        HuskLog.log("bridge", "opened the held connection to the Android shell")
        return fd
    }

    /// Drop the held connection, so the next command opens a fresh one.
    private func dropControl(_ why: String) {
        if control >= 0 {
            close(control)
            control = -1
            isConnected = false
            HuskLog.log("bridge", "the held connection broke: \(why)")
        }
    }

    /// Open the connection now, while connections still work.
    ///
    /// The window closes: connections succeeded for the first hundred seconds of
    /// the last session and not afterwards. Whatever the mechanism, being inside
    /// the window when the connection is made is what matters, so this is called
    /// as soon as the guest answers rather than when something first needs it.
    func holdConnection() {
        controlLock.lock()
        defer { controlLock.unlock() }
        _ = try? controlFD(timeout: 15)
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
        try run(command, timeout: timeout).out
    }

    /// Write a command to the held connection and read back to its marker.
    ///
    /// Serialised, because one connection means one shell: two commands
    /// interleaved on it would each read the other's output. Caller must hold
    /// controlLock.
    private func exchange(_ command: String, timeout: TimeInterval)
            throws -> (out: String, status: Int) {
        let fd = try controlFD(timeout: timeout)
        sequence += 1
        let token = "\(Self.marker)\(sequence):"
        try writeAll(fd, Data("\(command) 2>&1; echo \(token)$?\n".utf8))

        var out = ""
        var buf = [UInt8](repeating: 0, count: 16 * 1024)
        let deadline = Date().addingTimeInterval(timeout)
        while true {
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, $0.count) }
            if n < 0 && errno == EINTR { continue }
            // EAGAIN is SO_RCVTIMEO expiring: the guest has not answered YET.
            // Treating it as a dead socket is what tore down a working
            // connection every fifteen seconds.
            if n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) {
                if Date() > deadline {
                    throw BridgeError.timeout("waiting for `\(command.prefix(60))`")
                }
                continue
            }
            if n <= 0 {
                let why = n == 0 ? "the guest closed it" : "read failed (errno \(errno))"
                dropControl(why)
                throw BridgeError.io("guest closed the connection -- \(why)")
            }
            out += String(decoding: buf[0..<n], as: UTF8.self)
            if let r = out.range(of: token) {
                let status = Int(out[r.upperBound...]
                    .trimmingCharacters(in: .whitespacesAndNewlines)) ?? 0
                isConnected = true
                let body = String(out[out.startIndex..<r.lowerBound])
                if status != 0 {
                    HuskLog.log("bridge", "`\(command.prefix(60))` exit \(status)")
                }
                return (body, status)
            }
            if Date() > deadline {
                // The connection is KEPT. Whatever this command eventually
                // prints is preamble to the next command's marker, which no
                // longer matches this one.
                throw BridgeError.timeout("waiting for `\(command.prefix(60))`")
            }
        }
    }

    /// Run one command and return both its output and its exit status.
    ///
    /// `shell` discards the status, which is fine for reads and fatal for
    /// anything whose failure has to stop what follows.
    func run(_ command: String, timeout: TimeInterval = 30) throws -> (out: String, status: Int) {
        controlLock.lock()
        defer { controlLock.unlock() }
        do {
            return try exchange(command, timeout: timeout)
        } catch {
            // One retry, and only because the held connection may simply have
            // aged out while nothing was using it. dropControl() already cleared
            // it, so this attempt opens a fresh one -- which is exactly the
            // thing that stops working later in a session, hence one try and
            // not a loop.
            return try exchange(command, timeout: timeout)
        }
    }

    /// Run a command and read everything it writes, as bytes.
    ///
    /// `exec` again, for the same reason the push path uses it: the shell never
    /// closes the connection by itself, so "read until EOF" would hang forever.
    /// Replacing the shell with the program means the socket closes the moment
    /// that program exits, which is exactly the end-of-data signal needed for
    /// binary output that has no marker to look for.
    func pull(_ command: String, timeout: TimeInterval = 60,
              limit: Int = 8 << 20) throws -> Data {
        let fd = try openSocket(timeout: timeout)
        defer { close(fd) }
        try writeAll(fd, Data("exec \(command)\n".utf8))

        var out = Data()
        var buf = [UInt8](repeating: 0, count: 64 * 1024)
        while out.count < limit {
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, $0.count) }
            if n < 0 && errno == EINTR { continue }
            if n <= 0 { break }
            out.append(contentsOf: buf[0..<n])
        }
        isConnected = true
        return out
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

        // On the held connection, like everything else.
        //
        // This used to open its own, and that is precisely the operation that
        // stops working part-way through a session -- so the one thing in Husk
        // that most needs a working channel was the one asking for a new one.
        // Budget by size, not by a flat two minutes.
        //
        // 120 seconds is generous for a five-megabyte APK and nowhere near
        // enough for a three-hundred-megabyte one going through a shell on an
        // emulated NIC. The floor covers small files; the rate assumes a
        // pessimistic 300 KB/s so a slow transfer is slow rather than failed.
        let budget = max(180.0, Double(size) / 300_000.0)
        controlLock.lock()
        defer { controlLock.unlock() }
        let fd = try controlFD(timeout: budget)

        // `head -c N > file`, without `exec`.
        //
        // exec replaced the shell so that nothing could read ahead past the
        // command line and swallow the first bytes of the APK. It also ended the
        // connection, which is no longer acceptable. Dropping it is safe because
        // POSIX requires a shell reading commands from a non-seekable stream to
        // leave that stream positioned exactly after the command it ran -- mksh,
        // which is /system/bin/sh here, reads a byte at a time to honour that.
        // head then inherits the socket and takes exactly the next N bytes, and
        // the shell resumes reading commands after them.
        //
        // If that ever proves wrong the transfer does not silently corrupt: the
        // caller compares `wc -c` against the file's real size before pm sees it.
        sequence += 1
        let token = "\(Self.marker)\(sequence):"
        let command = "head -c \(size) > \(remote) 2>&1; echo \(token)$?\n"
        try writeAll(fd, Data(command.utf8))

        var sent = 0
        while sent < size {
            guard let chunk = try handle.read(upToCount: 256 * 1024), !chunk.isEmpty else { break }
            try writeAll(fd, chunk)
            sent += chunk.count
            progress(Double(sent) / Double(size))
        }
        if sent != size {
            dropControl("sent \(sent) of \(size) bytes; the stream is out of step")
            throw BridgeError.io("sent \(sent) of \(size) bytes")
        }

        // head has its N bytes and exits; the marker is the shell telling us it
        // is back to reading commands, which is also how we know the connection
        // is still usable for the pm install that follows.
        var out = ""
        var buf = [UInt8](repeating: 0, count: 4096)
        let deadline = Date().addingTimeInterval(budget)
        while !out.contains(token) {
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, $0.count) }
            if n < 0 && errno == EINTR { continue }
            if n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) {
                if Date() > deadline {
                    dropControl("no acknowledgement after the transfer")
                    throw BridgeError.timeout("waiting for the guest to write \(remote)")
                }
                continue
            }
            if n <= 0 {
                let why = n == 0 ? "the guest closed it" : "read failed (errno \(errno))"
                dropControl(why)
                throw BridgeError.io("\(why) after sending \(sent) bytes")
            }
            out += String(decoding: buf[0..<n], as: UTF8.self)
            if Date() > deadline {
                dropControl("no acknowledgement after the transfer")
                throw BridgeError.timeout("waiting for the guest to finish writing \(remote)")
            }
        }
        HuskLog.log("bridge", "pushed \(sent) bytes to \(remote)")
    }

    /// Is a shell answering right now?
    ///
    /// Retried, because init restarts `husk_agent` when it dies and a single
    /// failed probe cannot tell a dead listener from one being restarted.
    func isAlive(attempts: Int = 4) -> Bool {
        for i in 1...attempts {
            if let out = try? shell("echo __HUSK_ALIVE__", timeout: 15),
               out.contains("__HUSK_ALIVE__") {
                return true
            }
            if i < attempts { Thread.sleep(forTimeInterval: 2) }
        }
        return false
    }

    /// Why the bridge is not answering, in terms that name a cause.
    ///
    /// "the Android shell is not answering" was true and useless. With QEMU's
    /// user networking, connect() to the forwarded port succeeds locally almost
    /// always -- slirp accepts on the host and only then tries the guest -- so
    /// what happens NEXT is the whole diagnosis:
    ///
    ///   closed immediately  the guest refused the port: husk_agent is gone
    ///   nothing, then timeout  the packets are being dropped, not refused,
    ///                          which is what a firewall rule looks like
    ///   connect() itself failed  slirp is not forwarding at all
    ///
    /// Deliberately does NOT probe 5555.
    ///
    /// adbd binds that port and it is forwarded, which makes it an inviting
    /// control for "is the guest's network up at all". Husk does not use adb --
    /// LineageOS keeps adbd in adbd_tradeinmode until setup completes and
    /// refuses every shell in that domain, which is the whole reason husk_agent
    /// exists. Even a connect-only probe would imply a dependency this project
    /// removed on purpose, so the three cases below carry the diagnosis alone.
    func diagnose() -> String {
        func probe(_ port: UInt16, expectBytes: Bool) -> String {
            let fd = socket(AF_INET, SOCK_STREAM, 0)
            guard fd >= 0 else { return "socket() failed (errno \(errno))" }
            defer { close(fd) }
            var tv = timeval(tv_sec: 6, tv_usec: 0)
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))

            var addr = sockaddr_in()
            addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
            addr.sin_family = sa_family_t(AF_INET)
            addr.sin_port = port.bigEndian
            addr.sin_addr.s_addr = inet_addr("127.0.0.1")
            let rc = withUnsafePointer(to: &addr) { raw in
                raw.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                    connect(fd, $0, socklen_t(MemoryLayout<sockaddr_in>.size))
                }
            }
            if rc != 0 { return "connect failed (errno \(errno))" }
            guard expectBytes else { return "connected" }

            _ = try? writeAll(fd, Data("echo __HUSK_ALIVE__\n".utf8))
            var buf = [UInt8](repeating: 0, count: 512)
            let n = buf.withUnsafeMutableBytes { read(fd, $0.baseAddress, $0.count) }
            if n > 0 {
                let got = String(decoding: buf[0..<n], as: UTF8.self)
                    .trimmingCharacters(in: .whitespacesAndNewlines)
                return "answered: \(got.prefix(60))"
            }
            if n == 0 { return "connected, then the guest closed it at once "
                             + "(the port is refused inside the guest)" }
            if errno == EAGAIN || errno == EWOULDBLOCK {
                return "connected, then silence until timeout "
                     + "(packets dropped rather than refused)"
            }
            return "connected, then read failed (errno \(errno))"
        }

        return "5599 (husk_agent): \(probe(Self.port, expectBytes: true))"
    }

    /// Notice the moment the bridge dies, rather than at the next install.
    ///
    /// The last log had the shell answering at 28 seconds and dead at 226, with
    /// nothing in between -- a two-hundred-second window in which the cause was
    /// somewhere, unobserved. One cheap probe every fifteen seconds, logged only
    /// when the answer CHANGES, turns that window into a timestamp that can be
    /// lined up against what init and netd were doing.
    func startHealthWatch() {
        guard !watching else { return }
        watching = true
        Thread.detachNewThread { [weak self] in
            Thread.current.name = "com.husk.bridge.health"
            var wasAlive: Bool?
            var ticks = 0
            var lastCrashLine: String?
            var lastAudioLog: [String] = []
            while let self {
                let alive = (try? self.shell("echo __HUSK_ALIVE__", timeout: 30))?
                    .contains("__HUSK_ALIVE__") ?? false
                // Sample the guest's own routing while we still can.
                //
                // The shell goes silent about a minute in, and the leading
                // theory is that netd installs per-uid routing rules pointing at
                // a default network that was never registered -- which would
                // leave uid 2000's writes with nowhere to go while the socket
                // stays open. That is exactly what these two commands show, and
                // both run fine as shell. The last sample before the silence is
                // the one that matters.
                if alive, ticks % 3 == 0, ticks < 30 {
                    for cmd in ["ip addr show", "ip rule show", "ip route show table all"] {
                        if let out = try? self.shell(cmd, timeout: 20) {
                            HuskLog.log("net", "[t+\(ticks * 5)s] \(cmd):\n"
                                      + out.trimmingCharacters(in: .whitespacesAndNewlines))
                        }
                    }
                }
                // Android's crash buffer, which is the only place an app crash
                // is written.
                //
                // The kernel serial console carries native tombstones and
                // nothing else, so a Java-side crash -- the Files app dying, a
                // media exception -- left no trace anywhere Husk could see. The
                // log said the guest was healthy while an app was crashing on
                // every launch.
                //
                // `-b crash` is a small dedicated ring, so this is cheap: a few
                // lines every half minute, and only the ones not seen before.
                if alive, ticks % 6 == 0, ticks > 0 {
                    // Android's audio stack, at warning level and above. One
                    // sound effect played and then silence, with the guest
                    // queueing three buffers in three minutes -- whatever made
                    // it give up is written here and nowhere else.
                    if let out = try? self.shell(
                            "logcat -d -t 120 *:S AudioFlinger:W AudioTrack:W "
                          + "audio_hw_generic:W AudioPolicyService:W "
                          + "AudioSystem:W audioserver:W", timeout: 30),
                       !out.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                        let lines = out.split(separator: "\n").map(String.init)
                            .filter { !$0.isEmpty && !$0.hasPrefix("---------") }
                        if lines != lastAudioLog, !lines.isEmpty {
                            lastAudioLog = lines
                            HuskLog.log("gaudio", "---- Android's audio log ----")
                            for line in lines.suffix(40) { HuskLog.log("gaudio", line) }
                        }
                    }
                }
                if alive, ticks % 6 == 3 {
                    if let out = try? self.shell("logcat -b crash -d -t 200", timeout: 30) {
                        let lines = out.split(separator: "\n").map(String.init)
                            .filter { !$0.isEmpty && !$0.hasPrefix("---------") }
                        var fresh: [String] = []
                        if let last = lastCrashLine,
                           let idx = lines.lastIndex(of: last) {
                            fresh = Array(lines[(idx + 1)...])
                        } else {
                            fresh = lines
                        }
                        if !fresh.isEmpty {
                            lastCrashLine = lines.last
                            HuskLog.log("crash", "---- \(fresh.count) new line(s) from "
                                              + "Android's crash log ----")
                            for line in fresh.suffix(80) { HuskLog.log("crash", line) }
                        }
                    }
                }
                ticks += 1

                if alive != wasAlive {
                    if wasAlive == nil {
                        HuskLog.log("bridge", "health: the shell is "
                                  + (alive ? "answering" : "NOT answering"))
                    } else {
                        HuskLog.log("bridge", alive
                            ? "health: the shell started answering again"
                            : "health: the shell STOPPED answering -- \(self.diagnose())")
                    }
                    wasAlive = alive
                }
                // Five seconds, not thirty.
                //
                // This is a keepalive as much as a probe. One candidate for the
                // shell going silent is an idle rule on the guest's side, and
                // regular traffic is both the cheapest test of that theory and
                // its cure. It is one `echo` on a connection we already hold.
                Thread.sleep(forTimeInterval: 5)
            }
        }
    }
    private var watching = false

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
        /// Local file the icon was written to, once it has been fetched.
        var iconPath: String?
    }

    /// Where pulled icons live. Caches, not Documents: they are derived from
    /// APKs that are still in the guest and can always be fetched again.
    private static var iconDirectory: URL {
        let dir = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk-icons")
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
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
                        // Take the connection now and never let go. New
                        // connections worked for the first hundred seconds of
                        // the last session and not afterwards, so the moment the
                        // guest first answers is the moment to claim one.
                        GuestBridge.shared.holdConnection()
                        await self?.quietAbsentHardware()
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
                self.packages = names.map { name in
                    let cached = Self.iconDirectory.appendingPathComponent("\(name).png")
                    return Package(name: name, label: Self.pretty(name),
                                   iconPath: FileManager.default.fileExists(atPath: cached.path)
                                             ? cached.path : nil)
                }
            }
            HuskLog.log("bridge", "\(names.count) user package(s) installed")
            for name in names { await fetchIcon(for: name) }
        } catch {
            HuskLog.log("bridge", "could not list packages: \(error.localizedDescription)")
        }
    }

    /// Pull an app's launcher icon out of its APK.
    ///
    /// An APK is a zip, so the icon can be read straight out of it without
    /// resolving Android resources -- which would mean parsing the binary
    /// resource table, and is far more than a picture in a list is worth. The
    /// entry is chosen by name and then by size: an app ships the same icon at
    /// several densities, and the largest is the one that still looks right on
    /// a phone screen.
    ///
    /// Apps whose icon is only an adaptive XML drawable have no single PNG to
    /// find; those keep the generic placeholder, which is the honest result.
    private func fetchIcon(for package: String) async {
        let dest = Self.iconDirectory.appendingPathComponent("\(package).png")
        if FileManager.default.fileExists(atPath: dest.path) { return }

        do {
            let paths = try GuestBridge.shared.shell("pm path \(package)", timeout: 30)
            guard let apk = paths.split(separator: "\n")
                    .map({ $0.trimmingCharacters(in: .whitespacesAndNewlines) })
                    .first(where: { $0.hasPrefix("package:") })
                    .map({ String($0.dropFirst("package:".count)) }), !apk.isEmpty else {
                return
            }

            // Sorted by the size column, largest first.
            let listing = try GuestBridge.shared.shell(
                "unzip -l \(apk) | grep -iE 'res/.*(launcher|icon).*\\.png$' "
              + "| sort -k1 -rn | head -1", timeout: 60)
            guard let entry = listing.split(separator: "\n").first?
                    .split(separator: " ").last.map(String.init),
                  entry.hasSuffix(".png") else {
                HuskLog.log("bridge", "no icon png in \(package)")
                return
            }

            let data = try GuestBridge.shared.pull("unzip -p \(apk) \(entry)", timeout: 90)
            // A zip entry that does not exist gives an error on stdout rather
            // than a file, so check it really is a PNG before keeping it.
            guard data.count > 8, data.prefix(4) == Data([0x89, 0x50, 0x4E, 0x47]) else {
                HuskLog.log("bridge", "\(package): \(entry) was not a PNG (\(data.count) bytes)")
                return
            }
            try data.write(to: dest)
            HuskLog.log("bridge", "icon for \(package): \(entry), \(data.count) bytes")

            await MainActor.run {
                if let i = self.packages.firstIndex(where: { $0.name == package }) {
                    self.packages[i].iconPath = dest.path
                }
            }
        } catch {
            HuskLog.log("bridge", "icon for \(package) failed: \(error.localizedDescription)")
        }
    }

    /// "com.dotgears.flappybird" reads better as "Flappybird" until we can ask
    /// Android for the real label.
    private static func pretty(_ pkg: String) -> String {
        (pkg.split(separator: ".").last.map(String.init) ?? pkg).capitalized
    }

    /// Whether the guest is awake and unlocked, which installing needs it to be.
    ///
    /// A restored snapshot comes back exactly as it was frozen, so the guest can
    /// be asleep or sitting on its lock screen while Husk's library looks
    /// perfectly normal. `pm` then fails somewhere deep with a message about
    /// packages, and nothing points at the actual cause.
    enum GuestState {
        case ready
        case asleep
        case locked
        case unreachable(String)
    }

    nonisolated static func guestState() -> GuestState {
        guard GuestBridge.shared.isAlive() else {
            let why = GuestBridge.shared.diagnose()
            HuskLog.log("bridge", "the shell is not answering -- \(why)")
            return .unreachable("the Android shell is not answering")
        }
        // mWakefulness is Awake / Asleep / Dozing / Dreaming.
        if let power = try? GuestBridge.shared.shell(
                "dumpsys power 2>/dev/null | grep -m1 mWakefulness=", timeout: 20),
           power.contains("Asleep") || power.contains("Dozing") {
            return .asleep
        }
        // Both spellings, because which one exists depends on the Android
        // release and neither is worth depending on alone.
        if let win = try? GuestBridge.shared.shell(
                "dumpsys window 2>/dev/null | "
              + "grep -m1 -oE '(mDreamingLockscreen|mShowingLockscreen)=[a-z]+'",
                timeout: 30),
           win.contains("=true") {
            return .locked
        }
        return .ready
    }

    /// Copy files into the guest's Download folder, without installing them.
    ///
    /// The importer already pushed APKs; everything else a person might want to
    /// get into Android -- a save file, a ROM, a texture pack, a photo -- had no
    /// route in at all. Same transfer as an install, minus pm.
    func sendFiles(_ files: [URL]) {
        guard !files.isEmpty else { return }
        busy = "Sending \(files.count == 1 ? files[0].lastPathComponent : "\(files.count) files")…"
        Task.detached { [weak self] in
            var sent = 0
            for file in files {
                let name = file.lastPathComponent
                do {
                    switch AndroidHost.guestState() {
                    case .locked:
                        throw BridgeError.io("Android is locked. Unlock it and try again.")
                    case .unreachable(let why):
                        throw BridgeError.io(why)
                    case .asleep:
                        _ = try? GuestBridge.shared.shell("input keyevent KEYCODE_WAKEUP")
                    case .ready:
                        break
                    }
                    let scoped = file.startAccessingSecurityScopedResource()
                    defer { if scoped { file.stopAccessingSecurityScopedResource() } }

                    // Quoted and stripped of any path: a filename is chosen by
                    // whoever made the file, and it reaches a shell verbatim.
                    let safe = name.replacingOccurrences(of: "'", with: "")
                    let remote = "/sdcard/Download/\(safe)"
                    _ = try? GuestBridge.shared.shell("mkdir -p /sdcard/Download")
                    try GuestBridge.shared.push(file, to: "'\(remote)'") { p in
                        Task { @MainActor in
                            self?.busy = "Sending \(name) — \(Int(p * 100))%"
                        }
                    }
                    // Without this the file exists and no app can see it: the
                    // media database is what Android's file pickers read.
                    _ = try? GuestBridge.shared.shell(
                        "am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE "
                      + "-d file://\(remote)", timeout: 60)
                    HuskLog.log("bridge", "sent \(name) to Download")
                    sent += 1
                } catch {
                    await MainActor.run {
                        self?.busy = "Could not send \(name): \(error.localizedDescription)"
                    }
                    try? await Task.sleep(nanoseconds: 4_000_000_000)
                    await MainActor.run { self?.busy = nil }
                    return
                }
            }
            let n = sent
            await MainActor.run {
                self?.busy = "Sent \(n) to Android's Download folder"
            }
            try? await Task.sleep(nanoseconds: 3_000_000_000)
            await MainActor.run { self?.busy = nil }
        }
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

                // Check before the transfer, not after. Copying an APK into the
                // guest takes minutes on an emulated disk, and discovering at
                // the end of it that Android was asleep the whole time is the
                // worst possible moment to find out.
                switch AndroidHost.guestState() {
                case .asleep:
                    // Waking it is something Husk can do itself, so do it
                    // rather than asking. Unlocking is not.
                    HuskLog.log("bridge", "Android was asleep; waking it to install")
                    _ = try? GuestBridge.shared.shell("input keyevent KEYCODE_WAKEUP")
                    Thread.sleep(forTimeInterval: 1.5)
                    if case .locked = AndroidHost.guestState() {
                        throw BridgeError.io("Android is locked. Open the Android "
                                           + "screen, unlock it, and try again.")
                    }
                case .locked:
                    throw BridgeError.io("Android is locked. Open the Android "
                                       + "screen, unlock it, and try again.")
                case .unreachable(let why):
                    throw BridgeError.io("\(why). Open the Android screen and "
                                       + "check it is running, then try again.")
                case .ready:
                    break
                }

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
                // dex2oat compiles the whole APK on an emulated CPU, and it
                // scales with the code in it. Ten minutes fits a small game and
                // not a large one.
                let installBudget: TimeInterval = expected > 50 << 20 ? 2400 : 600
                let out = try GuestBridge.shared.shell("pm install -r -t \(remote)",
                                                      timeout: installBudget)
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
                        // GPU mode can save now: the virgl blocker is lifted, the
                        // compositor is stopped around the write, and the log
                        // says "save succeeded". This used to tell people their
                        // install was already lost, which is worse than saying
                        // nothing -- it is a true-sounding statement that is no
                        // longer true.
                        self?.busy = "Saving Android — the screen will freeze briefly"
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
            await self.dumpCrashes()
        }
    }

    /// Stop Android chasing hardware this machine does not have.
    ///
    /// The QEMU command line has no Bluetooth controller, and the Bluetooth HAL
    /// does not take the hint: `BpBluetoothHci::initialize` aborts inside a
    /// binder ioctl, com.android.bluetooth dies with it, and the framework
    /// restarts the whole stack a few seconds later. One log caught that happen
    /// **88 times**, alongside four deaths of system_server -- which is the real
    /// damage, because a system_server restart takes every app with it and
    /// stalls the guest for tens of seconds.
    ///
    /// Turning the radio off in settings is what stops the loop: it is the
    /// switch BluetoothManagerService actually reads, and it costs nothing on a
    /// machine that has no radio to switch off.
    ///
    /// Done from the bridge rather than the guest image because the bridge is
    /// already a shell and `settings` is a shell command -- the same call from
    /// init fails, which is why husk-provision.rc has never worked.
    func quietAbsentHardware() async {
        let off = [
            ("bluetooth", "settings put global bluetooth_on 0"),
            ("ble scan",  "settings put global ble_scan_always_enabled 0"),
        ]
        for (what, cmd) in off {
            let r = try? GuestBridge.shared.run(cmd, timeout: 60)
            HuskLog.log("bridge", (r?.status == 0)
                ? "\(what) disabled -- this machine has no radio for it"
                : "could not disable \(what): "
                  + (r?.out.trimmingCharacters(in: .whitespacesAndNewlines) ?? "no answer"))
        }
    }

    /// Everything Android has recorded about its own crashes.
    ///
    /// The GPU-mode guest kills and restarts system_server repeatedly, and
    /// nothing visible from outside says why: the serial console shows init
    /// reaping zombies and the watchdog firing, which is the consequence rather
    /// than the cause. Android already knows -- it writes the fault address and
    /// the backtrace into logcat's crash buffer -- and this is simply reading
    /// what it wrote, so the next change can be a decision instead of a guess.
    ///
    /// Line by line rather than one blob: the log is read by grepping for
    /// markers, and a single entry holding a hundred newlines defeats that.
    func dumpCrashes() async {
        let out = (try? GuestBridge.shared.shell("logcat -d -b crash -t 200 2>/dev/null",
                                                 timeout: 120)) ?? ""
        let lines = out.split(separator: "\n").map(String.init)
            .filter { !$0.trimmingCharacters(in: .whitespaces).isEmpty }
        guard !lines.isEmpty else {
            HuskLog.log("crash", "crash buffer is empty -- nothing has died yet")
            return
        }
        HuskLog.log("crash", "---- \(lines.count) lines from Android's crash buffer ----")
        for line in lines.suffix(200) { HuskLog.log("crash", line) }
        HuskLog.log("crash", "---- end of crash buffer ----")
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
