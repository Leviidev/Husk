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

    private init() {
        if let stored = Self.loadStored(), let opened = try? Self.open(stored.p12, stored.password).get() {
            // Migrate a previously saved container if this iOS version needs the compatibility path.
            guard opened.p12 == stored.p12 || Self.store(opened.p12, opened.password) else {
                lastError = "The certificate could not be saved in the Keychain."
                return
            }
            info = opened.info
        }
    }

    // MARK: - what the signer needs

    /// The certificate and password, for signing (read from the Keychain each time).
    nonisolated static func credentials() -> (p12: Data, password: String, tag: String)? {
        guard let s = loadStored() else { return nil }
        return (s.p12, s.password, tag(s.p12))
    }

    // MARK: - importing

    /// A .p12 file and its password (empty when it has none).
    func importP12(_ data: Data, password: String, fromStore: Bool = false) {
        HuskLog.log("sign", "opening .p12: \(data.count) bytes")
        let opened: Opened
        switch Self.open(data, password) {
        case .success(let value): opened = value
        case .failure(let failure):
            lastError = failure.message + (fromStore
                ? " You can also import a .p12 exported from the identity that installed Husk." : "")
            return
        }
        if let team = Self.installedTeamID, team != opened.info.teamID {
            lastError = "This certificate belongs to team \(opened.info.teamID), but Husk was installed with team \(team). "
                      + "Use the certificate of the Apple ID that installed Husk."
            return
        }
        guard Self.store(opened.p12, opened.password) else { lastError = "The certificate could not be saved in the Keychain."; return }
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
        HuskLog.log("sign", "store certificate: \(data.count) bytes")
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
    /// Keep the exact password, including significant whitespace. Store callbacks may need
    /// compatibility variants; the native importer also handles an empty-password export.
    private nonisolated static func passwordCandidates(_ given: String) -> [String] {
        var out: [String] = []
        func add(_ s: String) { if !out.contains(s) { out.append(s) } }
        let trimmed = given.trimmingCharacters(in: .whitespacesAndNewlines)
        add(given)
        add(trimmed)
        add(trimmed.replacingOccurrences(of: " ", with: "+"))
        if let decoded = trimmed.removingPercentEncoding { add(decoded) }
        add("")
        return out
    }

    private typealias Opened = (info: Info, password: String, p12: Data)

    private struct OpenFailure: Error {
        let detail: String
        let mac: Int32

        var message: String {
            if mac == 1 {
                return "The password was verified, but iOS could not import the signing identity. "
                     + "Export the certificate with its private key as a new .p12 and try again. \(detail)"
            }
            if mac == 0 {
                return "The password did not match the .p12 integrity check, or the file was modified. "
                     + "Check the password or export the identity again. \(detail)"
            }
            return "The .p12 could not be imported. Check its password and that it contains the certificate and its private key. \(detail)"
        }
    }

    private nonisolated static func open(_ p12: Data, _ password: String) -> Result<Opened, OpenFailure> {
        var failure: OpenFailure?
        for (index, candidate) in passwordCandidates(password).enumerated() {
            var team = [CChar](repeating: 0, count: 64), err = [CChar](repeating: 0, count: 512)
            var notAfter: Double = 0
            let ok = p12.withUnsafeBytes { b in
                husk_p12_info(b.bindMemory(to: UInt8.self).baseAddress, b.count, candidate, &team, team.count, &notAfter, &err, err.count)
            }
            if ok {
                return .success((Info(teamID: String(cString: team), expires: Date(timeIntervalSince1970: notAfter), tag: tag(p12)), candidate, p12))
            }
            let mac = p12.withUnsafeBytes { b in
                husk_p12_check_password(b.bindMemory(to: UInt8.self).baseAddress, b.count, candidate)
            }
            let current = OpenFailure(detail: String(cString: err), mac: mac)
            // Only classifications and OSStatus values enter logs; never passwords or key material.
            HuskLog.log("sign", "p12 candidate \(index): MAC=\(mac); \(current.detail)")
            if mac == 1 {
                var bytes: UnsafeMutablePointer<UInt8>?
                var count = 0
                let converted = p12.withUnsafeBytes { b in
                    husk_p12_copy_compatible(b.bindMemory(to: UInt8.self).baseAddress, b.count, candidate, &bytes, &count)
                }
                if converted, let bytes {
                    let compatible = Data(bytesNoCopy: bytes, count: count, deallocator: .free)
                    err = [CChar](repeating: 0, count: 512)
                    let imported = compatible.withUnsafeBytes { b in
                        husk_p12_info(b.bindMemory(to: UInt8.self).baseAddress, b.count, candidate, &team, team.count, &notAfter, &err, err.count)
                    }
                    if imported {
                        HuskLog.log("sign", "PKCS#12 compatibility import succeeded; using the repacked identity for signing")
                        return .success((Info(teamID: String(cString: team), expires: Date(timeIntervalSince1970: notAfter), tag: tag(compatible)), candidate, compatible))
                    }
                    let detail = String(cString: err)
                    HuskLog.log("sign", "PKCS#12 compatibility import failed: \(detail)")
                    return .failure(OpenFailure(detail: detail, mac: mac))
                }
                HuskLog.log("sign", "PKCS#12 compatibility export unavailable for this container")
                HuskLog.log("sign", "certificate not usable: password verified; \(current.detail)")
                return .failure(current)
            }
            if failure == nil { failure = current }
        }
        let result = failure ?? OpenFailure(detail: "No password candidate was checked.", mac: -1)
        HuskLog.log("sign", "certificate not usable: \(result.detail)")
        return .failure(result)
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
