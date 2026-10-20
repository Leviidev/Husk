// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// Geode, the mod loader for Geometry Dash, for the translation layer to load into the game (husk-tl-geode.c).
///
/// Two files make it work, both downloaded when Geode is turned on in the game's settings:
///   - the Geode release for this version of the game: Geode's own index names it, from the game's versionCode (a release
///     only supports the game versions it was built for -- Geode 4.10.2 is the last for 2.2074, for example);
///   - Geode's Android launcher, for the C++ runtime (libc++_shared.so) that Geode and every mod are built against.
/// Mods themselves are Geode's business: it finds, downloads and loads them from its own button on the game's main menu.
@MainActor
final class GeodeSupport: ObservableObject {
    static let shared = GeodeSupport()
    static let gamePackage = "com.robtopx.geometryjump"
    static let launcherPackage = "com.geode.launcher"

    enum Status: Equatable {
        case idle
        case working(String)
        case ready(String)           // the Geode version
        case failed(String)
    }

    @Published private(set) var status: [String: Status] = [:]

    nonisolated static func folder(_ appID: String) -> URL {
        TranslationLayer.root.appendingPathComponent(appID, isDirectory: true).appendingPathComponent("geode", isDirectory: true)
    }
    nonisolated private static func releaseZip(_ appID: String) -> URL { folder(appID).appendingPathComponent("geode-android64.zip") }
    nonisolated private static func launcherAPK(_ appID: String) -> URL { folder(appID).appendingPathComponent("geode-launcher.apk") }
    nonisolated private static func versionFile(_ appID: String) -> URL { folder(appID).appendingPathComponent("version.txt") }
    /// The loader's resources, a release file of their own since Geode 5 (husk-tl-geode.c looks for it beside the release).
    nonisolated private static func resourcesZip(_ appID: String) -> URL { folder(appID).appendingPathComponent("geode-resources.zip") }
    /// Written once the downloaded launcher has been seen to carry the 64-bit C++ runtime (the 32-bit build does not).
    nonisolated private static func launcherChecked(_ appID: String) -> URL { folder(appID).appendingPathComponent("launcher-arm64.ok") }

    /// Whether an APK carries what Geode needs from the launcher: the 64-bit libc++_shared.so.
    nonisolated static func launcherUsable(_ apk: String) -> Bool {
        TranslationLayerStore.entry(apk, "lib/arm64-v8a/libc++_shared.so", limit: 16 << 20) != nil
    }

    /// What a launch hands the game: both files, when Geode is on for this game and has been downloaded. `appDir` is the
    /// game's folder (Documents/TranslationLayer/<id>).
    nonisolated static func files(appDir: String) -> (zip: String, launcher: String)? {
        let id = (appDir as NSString).lastPathComponent
        guard TLAppSettings.load(id).geode else { return nil }
        let zip = releaseZip(id).path
        guard FileManager.default.fileExists(atPath: zip), let apk = launcherOnDisk(id) else { return nil }
        return (zip, apk)
    }

    /// The launcher APK: the one downloaded for this game, or one added to the library as an app of its own.
    nonisolated private static func launcherOnDisk(_ appID: String) -> String? {
        let mine = launcherAPK(appID).path
        if FileManager.default.fileExists(atPath: mine), FileManager.default.fileExists(atPath: launcherChecked(appID).path) { return mine }
        let root = TranslationLayer.root
        for dir in (try? FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil)) ?? [] {
            let pkg = (try? String(contentsOf: dir.appendingPathComponent("package.txt"), encoding: .utf8))?
                .trimmingCharacters(in: .whitespacesAndNewlines)
            guard pkg == launcherPackage,
                  let apk = (try? FileManager.default.contentsOfDirectory(atPath: dir.path))?.first(where: { $0.hasSuffix(".apk") })
            else { continue }
            let path = dir.appendingPathComponent(apk).path
            if launcherUsable(path) { return path }
        }
        return nil
    }

    func current(_ app: TLApp) -> Status {
        if let s = status[app.id] { return s }
        if let v = try? String(contentsOf: Self.versionFile(app.id), encoding: .utf8),
           FileManager.default.fileExists(atPath: Self.releaseZip(app.id).path),
           FileManager.default.fileExists(atPath: Self.resourcesZip(app.id).path),
           Self.launcherOnDisk(app.id) != nil {
            return .ready(v.trimmingCharacters(in: .whitespacesAndNewlines))
        }
        return .idle
    }

    /// Download what is missing or out of date.
    func prepare(_ app: TLApp) async {
        if case .working = status[app.id] { return }
        status[app.id] = .working(L10n.tr("Finding the Geode release for this game…"))
        do {
            let folder = Self.folder(app.id)
            try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
            guard let apk = app.apks.first,
                  let manifest = TranslationLayerStore.entry(apk, "AndroidManifest.xml", limit: 8 << 20),
                  let versionCode = ApkMetadata.versionCode(manifest) else {
                throw GeodeError(L10n.tr("Could not read this game's version."))
            }

            // Geode's index: the newest Geode release for this game version.
            let (release, zipURL) = try await Self.latestRelease(versionCode: versionCode)
            let have = (try? String(contentsOf: Self.versionFile(app.id), encoding: .utf8))?.trimmingCharacters(in: .whitespacesAndNewlines)
            let haveResources = FileManager.default.fileExists(atPath: Self.resourcesZip(app.id).path)
            if have != release || !FileManager.default.fileExists(atPath: Self.releaseZip(app.id).path) || !haveResources {
                status[app.id] = .working(L10n.tr("Downloading Geode \(release)…"))
                try await Self.download(zipURL, to: Self.releaseZip(app.id))
                // Its resources, from the release on GitHub. Geode 4 kept them inside the release zip and has no such file,
                // so a missing one is not an error.
                try? FileManager.default.removeItem(at: Self.resourcesZip(app.id))
                let tag = release.hasPrefix("v") ? release : "v" + release
                if let res = URL(string: "https://github.com/geode-sdk/geode/releases/download/\(tag)/resources.zip") {
                    do { try await Self.download(res, to: Self.resourcesZip(app.id)) }
                    catch { HuskLog.log("geode", "no separate resources for Geode \(release): \(error.localizedDescription)") }
                }
                if !FileManager.default.fileExists(atPath: Self.resourcesZip(app.id).path) {
                    try Data().write(to: Self.resourcesZip(app.id))      // checked once: this release has none
                }
                try release.write(to: Self.versionFile(app.id), atomically: true, encoding: .utf8)
            }

            if Self.launcherOnDisk(app.id) == nil {
                status[app.id] = .working(L10n.tr("Downloading Geode's launcher…"))
                try? FileManager.default.removeItem(at: Self.launcherChecked(app.id))
                try await Self.download(try await Self.launcherURL(), to: Self.launcherAPK(app.id))
                guard Self.launcherUsable(Self.launcherAPK(app.id).path) else {
                    throw GeodeError(L10n.tr("Geode's launcher download has no 64-bit C++ runtime."))
                }
                try Data().write(to: Self.launcherChecked(app.id))
            }
            status[app.id] = .ready(release)
            HuskLog.log("geode", "Geode \(release) ready for \(app.label) (versionCode \(versionCode))")
        } catch {
            status[app.id] = .failed(error.localizedDescription)
            HuskLog.log("geode", "could not get Geode: \(error.localizedDescription)")
        }
    }

    /// Remove the downloaded files (Geode turned off). Mods and their settings stay, in the game's own data.
    func remove(_ app: TLApp) {
        try? FileManager.default.removeItem(at: Self.folder(app.id))
        status[app.id] = .idle
    }

    // MARK: network

    struct GeodeError: LocalizedError {
        let message: String
        init(_ m: String) { message = m }
        var errorDescription: String? { message }
    }

    private static func json(_ url: URL) async throws -> [String: Any] {
        var req = URLRequest(url: url)
        req.timeoutInterval = 30
        req.setValue("Husk", forHTTPHeaderField: "User-Agent")
        let (data, response) = try await URLSession.shared.data(for: req)
        guard (response as? HTTPURLResponse)?.statusCode == 200,
              let obj = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw GeodeError(L10n.tr("\(url.host ?? "The server") did not answer (HTTP \((response as? HTTPURLResponse)?.statusCode ?? 0))."))
        }
        return obj
    }

    private static func latestRelease(versionCode: Int) async throws -> (String, URL) {
        let url = URL(string: "https://api.geode-sdk.org/v1/loader/versions/latest?gd=\(versionCode)&platform=android&prerelease=false")!
        let obj = try await json(url)
        let payload = obj["payload"] as? [String: Any] ?? obj
        guard let version = payload["version"] as? String,
              let downloads = payload["downloads"] as? [String: Any] else {
            throw GeodeError(L10n.tr("Geode has no release for this version of Geometry Dash."))
        }
        let entry = downloads["android64"]
        let link = (entry as? String) ?? ((entry as? [String: Any])?["url"] as? String)
        guard let link, let zip = URL(string: link) else { throw GeodeError(L10n.tr("Geode's release has no Android build.")) }
        return (version, zip)
    }

    private static func launcherURL() async throws -> URL {
        let obj = try await json(URL(string: "https://api.github.com/repos/geode-sdk/android-launcher/releases/latest")!)
        // The release has a 32-bit build (…-android32.apk) beside the 64-bit one, and lists it first: only the 64-bit one has
        // the arm64 C++ runtime Geode needs.
        let assets = obj["assets"] as? [[String: Any]] ?? []
        let apks = assets.compactMap { $0["browser_download_url"] as? String }.filter { $0.hasSuffix(".apk") }
        guard let link = apks.first(where: { !$0.contains("android32") && !$0.contains("armeabi") }), let url = URL(string: link) else {
            throw GeodeError(L10n.tr("Geode's launcher release has no 64-bit APK."))
        }
        return url
    }

    private static func download(_ url: URL, to dest: URL) async throws {
        var req = URLRequest(url: url)
        req.timeoutInterval = 120
        req.setValue("Husk", forHTTPHeaderField: "User-Agent")
        let (tmp, response) = try await URLSession.shared.download(for: req)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else {
            throw GeodeError(L10n.tr("Download failed (HTTP \((response as? HTTPURLResponse)?.statusCode ?? 0))."))
        }
        try? FileManager.default.removeItem(at: dest)
        try FileManager.default.moveItem(at: tmp, to: dest)
    }
}
