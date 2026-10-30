// SPDX-License-Identifier: GPL-2.0-or-later
import CryptoKit
import Foundation
import Security
import UIKit

/// The developer certificate JIT-less mode signs a game's converted libraries with (JITLess.swift). iOS loads a library in a
/// sideloaded app only when it is signed by the same team as the app, so this has to be the certificate SideStore or AltStore
/// installed Husk with. It comes from the store itself (it hands it over through a URL callback) or from a .p12 file and its
/// password. Kept in the Keychain; nothing leaves the device.
@MainActor
final class SigningCertificate: ObservableObject {
    static let shared = SigningCertificate()

    struct Info: Equatable {
        let teamID: String
        let expires: Date
        /// Names this certificate in the converted libraries' .signer files: a new one means they are signed again.
        let tag: String
    }

    @Published private(set) var info: Info?
    @Published var lastError: String?

    private static let service = "com.husk.signing"
    private static let callbackScheme = "husk"
    private static let callbackHost = "certificate"

    private init() { info = Self.loadStored().flatMap { Self.open($0.p12, $0.password)?.info } }

    // MARK: - what the signer needs

    /// The certificate and password, for signing (read from the Keychain each time).
    nonisolated static func credentials() -> (p12: Data, password: String, tag: String)? {
        guard let s = loadStored() else { return nil }
        return (s.p12, s.password, tag(s.p12))
    }

    // MARK: - importing

    /// A .p12 file and its password (empty when it has none).
    func importP12(_ data: Data, password: String, fromStore: Bool = false) {
        HuskLog.log("sign", "opening .p12: \(data.count) bytes, password length \(password.count)")
        guard let opened = Self.open(data, password) else {
            lastError = fromStore
                ? "The store's certificate could not be opened. If you have a .p12, import that with the password you set when you exported it."
                : (password.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                   ? "That .p12 needs a password. Enter the one you set when you exported it."
                   : "That password did not open the .p12. Check it, or export the certificate again.")
            return
        }
        if let team = Self.installedTeamID, team != opened.info.teamID {
            lastError = "This certificate belongs to team \(opened.info.teamID), but Husk was installed with team \(team). "
                      + "Use the certificate of the Apple ID that installed Husk."
            return
        }
        guard Self.store(data, opened.password) else { lastError = "The certificate could not be saved in the Keychain."; return }
        info = opened.info
        lastError = nil
        HuskLog.log("sign", "certificate imported: team \(opened.info.teamID), expires \(opened.info.expires)")
    }

    /// Ask SideStore or AltStore for the certificate it signs Husk with; it answers by opening husk://certificate?cert=…&password=…
    func requestFromStore(_ store: Store) {
        let callback = "\(Self.callbackScheme)://\(Self.callbackHost)?cert=$(BASE64_CERT)&password=$(PASSWORD)"
        guard let encoded = callback.addingPercentEncoding(withAllowedCharacters: .alphanumerics),
              let url = URL(string: "\(store.scheme)://certificate?callback_template=\(encoded)") else { return }
        HuskLog.log("sign", "asking \(store.title) for its certificate")
        UIApplication.shared.open(url) { [weak self] ok in
            if !ok { Task { @MainActor in self?.lastError = "\(store.title) is not installed, or did not answer." } }
        }
    }

    /// husk://certificate?cert=<base64>&password=<password>, from SideStore or AltStore. False when the URL is not one.
    func handle(_ url: URL) -> Bool {
        guard url.scheme?.lowercased() == Self.callbackScheme, url.host?.lowercased() == Self.callbackHost else { return false }
        // Split the raw query so '+' stays '+' (URLComponents turns it into a space, which breaks base64 and passwords).
        func value(_ name: String) -> String? {
            let raw = Self.rawQueryItems(url)[name]
                ?? URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?
                    .first(where: { $0.name.lowercased() == name })?.value
            guard var s = raw, !s.isEmpty else { return raw }
            if let once = s.removingPercentEncoding { s = once }
            return s
        }
        guard let cert = value("cert"), let data = Self.p12(fromBase64: cert), data.count > 32 else {
            lastError = "The store's answer had no certificate in it."
            return true
        }
        var password = value("password") ?? value("pass") ?? ""
        if password == "$(PASSWORD)" { password = "" }
        HuskLog.log("sign", "store certificate: \(data.count) bytes, password length \(password.count)")
        importP12(data, password: password, fromStore: true)
        return true
    }

    func remove() {
        for account in ["p12", "password"] {
            SecItemDelete([kSecClass: kSecClassGenericPassword, kSecAttrService: Self.service, kSecAttrAccount: account] as CFDictionary)
        }
        info = nil
        HuskLog.log("sign", "certificate removed")
    }

    enum Store: CaseIterable, Identifiable {
        case sideStore, altStore
        var id: Self { self }
        var title: String { self == .sideStore ? "SideStore" : "AltStore" }
        var scheme: String { self == .sideStore ? "sidestore" : "altstore-classic" }
    }

    // MARK: - the team Husk was installed with

    /// From the provisioning profile the sideloader embedded; nil when there is none (TrollStore, a development build).
    nonisolated static var installedTeamID: String? {
        guard let url = Bundle.main.url(forResource: "embedded", withExtension: "mobileprovision"),
              let raw = try? Data(contentsOf: url),
              let start = raw.range(of: Data("<?xml".utf8)), let end = raw.range(of: Data("</plist>".utf8), in: start.lowerBound..<raw.endIndex),
              let plist = try? PropertyListSerialization.propertyList(from: raw[start.lowerBound..<end.upperBound], format: nil) as? [String: Any]
        else { return nil }
        return (plist["TeamIdentifier"] as? [String])?.first
    }

    // MARK: - Keychain and inspection

    /// SideStore percent-encodes the cert; a leftover '+' may still arrive as a space. Also accepts base64url.
    private nonisolated static func p12(fromBase64 raw: String) -> Data? {
        var s = raw.replacingOccurrences(of: " ", with: "+")
        if let again = s.removingPercentEncoding { s = again }
        s = s.replacingOccurrences(of: "-", with: "+").replacingOccurrences(of: "_", with: "/")
            .filter { !$0.isWhitespace }
        let pad = (4 - s.count % 4) % 4
        if pad > 0 { s += String(repeating: "=", count: pad) }
        return Data(base64Encoded: s) ?? Data(base64Encoded: s, options: .ignoreUnknownCharacters)
    }

    private nonisolated static func rawQueryItems(_ url: URL) -> [String: String] {
        let text = url.absoluteString
        guard let q = text.split(separator: "?", maxSplits: 1).dropFirst().first else { return [:] }
        var out: [String: String] = [:]
        for part in q.split(separator: "&") {
            let kv = part.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
            guard let name = kv.first?.lowercased(), !name.isEmpty else { continue }
            out[name] = kv.count > 1 ? String(kv[1]) : ""
        }
        return out
    }

    /// Passwords SideStore/AltStore actually used vs. what the URL claimed.
    /// Feather's p12_password_check: empty passphrase first (PKCS12_verify_mac NULL, 0), then the
    /// string from the UI or the store URL. SideStore often re-saves the bag with no password.
    private nonisolated static func passwordCandidates(_ given: String) -> [String] {
        var out: [String] = []
        func add(_ s: String) { if !out.contains(s) { out.append(s) } }
        let trimmed = given.trimmingCharacters(in: .whitespacesAndNewlines)
        add("")
        add(trimmed)
        add(trimmed.replacingOccurrences(of: " ", with: "+"))
        if let decoded = trimmed.removingPercentEncoding { add(decoded) }
        return out
    }

    private nonisolated static func open(_ p12: Data, _ password: String) -> (info: Info, password: String)? {
        var lastError = ""
        for candidate in passwordCandidates(password) {
            var team = [CChar](repeating: 0, count: 64), err = [CChar](repeating: 0, count: 200)
            var notAfter: Double = 0
            let ok = p12.withUnsafeBytes { b in
                husk_p12_info(b.bindMemory(to: UInt8.self).baseAddress, b.count, candidate, &team, team.count, &notAfter, &err, err.count)
            }
            if ok {
                return (Info(teamID: String(cString: team), expires: Date(timeIntervalSince1970: notAfter), tag: tag(p12)), candidate)
            }
            lastError = String(cString: err)
        }
        HuskLog.log("sign", "certificate not usable: \(lastError)")
        return nil
    }

    private nonisolated static func tag(_ p12: Data) -> String {
        SHA256.hash(data: p12).prefix(8).map { String(format: "%02x", $0) }.joined()
    }

    private nonisolated static func store(_ p12: Data, _ password: String) -> Bool {
        func put(_ account: String, _ value: Data) -> Bool {
            let key: [CFString: Any] = [kSecClass: kSecClassGenericPassword, kSecAttrService: service, kSecAttrAccount: account]
            SecItemDelete(key as CFDictionary)
            var add = key
            add[kSecValueData] = value
            add[kSecAttrAccessible] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
            return SecItemAdd(add as CFDictionary, nil) == errSecSuccess
        }
        return put("p12", p12) && put("password", Data(password.utf8))
    }

    private nonisolated static func loadStored() -> (p12: Data, password: String)? {
        func get(_ account: String) -> Data? {
            var out: CFTypeRef?
            let q: [CFString: Any] = [kSecClass: kSecClassGenericPassword, kSecAttrService: service, kSecAttrAccount: account,
                                      kSecReturnData: true, kSecMatchLimit: kSecMatchLimitOne]
            return SecItemCopyMatching(q as CFDictionary, &out) == errSecSuccess ? out as? Data : nil
        }
        guard let p12 = get("p12") else { return nil }
        return (p12, get("password").flatMap { String(data: $0, encoding: .utf8) } ?? "")
    }
}
