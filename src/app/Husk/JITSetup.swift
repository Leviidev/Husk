// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import SwiftUI
import UIKit

/// How Husk gets a debugger attached. See docs/06-built-in-jit.md.
enum JITMethod: String, CaseIterable, Identifiable {
    /// StikDebug when it is installed, then TrollStore, otherwise Built-in StikJIT.
    case automatic
    case stikDebug
    case trollStore
    case jailbreak
    case builtIn

    var id: String { rawValue }
    /// A localized `String` rather than a key: the picker draws it, the button
    /// that enables JIT interpolates it into a sentence, and the log names it.
    var title: String {
        switch self {
        case .automatic: return localizedString("Automatic")
        case .stikDebug: return localizedString("StikDebug")
        case .trollStore: return localizedString("TrollStore")
        case .jailbreak: return localizedString("Jailbreak (Dopamine)")
        case .builtIn: return localizedString("Built-in StikJIT")
        }
    }
}

/// Where Built-in StikJIT's pairing file came from.
enum JITPairingSource: String {
    /// Made by Husk on this device (iOS 27, OnDevicePairing).
    case onDevice
    /// Imported from a file made on a computer.
    case imported
}

/// The RPPairing file Built-in StikJIT authenticates with. Lives in Documents,
/// so it is visible through Finder file sharing -- it is device-sensitive and
/// is only ever sent to Husk's own helper process.
enum JITPairingFileStore {
    static var directory: URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("StikJIT", isDirectory: true)
    }
    static var url: URL { directory.appendingPathComponent("pairingFile.plist") }
    static var exists: Bool { FileManager.default.fileExists(atPath: url.path) }
    private static let sourceKey = "husk.jitPairingSource"

    static var source: JITPairingSource? {
        guard exists else { return nil }
        return UserDefaults.standard.string(forKey: sourceKey).flatMap(JITPairingSource.init(rawValue:)) ?? .imported
    }

    static func data() throws -> Data { try Data(contentsOf: url) }

    static func importFile(from source: URL) throws {
        let scoped = source.startAccessingSecurityScopedResource()
        defer { if scoped { source.stopAccessingSecurityScopedResource() } }
        try store(try Data(contentsOf: source), source: .imported)
    }

    /// Validates an RPPairing plist and makes it the pairing file.
    static func store(_ data: Data, source: JITPairingSource) throws {
        guard !data.isEmpty,
              let plist = try? PropertyListSerialization.propertyList(from: data, options: [], format: nil),
              let dictionary = plist as? [String: Any],
              let publicKey = dictionary["public_key"] as? Data, publicKey.count == 32,
              let privateKey = dictionary["private_key"] as? Data, privateKey.count == 32,
              let identifier = dictionary["identifier"] as? String, !identifier.isEmpty else {
            throw NSError(domain: "HuskJIT", code: 10,
                          userInfo: [NSLocalizedDescriptionKey:
                            localizedString("That is not a remote pairing file. Make one with the StikDebug pairing-file guide and try again.")])
        }
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        try data.write(to: url, options: .atomic)
        UserDefaults.standard.set(source.rawValue, forKey: sourceKey)
    }
}

@MainActor
final class JITCoordinator: ObservableObject {
    static let shared = JITCoordinator()

    /// Why the built-in helper could not reach this device, read from its error.
    enum ConnectionProblem: Equatable {
        /// Nothing answered at LocalDevVPN's address: the VPN is off or not routing.
        case vpn
        /// The device closed the connection, usually because it no longer accepts
        /// this pairing (every new pairing replaces the last).
        case pairing

        init?(helperMessage: String) {
            let text = helperMessage.lowercased()
            let has = { (needles: [String]) in needles.contains(where: text.contains) }
            if has(["connectionreset", "connection reset", "device refused connection"]) {
                self = .pairing
            } else if has(["connectionrefused", "connection refused", "timedout", "timed out",
                           "networkunreachable", "network unreachable", "hostunreachable",
                           "host unreachable", "no route"]) {
                self = .vpn
            } else {
                return nil
            }
        }

        var message: String {
            switch self {
            case .vpn:
                return localizedString("Husk couldn't reach this device. Connect LocalDevVPN, then try again.")
            case .pairing:
                return localizedString("This device closed the connection, which usually means it no longer accepts "
                     + "Husk's pairing. Pair again, and check that LocalDevVPN is connected.")
            }
        }
    }

    @Published var method: JITMethod {
        didSet { UserDefaults.standard.set(method.rawValue, forKey: "husk.jitMethod") }
    }
    @Published var showSetup = false
    @Published private(set) var connectionProblem: ConnectionProblem?
    @Published private(set) var busy = false
    @Published private(set) var status: String?
    @Published private(set) var error: String?
    @Published private(set) var txmPresent: Bool?
    /// The setup check passed for the current pairing file.
    @Published private(set) var prepared = false
    @Published private(set) var pairingSource = JITPairingFileStore.source
    /// Bumped each time the built-in helper attaches. Husk stays in the
    /// foreground for that, so nothing else tells ContentView to claim the region.
    @Published private(set) var attachGeneration = 0

    var hasPairing: Bool { pairingSource != nil }

    private init() {
        method = UserDefaults.standard.string(forKey: "husk.jitMethod")
            .flatMap(JITMethod.init(rawValue:)) ?? .automatic
    }

    var resolvedMethod: JITMethod {
        guard method == .automatic else { return method }
        if JITBootstrap.isStikDebugInstalled { return .stikDebug }
        if JITBootstrap.isTrollStoreInstalled { return .trollStore }
        if JITBootstrap.isJailbroken { return .jailbreak }
        return .builtIn
    }

    var automaticDescription: String {
        switch resolvedMethod {
        case .stikDebug: return localizedString("StikDebug is installed, so Husk will open it.")
        case .trollStore: return localizedString("TrollStore is installed, so Husk will ask it to enable JIT.")
        case .jailbreak: return localizedString("This device is jailbroken, so JIT comes from the jailbreak's Allow JIT in Apps setting.")
        default: return localizedString("StikDebug, TrollStore and a jailbreak were not found, so Husk will use its built-in helper.")
        }
    }

    func refreshPairingStatus() {
        pairingSource = JITPairingFileStore.source
    }

    private func log(_ line: String) { HuskLog.log("jit", line) }

    /// Get a debugger attached with whichever method applies, or open the setup
    /// walkthrough when that method is not set up yet.
    func enable() {
        guard !JITBootstrap.isDebuggerAttached else { return }
        error = nil
        connectionProblem = nil
        switch resolvedMethod {
        case .automatic:
            assertionFailure("Automatic must resolve to a concrete JIT method")
        case .stikDebug:
            if !JITBootstrap.requestAttach(), !JITBootstrap.requestTrollStoreAttach() {
                error = localizedString("StikDebug is not installed. Install it, or set up Built-in StikJIT.")
                showSetup = true
            }
        case .trollStore:
            if JITBootstrap.requestTrollStoreAttach() {
                awaitTrollStore()
            } else {
                error = localizedString("TrollStore could not be opened. Install Husk through TrollStore, or choose another method.")
                showSetup = true
            }
        case .jailbreak:
            // Nothing to ask: a jailbreak marks an app as debugged as it opens, if it has been told to. Say what to turn on.
            error = localizedString("Turn on Allow JIT in Apps in Dopamine's settings, then open Husk again.")
            showSetup = true
        case .builtIn:
            guard HuskBuiltInJIT.isAvailable, hasPairing else {
                log("built-in JIT is not set up; opening the walkthrough")
                showSetup = true
                return
            }
            enableBuiltIn()
        }
    }

    /// After TrollStore is asked: it opens Husk again and attaches to it for a moment, which marks the process as debugged. Watch for
    /// that, so the Games tab and the Library notice without waiting for another trip to the foreground, and say what to check if
    /// it never comes -- TrollStore only answers enable-jit once its URL Scheme setting is on.
    private func awaitTrollStore() {
        busy = true
        status = localizedString("Waiting for TrollStore to enable JIT…")
        log("waiting for TrollStore's attach")
        _ = Self.waitForDebugger(timeout: 60) { [weak self] attached in
            guard let self else { return }
            busy = false
            if attached {
                status = localizedString("JIT is on.")
                error = nil
                attachGeneration += 1
                log("TrollStore enabled JIT")
            } else {
                status = nil
                error = localizedString("TrollStore did not enable JIT. In TrollStore's Settings turn on URL Scheme, and make sure Husk was "
                      + "installed through TrollStore (or is signed with get-task-allow).")
                log("TrollStore did not enable JIT within a minute")
                showSetup = true
            }
        }
    }

    func importPairingFile(_ source: URL) {
        do {
            try JITPairingFileStore.importFile(from: source)
            OnDevicePairing.shared.cancel()
            refreshPairingStatus()
            method = .builtIn
            prepared = false
            status = localizedString("Pairing file imported.")
            error = nil
            log("pairing file imported")
        } catch {
            self.error = error.localizedDescription
        }
    }

    /// OnDevicePairing finished: its file becomes the pairing file and Built-in StikJIT the method.
    func storeOnDevicePairing(_ data: Data) throws {
        try JITPairingFileStore.store(data, source: .onDevice)
        refreshPairingStatus()
        method = .builtIn
        prepared = false
        status = localizedString("Paired on this device.")
        error = nil
    }

    /// Check LocalDevVPN and the Developer Disk Image, mounting it when needed.
    func prepareBuiltIn() {
        guard let pairing = builtInPairing() else { return }
        busy = true
        error = nil
        connectionProblem = nil
        status = localizedString("Checking LocalDevVPN and the Developer Disk Image…")
        HuskBuiltInJIT.send(.prepare(pairingData: pairing)) { [weak self] result in
            guard let self else { return }
            busy = false
            switch result {
            case .success(let response):
                txmPresent = response.txmPresent
                prepared = response.success
                status = response.success ? response.message : nil
                error = response.success ? nil : helperFailure(response.message)
                log("setup check \(response.success ? "passed" : "failed"): \(response.message)")
            case .failure(let failure):
                prepared = false
                status = nil
                error = failure.localizedDescription
            }
        }
    }

    func enableBuiltIn() {
        guard let pairing = builtInPairing() else { return }
        guard let script = JITBootstrap.scriptBase64 else {
            error = localizedString("Husk's JIT script is missing from this installation. Reinstall Husk.")
            return
        }

        busy = true
        error = nil
        connectionProblem = nil
        status = localizedString("Starting Husk's JIT helper…")
        log("enabling JIT with the built-in helper")
        var readiness: Timer?
        var finished = false
        let finish: (String?) -> Void = { [weak self] failure in
            guard let self, !finished else { return }
            finished = true
            readiness?.invalidate()
            busy = false
            if let failure {
                status = nil
                error = failure
                log("built-in JIT failed: \(failure)")
                showSetup = true
            } else {
                status = localizedString("JIT is on.")
                error = nil
                attachGeneration += 1
                log("built-in helper attached")
            }
        }

        HuskBuiltInJIT.send(
            .enable(targetPID: getpid(), pairingData: pairing, scriptBase64: script),
            started: { [weak self] in
                self?.status = localizedString("Waiting for the helper to attach…")
                readiness = Self.waitForDebugger { attached in
                    finish(attached ? nil : localizedString("The helper did not attach within 90 seconds. "
                                          + "Check that LocalDevVPN is connected, then try again."))
                }
            },
            completion: { [weak self] result in
                // The helper replies once Husk detaches, or when attaching failed.
                switch result {
                case .success(let response):
                    self?.txmPresent = response.txmPresent
                    HuskLog.log("jit", "helper: \(response.message)")
                    if !response.success {
                        finish(self?.helperFailure(response.message) ?? response.message)
                    }
                case .failure(let failure):
                    finish(failure.localizedDescription)
                }
            })
    }

    func resetDDI() {
        busy = true
        error = nil
        status = localizedString("Resetting the Developer Disk Image cache…")
        HuskBuiltInJIT.send(.resetDDI) { [weak self] result in
            guard let self else { return }
            busy = false
            prepared = false
            switch result {
            case .success(let response):
                status = response.success ? response.message : nil
                error = response.success ? nil : response.message
            case .failure(let failure):
                status = nil
                error = failure.localizedDescription
            }
        }
    }

    private func builtInPairing() -> Data? {
        if let reason = HuskBuiltInJIT.unavailableReason {
            error = reason
            return nil
        }
        guard let pairing = try? JITPairingFileStore.data() else {
            error = localizedString("Pair this device or import its pairing file first.")
            return nil
        }
        return pairing
    }

    /// The helper's failure as shown: a connection problem gets its plain
    /// explanation (the helper's own message is already in the log).
    private func helperFailure(_ message: String) -> String {
        connectionProblem = ConnectionProblem(helperMessage: message)
        return connectionProblem?.message ?? message
    }

    /// Polls CS_DEBUGGED until it is set or `timeout` passes.
    private static func waitForDebugger(timeout: TimeInterval = 90,
                                        completion: @escaping (Bool) -> Void) -> Timer {
        let deadline = Date().addingTimeInterval(timeout)
        let timer = Timer(timeInterval: 0.5, repeats: true) { timer in
            if JITBootstrap.debuggedFlag {
                timer.invalidate()
                completion(true)
            } else if Date() >= deadline {
                timer.invalidate()
                completion(false)
            }
        }
        RunLoop.main.add(timer, forMode: .common)
        return timer
    }
}

/// LocalDevVPN, which Built-in StikJIT and StikDebug reach the device through:
/// open it to connect when it is installed, otherwise its App Store page.
enum LocalDevVPN {
    static let appStore = URL(string: "https://apps.apple.com/us/app/localdevvpn/id6755608044")!
    /// `enable` connects the VPN; `scheme` has LocalDevVPN return to Husk afterwards.
    static let connect = URL(string: "localdevvpn://enable?scheme=husk")!

    static var isInstalled: Bool { UIApplication.shared.canOpenURL(URL(string: "localdevvpn://")!) }
    /// A `String` rather than a key: it is also the name of a button in the
    /// walkthrough, where `Button` localizes it just the same.
    static var actionTitle: String {
        localizedString(isInstalled ? "Connect LocalDevVPN" : "Get LocalDevVPN")
    }

    @MainActor static func open() {
        let installed = isInstalled
        HuskLog.log("jit", "LocalDevVPN \(installed ? "connect" : "app-store")")
        UIApplication.shared.open(installed ? connect : appStore)
    }
}
