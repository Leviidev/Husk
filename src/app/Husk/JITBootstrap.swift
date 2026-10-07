// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import UIKit
import os

/// `csops(2)`. Reading CS_DEBUGGED is how we ask "is a debugger attached right
/// now" without executing anything — unlike the brk probe, which costs a trap.
@_silgen_name("csops")
private func csops(_ pid: Int32, _ ops: Int32,
                   _ useraddr: UnsafeMutableRawPointer?, _ usersize: Int) -> Int32

private let CS_OPS_STATUS = Int32(0)
private let CS_DEBUGGED = UInt32(0x10000000)

/// Gets Husk from "launched normally, no executable memory" to "JIT is live".
///
/// On iOS 27 every supported device enforces TXM, so the app cannot grant itself
/// executable memory — only an attached debugger can. That debugger is either
/// StikDebug, which Husk hands the JIT script inline over its URL scheme, or
/// Husk's own built-in StikJIT helper (JITSetup.swift), which runs the same
/// script from an app extension.
enum JITBootstrap {
    private static let log = Logger(subsystem: "com.husk.app", category: "jit")

    enum State: Equatable {
        case unknown
        case waitingForDebugger
        case live
        case failed(String)
    }

    /// Install the trap guard. Must run before anything can execute a `brk`.
    ///
    /// Without this, a `brk` that StikDebug is not there to service is a fatal
    /// SIGTRAP rather than a failed call — the process simply dies.
    static func installTrapGuard() {
        HuskLog.log("jit", "installing brk trap guard")
        husk_ios_jit_install_trap_handler()
        HuskLog.log("jit", "trap guard installed -- an unserviced brk will now "
                         + "return 0 instead of killing the process")
    }

    /// Size QEMU will ask for. Must match tb-size in the phase 1 command line:
    /// a smaller region here means QEMU allocates a second one, at a point where
    /// StikDebug may be long gone.
    // Back to 256 MiB. Raising this to 512 was one of three changes made at
    // once in v15, and v15 was the first build to die inside qemu_init(). The
    // guest RAM -- the other suspect -- has since been shown to map and write
    // cleanly at 6144 MiB, which leaves this. The region itself allocates and
    // passes its selftest at 512; whatever objects is further in, where TCG
    // carves the buffer into per-vCPU regions.
    //
    // 512 MiB since the native runtime runs Minecraft: its main library alone is a 354 MiB image that has to sit in this
    // region, with the stubs the loader places beside it. A larger prewarm is safe for QEMU, which is handed the
    // prewarmed region whenever it is at least what tb-size asks for.
    static let jitBytes = 512 * 1024 * 1024

    /// True once the region is held. The memory budget needs this: after a
    /// prewarm the JIT is already counted in the footprint, so subtracting it
    /// again charges for it twice and cost the guest 256 MiB.
    nonisolated(unsafe) static var prewarmed = false

    /// Take the JIT region now, while StikDebug is definitely still attached.
    ///
    /// StikDebug lets go after a while, and a first run spends a minute
    /// downloading 1.1 GB of guest image before QEMU starts. By the time
    /// qemu_init() asked for memory the debugger had detached, and there is no
    /// recovering from that in-process -- without a debugger there is no
    /// executable memory at all, and asking again later is precisely what does
    /// not work. So claim it first and hold it.
    @discardableResult
    static func prewarm() -> Bool {
        guard isDebuggerAttached else {
            HuskLog.log("jit", "no debugger attached yet; not prewarming")
            return false
        }
        HuskLog.log("jit", "claiming \(jitBytes / (1024 * 1024)) MiB of JIT memory now, "
                         + "before the guest download -- StikDebug does not stay attached")
        let ok = husk_ios_jit_prewarm(jitBytes)
        if ok {
            prewarmed = true
            lastFailure = nil
            detachIfDone()
        }
        else if mapJITWorks {
            // Not a failure worth reporting: this is the ordinary shape of an
            // iOS that does not need a trap servicer. QEMU maps its own buffer
            // with MAP_JIT a moment later and runs exactly as well.
            lastFailure = nil
            HuskLog.log("jit", "no trap servicer, but MAP_JIT executes here -- "
                             + "QEMU will map its own buffer")
        } else {
            lastFailure = "The debugger is attached but is not answering trap "
                        + "requests, and this device will not execute a MAP_JIT "
                        + "mapping either, so no executable memory could be "
                        + "claimed. This is what happens when Husk runs inside "
                        + "another container app rather than sideloaded on its own."
        }
        HuskLog.log("jit", ok ? "JIT region secured; it will be handed to QEMU later"
                              : "JIT prewarm FAILED -- StikDebug is not servicing traps")
        return ok
    }

    /// Whether to leave StikDebug attached after the region is held.
    ///
    /// While a debugger is attached, every stop event (a `brk`, a signal, a
    /// Mach exception) halts the whole process until StikDebug answers. iOS
    /// suspends StikDebug soon after it hands the foreground back, so the next
    /// stop freezes Husk with nothing in the log. On iOS 26 that looks like
    /// Android hanging on its boot screen. The RX pages stay valid after detach
    /// (AetherPS4 runs this way on TXM devices), so detaching once the region is
    /// held removes the debugger from the picture.
    static var keepDebuggerAttached: Bool {
        get { UserDefaults.standard.bool(forKey: "husk.keepDebuggerAttached") }
        set { UserDefaults.standard.set(newValue, forKey: "husk.keepDebuggerAttached") }
    }

    /// True once this process has told StikDebug to let go.
    nonisolated(unsafe) static private(set) var detached = false

    /// Release the debugger once the JIT region is held. It is no longer
    /// needed for anything, and leaving it attached is what froze the app.
    private static func detachIfDone() {
        guard prewarmed, !detached, !keepDebuggerAttached else { return }
        HuskLog.log("jit", "JIT region held; detaching StikDebug so a suspended "
                         + "debugger cannot stall the process later")
        husk_ios_jit_detach()
        detached = true
    }

    /// Whether this device enforces TXM, so that only a trap servicer can grant
    /// executable memory. Uses the same rule as the "TXM expected" line in the
    /// log banner, so the two can never disagree.
    static let deviceEnforcesTXM: Bool = HuskLog.expectsTXM(model: HuskLog.deviceModel)

    /// Why the last prewarm failed, for the UI to show.
    ///
    /// CS_DEBUGGED being set is not the same as the debugger servicing traps.
    /// A build running inside LiveContainer reports itself debugged, answers no
    /// brk, and gets no executable memory -- and Husk started QEMU anyway,
    /// which segfaulted inside qemu_init() with a perfectly healthy 4 GB of
    /// headroom. The crash looked like memory pressure and was nothing of the
    /// kind.
    nonisolated(unsafe) static var lastFailure: String?

    /// Whether a plain MAP_JIT mapping executes in this process.
    ///
    /// Not run on a TXM device: MAP_JIT memory cannot execute there, and the
    /// probe is not harmless. With a debugger attached, its fault goes to the
    /// debugger as a stop, not to the probe's signal guard. If StikDebug is not
    /// answering, that stop freezes the app. Views must never trigger this probe
    /// directly.
    static var mapJITWorks: Bool {
        if let known = mapJITResult { return known }
        let result: Bool
        if deviceEnforcesTXM {
            HuskLog.log("jit", "TXM device: MAP_JIT cannot execute here, not probing")
            result = false
        } else {
            result = husk_ios_jit_mapjit_works()
        }
        mapJITResult = result
        return result
    }

    /// The MAP_JIT answer if it has been worked out, without working it out.
    /// For display only: views must never trigger the probe.
    nonisolated(unsafe) static private(set) var mapJITResult: Bool?

    /// True only after a JIT region has been allocated AND passed the execute
    /// self-test — which happens inside `qemu_init`. It is therefore always false
    /// before the guest starts, and must NOT be used to decide whether to start it.
    /// Use `isDebuggerAttached` for that, and this afterwards to confirm it worked.
    static var isLive: Bool { husk_ios_jit_is_available() }

    /// The actual precondition for starting the guest: StikDebug has attached.
    ///
    /// Checked with csops/CS_DEBUGGED rather than the `brk #0x69` probe, because
    /// this costs nothing and is safe to call repeatedly — the brk probe traps
    /// every time, and every trap is a chance to hit a moment when StikDebug is
    /// not listening. The brk probe still runs once, inside the allocator, where
    /// its answer is immediately acted on.
    static var isDebuggerAttached: Bool {
        guard let flags = csStatus() else {
            HuskLog.log("jit", "csops failed (errno \(errno)); assuming no debugger")
            return false
        }
        let attached = (flags & CS_DEBUGGED) != 0
        HuskLog.log("jit", String(format: "csops status = 0x%08x, CS_DEBUGGED = %@",
                                  flags, attached ? "set" : "clear"))
        return attached
    }

    /// `isDebuggerAttached` without the log line, for polling while the
    /// built-in helper attaches.
    static var debuggedFlag: Bool {
        (csStatus() ?? 0) & CS_DEBUGGED != 0
    }

    private static func csStatus() -> UInt32? {
        var flags: UInt32 = 0
        let rc = withUnsafeMutableBytes(of: &flags) { buf in
            csops(getpid(), CS_OPS_STATUS, buf.baseAddress, buf.count)
        }
        return rc == 0 ? flags : nil
    }

    static var isStikDebugInstalled: Bool {
        URL(string: "stikjit://").map(UIApplication.shared.canOpenURL) ?? false
    }

    /// TrollStore answers enable-jit on the `apple-magnifier` scheme.
    static var isTrollStoreInstalled: Bool {
        URL(string: "apple-magnifier://").map(UIApplication.shared.canOpenURL) ?? false
    }

    /// Whether this copy of Husk was installed by TrollStore (or TrollStore Lite): it leaves a marker file next to the app in
    /// its bundle container (TrollStore's `TS_MARKER`). Only then is it a TrollStore app, which keeps the entitlements it was
    /// built with -- including the memory ones.
    static var isInstalledWithTrollStore: Bool {
        let container = Bundle.main.bundleURL.deletingLastPathComponent()
        return ["_TrollStore", "_TrollStoreLite"].contains {
            FileManager.default.fileExists(atPath: container.appendingPathComponent($0).path)
        }
    }

    /// A rootless jailbreak (Dopamine and its kin) puts its files in /var/jb and lets apps see it.
    static var isJailbroken: Bool { FileManager.default.fileExists(atPath: "/var/jb") }

    /// Whether Husk was already marked as debugged when it was opened, before it asked anything of anyone. That is what a
    /// jailbreak that allows JIT in apps does to every app it launches (Dopamine's "Allow JIT in Apps" setting), and it needs
    /// no hand-off at all. Read once, early (HuskApp.init), so a later attach is not mistaken for it.
    nonisolated(unsafe) static var debuggedAtLaunch = false
    static func noteLaunchState() { debuggedAtLaunch = (csStatus() ?? 0) & CS_DEBUGGED != 0 }

    /// Whether this iOS lets a process that is marked as debugged make executable memory for itself, as opposed to needing a
    /// debugger to hand it out: true before iOS 26, which is when the Trusted Execution Monitor arrived. It is what makes
    /// TrollStore and jailbreaks enough, without StikDebug.
    static var canGrantOwnJIT: Bool {
        if #available(iOS 26, *) { return false }
        return true
    }

    /// husk-jit.js as standard base64, which Built-in StikJIT's custom script takes.
    static var scriptBase64: String? {
        loadScript()?.data(using: .utf8)?.base64EncodedString()
    }

    /// Ask StikDebug to attach to us and run the JIT script.
    ///
    /// This backgrounds Husk — iOS switches to StikDebug, which attaches over the
    /// debugserver protocol, runs the script, and relaunches us. Everything after
    /// this point happens in a *new* foreground pass of the app.
    @MainActor
    static func requestTrollStoreAttach() -> Bool {
        HuskLog.log("jit", "requestTrollStoreAttach() -- handing off to TrollStore")
        guard let bundleID = Bundle.main.bundleIdentifier else { return false }
        
        let url = "apple-magnifier://enable-jit?bundle-id=\(bundleID)"
        guard let launchURL = URL(string: url), UIApplication.shared.canOpenURL(launchURL) else {
            HuskLog.log("jit", "FAIL: cannot open apple-magnifier:// -- TrollStore is not installed")
            return false
        }
        
        HuskLog.log("jit", "opening apple-magnifier:// for bundle \(bundleID)")
        UIApplication.shared.open(launchURL)
        return true
    }

    @MainActor
    static func requestAttach() -> Bool {
        HuskLog.log("jit", "requestAttach() -- handing off to StikDebug")

        guard let bundleID = Bundle.main.bundleIdentifier else {
            HuskLog.log("jit", "FAIL: no bundle identifier")
            return false
        }

        var url = "stikjit://enable-jit?bundle-id=\(bundleID)"

        // Sending the script inline makes Husk self-contained. StikDebug accepts
        // script-data unconditionally (HomeView.handleExternalURL), so we do not
        // depend on the user having assigned a script to Husk in StikDebug's UI.
        if let script = loadScript(), let encoded = base64URLEncode(script) {
            HuskLog.log("jit", "loaded husk-jit.js (\(script.count) chars, "
                             + "\(encoded.count) chars encoded); sending inline")
            url += "&script-data=\(encoded)"
        } else {
            HuskLog.log("jit", "WARNING: could not load husk-jit.js from the bundle. "
                             + "Falling back to whatever script StikDebug has assigned "
                             + "to this bundle id -- if none, JIT will not be enabled.")
        }

        guard let launchURL = URL(string: url) else {
            HuskLog.log("jit", "FAIL: malformed StikDebug URL (\(url.count) chars)")
            return false
        }

        guard UIApplication.shared.canOpenURL(launchURL) else {
            HuskLog.log("jit", "FAIL: cannot open stikjit:// -- StikDebug is not "
                             + "installed, or LSApplicationQueriesSchemes is missing "
                             + "the 'stikjit' entry in Info.plist")
            return false
        }

        HuskLog.log("jit", "opening stikjit:// for bundle \(bundleID); "
                         + "Husk will be backgrounded and relaunched after attach")
        UIApplication.shared.open(launchURL)
        return true
    }

    private static func loadScript() -> String? {
        // Accept either bundle layout: the build copies husk-jit.js to the bundle
        // root, but a folder-reference build phase would put it under Resources/.
        let candidates = [
            Bundle.main.path(forResource: "husk-jit", ofType: "js"),
            Bundle.main.path(forResource: "husk-jit", ofType: "js", inDirectory: "Resources"),
        ]
        for case let path? in candidates {
            if let s = try? String(contentsOfFile: path, encoding: .utf8) { return s }
        }
        return nil
    }

    /// StikDebug expects base64url (`-`/`_`, no padding) and percent-encodes it
    /// into the query string.
    private static func base64URLEncode(_ s: String) -> String? {
        guard let data = s.data(using: .utf8) else { return nil }
        let b64 = data.base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .trimmingCharacters(in: CharacterSet(charactersIn: "="))
        return b64.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed)
    }
}
