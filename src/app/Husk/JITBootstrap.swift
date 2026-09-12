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
/// executable memory — only an attached debugger can. StikDebug is that debugger.
/// Husk hands it the JIT script inline over its URL scheme, so the user never has
/// to configure anything inside StikDebug for Husk specifically.
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
    static let jitBytes = 256 * 1024 * 1024

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
        if ok { prewarmed = true }
        HuskLog.log("jit", ok ? "JIT region secured; it will be handed to QEMU later"
                              : "JIT prewarm FAILED -- StikDebug is not servicing traps")
        return ok
    }

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
        var flags: UInt32 = 0
        let rc = withUnsafeMutableBytes(of: &flags) { buf in
            csops(getpid(), CS_OPS_STATUS, buf.baseAddress, buf.count)
        }
        if rc != 0 {
            HuskLog.log("jit", "csops failed (errno \(errno)); assuming no debugger")
            return false
        }
        let attached = (flags & CS_DEBUGGED) != 0
        HuskLog.log("jit", String(format: "csops status = 0x%08x, CS_DEBUGGED = %@",
                                  flags, attached ? "set" : "clear"))
        return attached
    }

    /// Ask StikDebug to attach to us and run the JIT script.
    ///
    /// This backgrounds Husk — iOS switches to StikDebug, which attaches over the
    /// debugserver protocol, runs the script, and relaunches us. Everything after
    /// this point happens in a *new* foreground pass of the app.
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
