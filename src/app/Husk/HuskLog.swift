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
    private static let queue = DispatchQueue(label: "com.husk.log", qos: .utility)

    private static var fileHandle: FileHandle?
    private static var started = false
    private static var pendingWrites = 0
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

        // Fresh file each launch. StikDebug relaunches us after attaching, so the
        // run that matters is always the most recent one; keeping the previous
        // run's noise would make the log harder to read, not easier.
        let url = logFileURL
        FileManager.default.createFile(atPath: url.path, contents: nil)
        fileHandle = try? FileHandle(forWritingTo: url)

        redirectStdio()
        installCrashHandlers()
        logBanner()
    }

    /// Turn "the app vanished with no crash log" into a labelled last line.
    ///
    /// Deliberately does NOT touch SIGTRAP or SIGBUS: those belong to the JIT trap
    /// guard in husk-ios-jit.c, which needs to step over an unserviced brk rather
    /// than treat it as fatal. Stealing them here would break JIT detection.
    private static func installCrashHandlers() {
        let fatal: [Int32] = [SIGSEGV, SIGABRT, SIGILL, SIGFPE, SIGSYS]
        for sig in fatal {
            signal(sig) { received in
                // Async-signal-safety: write(2) straight to the log fd, no
                // allocation, no Swift runtime.
                let msg = "\n*** FATAL SIGNAL \(received) -- process is dying ***\n"
                msg.withCString { p in _ = write(STDERR_FILENO, p, strlen(p)) }
                HuskLog.flushNow()
                signal(received, SIG_DFL)
                raise(received)
            }
        }
        NSSetUncaughtExceptionHandler { ex in
            HuskLog.log("crash", "uncaught exception: \(ex.name.rawValue) -- \(ex.reason ?? "")")
            HuskLog.log("crash", (ex.callStackSymbols.prefix(20)).joined(separator: " | "))
            HuskLog.flushNow()
        }
        log("boot", "crash handlers installed (SEGV/ABRT/ILL/FPE/SYS; TRAP+BUS left to the JIT guard)")
    }

    /// Force everything buffered out to disk. Safe to call from a signal handler
    /// path -- worst case the sync barrier times out and we lose nothing we had.
    static func flushNow() {
        queue.sync {
            pendingWrites = 0
            try? fileHandle?.synchronize()
        }
    }

    /// Point stdout and stderr at a pipe we drain ourselves.
    private static func redirectStdio() {
        let pipe = Pipe()
        let readFD = pipe.fileHandleForReading.fileDescriptor
        let writeFD = pipe.fileHandleForWriting.fileDescriptor

        setvbuf(stdout, nil, _IOLBF, 0)
        setvbuf(stderr, nil, _IONBF, 0)
        dup2(writeFD, STDOUT_FILENO)
        dup2(writeFD, STDERR_FILENO)

        let t = Thread {
            var pending = Data()
            while true {
                var buf = [UInt8](repeating: 0, count: 16 * 1024)
                let n = read(readFD, &buf, buf.count)
                if n <= 0 { break }
                pending.append(contentsOf: buf[0..<n])

                // Emit complete lines only, so a partial write never splits a
                // message across two log entries.
                while let nl = pending.firstIndex(of: 0x0A) {
                    let lineData = pending.subdata(in: pending.startIndex..<nl)
                    pending.removeSubrange(pending.startIndex...nl)
                    if let line = String(data: lineData, encoding: .utf8) {
                        emit(line, alreadyOnStream: true)
                    }
                }
            }
        }
        t.name = "com.husk.log.reader"
        t.qualityOfService = .utility
        t.start()
    }

    // MARK: - Emitting

    /// - Parameter alreadyOnStream: true when the line came out of the captured
    ///   pipe, in which case writing it back to stderr would loop forever.
    private static func emit(_ line: String, alreadyOnStream: Bool) {
        ringLock.lock()
        ring.append(line)
        if ring.count > ringLimit { ring.removeFirst(ring.count - ringLimit) }
        ringLock.unlock()

        // The C side (husk-ios-jit.c, husk-display.c) writes to os_log itself as
        // well as stderr, because those are the lines most worth surviving a hard
        // crash and os_log is more durable than our async file write. Mirroring
        // them again here would show every one of them twice in Console.app, so
        // skip the ones that already went out.
        if !line.contains("[husk-jit]") && !line.contains("[husk-dpy]") {
            osLog.log("\(line, privacy: .public)")
        }

        // Durability matters more here than throughput. The lines worth having are
        // the last ones before a crash, so flush to disk on anything that looks
        // like a failure, and periodically otherwise. Without this the async write
        // can still be queued when the process dies and the log simply stops short
        // of the interesting part.
        let critical = line.contains("FAIL") || line.contains("FATAL")
                    || line.contains("error") || line.contains("WARNING")
        queue.async {
            guard let data = (line + "\n").data(using: .utf8) else { return }
            fileHandle?.write(data)
            pendingWrites += 1
            if critical || pendingWrites >= 32 {
                pendingWrites = 0
                try? fileHandle?.synchronize()
            }
        }
    }

    /// Log from Swift. Lands in the same stream as QEMU's own output.
    static func log(_ category: String, _ message: String) {
        let t = Date().timeIntervalSince(startTime) * 1000
        emit(String(format: "[%9.2fms][%@] %@", t, category, message), alreadyOnStream: false)
    }

    // MARK: - Context

    private static func logBanner() {
        let d = UIDevice.current
        var sysinfo = utsname(); uname(&sysinfo)
        let model = withUnsafePointer(to: &sysinfo.machine) {
            $0.withMemoryRebound(to: CChar.self, capacity: 1) { String(cString: $0) }
        }
        let mem = ProcessInfo.processInfo.physicalMemory / (1024 * 1024)

        log("boot", "================ Husk starting ================")
        log("boot", "device      : \(model)")
        log("boot", "system      : \(d.systemName) \(d.systemVersion)")
        log("boot", "physical RAM: \(mem) MiB")
        log("boot", "processors  : \(ProcessInfo.processInfo.processorCount) "
                  + "(active \(ProcessInfo.processInfo.activeProcessorCount))")
        log("boot", "bundle      : \(Bundle.main.bundleIdentifier ?? "?")")
        log("boot", "pid         : \(getpid())")
        log("boot", "log file    : \(logFileURL.path)")
        // Every iOS 27 device except iPad8,11/8,12 enforces TXM, which is what makes
        // the debugger-assisted JIT path mandatory rather than optional.
        log("boot", "TXM expected: \(Self.expectsTXM(model: model) ? "YES" : "no")")
        log("boot", "===============================================")
    }

    private static func expectsTXM(model: String) -> Bool {
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
