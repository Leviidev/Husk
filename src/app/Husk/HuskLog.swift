// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import UIKit
import os

/// Everything Husk knows, in one chronological file.
///
/// The problem this solves: QEMU and Husk's own C code write diagnostics with
/// `fprintf(stderr, ...)`, and on iOS stderr goes nowhere a user can reach. So
/// before anything else runs, stdout and stderr are redirected into a pipe that a
/// reader thread drains, writing every line to three places at once:
///
///   * `Documents/husk.log`, retrievable over the Files app or the share sheet
///   * `os_log`, so Console.app shows it live with the device attached
///   * an in-memory ring buffer the app can display without leaving the device
///
/// Because QEMU's output and Husk's Swift logging both land in the same stream,
/// the ordering between "the JIT region was allocated" and "QEMU started
/// translating" is preserved, which is exactly the ordering that matters when
/// something goes wrong.
enum HuskLog {
    private static let osLog = Logger(subsystem: "com.husk.app", category: "husk")

    /// Exposed because a C signal handler closure cannot capture context.
    fileprivate static var rawLogFD: Int32 { logFD }
    private static var logFD: Int32 = -1
    private static var pipeReadFD: Int32 = -1
    private static var pipeWriteFD: Int32 = -1
    private static var started = false
    private static let writeLock = NSLock()
    private static let startTime = Date()

    /// Recent lines, for the in-app viewer. Bounded so a long session cannot grow
    /// without limit.
    private static let ringLimit = 4000
    private static var ring: [String] = []
    private static let ringLock = NSLock()

    static var logFileURL: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("husk.log")
    }

    static func recentLines(_ n: Int = 400) -> [String] {
        ringLock.lock(); defer { ringLock.unlock() }
        return Array(ring.suffix(n))
    }

    // MARK: - Startup

    static func start() {
        guard !started else { return }
        started = true

        // A write to a pipe whose read end has closed raises SIGPIPE, and the
        // default action for SIGPIPE is to terminate the process. Since stdout and
        // stderr are about to become that pipe, ignore it outright -- a logging
        // failure must never be able to kill the app it is trying to diagnose.
        signal(SIGPIPE, SIG_IGN)

        // Fresh file each launch. StikDebug relaunches us after attaching, so the
        // run that matters is always the most recent one; keeping the previous
        // run's noise would make the log harder to read, not easier.
        let url = logFileURL
        logFD = open(url.path, O_CREAT | O_WRONLY | O_TRUNC, 0o644)

        redirectStdio()
        installCrashHandlers()
        logBanner()
    }

    /// Force everything out to disk.
    static func flushNow() {
        if logFD >= 0 { fsync(logFD) }
    }

    /// Point stdout and stderr at a pipe we drain ourselves.
    ///
    /// Uses raw pipe(2) descriptors held in statics rather than Foundation's Pipe.
    /// Pipe owns its FileHandles and closes their descriptors when it deallocates,
    /// so a locally-scoped Pipe tears down the read end the moment this function
    /// returns -- after which every write to stderr hits a reader-less pipe. These
    /// descriptors belong to no object and are never closed.
    private static func redirectStdio() {
        var fds: [Int32] = [-1, -1]
        guard pipe(&fds) == 0 else {
            // Nothing to redirect into; keep going, the file and os_log still work
            // for anything logged through HuskLog.log().
            log("boot", "WARNING: pipe() failed (errno \(errno)); C-side stderr will not be captured")
            return
        }
        pipeReadFD = fds[0]
        pipeWriteFD = fds[1]

        setvbuf(stdout, nil, _IOLBF, 0)
        setvbuf(stderr, nil, _IONBF, 0)
        dup2(pipeWriteFD, STDOUT_FILENO)
        dup2(pipeWriteFD, STDERR_FILENO)

        let t = Thread {
            var pending = Data()
            var buf = [UInt8](repeating: 0, count: 16 * 1024)
            while true {
                let n = read(pipeReadFD, &buf, buf.count)
                if n < 0 {
                    if errno == EINTR { continue }
                    break
                }
                if n == 0 { break }
                pending.append(contentsOf: buf[0..<n])

                // Emit complete lines only, so a partial write never splits a
                // message across two log entries.
                while let nl = pending.firstIndex(of: 0x0A) {
                    let lineData = pending.subdata(in: pending.startIndex..<nl)
                    pending.removeSubrange(pending.startIndex...nl)
                    if let line = String(data: lineData, encoding: .utf8) {
                        emit(line, fromStream: true)
                    }
                }
            }
        }
        t.name = "com.husk.log.reader"
        t.qualityOfService = .userInitiated
        t.start()
    }

    /// Turn "the app vanished with no crash log" into a labelled last line.
    ///
    /// Deliberately does NOT touch SIGTRAP or SIGBUS: those belong to the JIT trap
    /// guard in husk-ios-jit.c, which needs to step over an unserviced brk rather
    /// than treat it as fatal. Stealing them here would break JIT detection.
    /// SIGPIPE is already set to SIG_IGN in start() and must stay that way.
    private static func installCrashHandlers() {
        let fatal: [Int32] = [SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGSYS]
        for sig in fatal {
            signal(sig) { received in
                // Async-signal-safe only: write(2) straight to the fds, no
                // allocation, no locks, no Swift runtime.
                let msg = "\n*** FATAL SIGNAL \(received) -- process is dying ***\n"
                msg.withCString { p in
                    let n = strlen(p)
                    if HuskLog.rawLogFD >= 0 { _ = write(HuskLog.rawLogFD, p, n) }
                }
                if HuskLog.rawLogFD >= 0 { fsync(HuskLog.rawLogFD) }
                signal(received, SIG_DFL)
                raise(received)
            }
        }
        NSSetUncaughtExceptionHandler { ex in
            HuskLog.log("crash", "uncaught exception: \(ex.name.rawValue) -- \(ex.reason ?? "")")
            HuskLog.log("crash", ex.callStackSymbols.prefix(20).joined(separator: " | "))
            HuskLog.flushNow()
        }
        log("boot", "crash handlers installed (SEGV/ABRT/ILL/FPE/SYS; "
                  + "TRAP+BUS left to the JIT guard; SIGPIPE ignored)")
    }

    // MARK: - Emitting

    /// - Parameter fromStream: true when the line came out of the captured pipe,
    ///   meaning the C side already sent it to os_log itself.
    private static func emit(_ line: String, fromStream: Bool) {
        ringLock.lock()
        ring.append(line)
        if ring.count > ringLimit { ring.removeFirst(ring.count - ringLimit) }
        ringLock.unlock()

        // The C side (husk-ios-jit.c, husk-display.c) writes to os_log itself as
        // well as stderr, because those are the lines most worth surviving a hard
        // crash. Mirroring them again here would double every one of them in
        // Console.app.
        if !fromStream || (!line.contains("[husk-jit]") && !line.contains("[husk-dpy]")) {
            osLog.log("\(line, privacy: .public)")
        }

        // Written synchronously with write(2) rather than queued. An async write
        // can still be pending when the process dies, which loses exactly the
        // lines that explain why it died -- and those are the whole point of this
        // file. write(2) to a file lands in the page cache and is cheap.
        guard logFD >= 0, let data = (line + "\n").data(using: .utf8) else { return }
        writeLock.lock()
        data.withUnsafeBytes { raw in
            var off = 0
            while off < raw.count {
                let n = write(logFD, raw.baseAddress!.advanced(by: off), raw.count - off)
                if n <= 0 { break }
                off += n
            }
        }
        writeLock.unlock()
    }

    /// Log from Swift. Lands in the same stream as QEMU's own output.
    static func log(_ category: String, _ message: String) {
        let t = Date().timeIntervalSince(startTime) * 1000
        emit(String(format: "[%9.2fms][%@] %@", t, category, message), fromStream: false)
    }

    // MARK: - Context

    /// The hardware identifier, e.g. "iPhone18,1". Which JIT routes exist on
    /// this device is decided from it and the iOS version.
    static var deviceModel: String {
        var sysinfo = utsname(); uname(&sysinfo)
        return withUnsafePointer(to: &sysinfo.machine) {
            $0.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
        }
    }

    private static func logBanner() {
        let d = UIDevice.current
        let model = deviceModel
        let mem = ProcessInfo.processInfo.physicalMemory / (1024 * 1024)

        log("boot", "================ Husk starting ================")
        log("boot", "device      : \(model)")
        log("boot", "system      : \(d.systemName) \(d.systemVersion)")
        log("boot", "physical RAM: \(mem) MiB")
        log("boot", "processors  : \(ProcessInfo.processInfo.processorCount) "
                  + "(active \(ProcessInfo.processInfo.activeProcessorCount))")
        log("boot", "bundle      : \(Bundle.main.bundleIdentifier ?? "?")")
        // Which build this is, in the log itself.
        //
        // Without this a log cannot be told apart from one produced by an older
        // install, and a stale app on the phone reads exactly like a fix that
        // did not work -- which has now cost a debugging round trip. The commit
        // and date are stamped into Info.plist by package_ipa.sh.
        let info = Bundle.main.infoDictionary ?? [:]
        let version = info["CFBundleShortVersionString"] as? String ?? "?"
        let build   = info["CFBundleVersion"] as? String ?? "?"
        let commit  = info["HuskBuildCommit"] as? String ?? "unstamped"
        let built   = info["HuskBuildDate"] as? String ?? "?"
        log("boot", "build       : \(version) (\(build)) \(commit) built \(built)")
        log("boot", "pid         : \(getpid())")
        log("boot", "log file    : \(logFileURL.path)")
        // Every iOS 27 device except iPad8,11/8,12 enforces TXM, which is what makes
        // the debugger-assisted JIT path mandatory rather than optional.
        log("boot", "TXM expected: \(Self.expectsTXM(model: model) ? "YES" : "no")")
        log("boot", "===============================================")
    }

    static func expectsTXM(model: String) -> Bool {
        if #available(iOS 27.0, *) {
            return model != "iPad8,11" && model != "iPad8,12"
        }
        if #available(iOS 26.0, *) {
            // A14-era and newer: iPhone13,x is A14, so HW major >= 13 for phones.
            let digits = model.drop { !$0.isNumber }.prefix { $0.isNumber }
            if let major = Int(digits) {
                return model.hasPrefix("iPhone") ? major >= 13 : major >= 14
            }
        }
        return false
    }

    /// Snapshot of process memory, the figure jetsam kills on.
    static func logFootprint(_ tag: String) {
        husk_ios_jit_log_footprint(tag)
    }
}
