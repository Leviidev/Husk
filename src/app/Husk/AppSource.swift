import Foundation

// MARK: - Internal Model
struct AppSource: Identifiable, Equatable {
    let name: String
    let identifier: String
    var apps: [SourceApp]
    var id: String { identifier }
}

struct SourceApp: Identifiable, Equatable {
    let name: String
    let bundleIdentifier: String
    let version: String
    let downloadURL: String
    let iconURL: String
    let localizedDescription: String
    var id: String { bundleIdentifier }
}

// MARK: - F-Droid v1 Schema
private struct FDroidIndex: Codable {
    let repo: FDroidRepo
    let apps: [FDroidApp]
    let packages: [String: [FDroidPackage]]
}
private struct FDroidRepo: Codable { let name: String; let address: String }
private struct FDroidApp: Codable {
    let packageName: String
    let name: String?
    let summary: String?
    let description: String?
    let icon: String?
    let localized: [String: FDroidLocalized]?
}
private struct FDroidLocalized: Codable {
    let name: String?
    let summary: String?
    let description: String?
    let icon: String?
}
private struct FDroidPackage: Codable { let apkName: String; let versionName: String }

// MARK: - Husk Simple Schema
private struct HuskSimpleSource: Codable {
    let name: String; let identifier: String; let apps: [SourceAppCodable]
}
private struct SourceAppCodable: Codable {
    let name, bundleIdentifier, version, downloadURL, iconURL, localizedDescription: String
}

// MARK: - Manager
@MainActor
final class SourceManager: ObservableObject {
    static let shared = SourceManager()

    @Published var sources: [AppSource] = []
    @Published var loadingSources: Set<String> = Set<String>()
    @Published var fetchErrors: [String: String] = [String: String]()
    @Published var downloadProgress: [String: Double] = [String: Double]()

    var isLoading: Bool { !loadingSources.isEmpty }

    @Published var sourceURLs: [String] = [
        "https://f-droid.org/repo/index-v1.json"
    ] {
        didSet {
            UserDefaults.standard.set(sourceURLs, forKey: "HuskSourceURLs")
        }
    }

    private let session: URLSession = {
        let cfg = URLSessionConfiguration.default
        cfg.timeoutIntervalForRequest = 120
        cfg.timeoutIntervalForResource = 300
        return URLSession(configuration: cfg)
    }()

    init() {
        if let saved = UserDefaults.standard.stringArray(forKey: "HuskSourceURLs"), !saved.isEmpty {
            sourceURLs = saved
        }
    }

    // Fetch everything (called on first appear / manual refresh)
    func fetchSources() async {
        for urlString in sourceURLs {
            if !sources.contains(where: { $0.identifier == urlString }) || loadingSources.isEmpty {
                await fetchSource(urlString: urlString)
            }
        }
    }

    // Add a new source and fetch ONLY that one
    func addSource(urlString: String) async {
        guard !sourceURLs.contains(urlString) else { return }
        sourceURLs.append(urlString)
        await fetchSource(urlString: urlString)
    }

    // Fetch a single source by URL
    func fetchSource(urlString: String) async {
        guard let url = URL(string: urlString) else {
            fetchErrors[urlString] = "Invalid URL"
            return
        }

        loadingSources.insert(urlString)
        fetchErrors.removeValue(forKey: urlString)

        defer { loadingSources.remove(urlString) }

        do {
            let (data, _) = try await session.data(from: url)

            // Try F-Droid v1 format
            if let fdroid = try? JSONDecoder().decode(FDroidIndex.self, from: data) {
                let baseURL = fdroid.repo.address
                var apps: [SourceApp] = []
                for fApp in fdroid.apps {
                    guard let pkgs = fdroid.packages[fApp.packageName], let latest = pkgs.first else { continue }
                    let loc = fApp.localized?["en-US"] ?? fApp.localized?.values.first
                    let appName = fApp.name ?? loc?.name ?? fApp.packageName
                    let appSummary = fApp.summary ?? loc?.summary ?? fApp.description ?? loc?.description ?? ""
                    let appIcon = fApp.icon ?? loc?.icon
                    let iconURL = appIcon.map { "\(baseURL)/icons/\($0)" } ?? ""
                    
                    apps.append(SourceApp(
                        name: appName,
                        bundleIdentifier: fApp.packageName,
                        version: latest.versionName,
                        downloadURL: "\(baseURL)/\(latest.apkName)",
                        iconURL: iconURL,
                        localizedDescription: appSummary
                    ))
                }
                apps.sort { $0.name.lowercased() < $1.name.lowercased() }
                let source = AppSource(name: fdroid.repo.name, identifier: urlString, apps: apps)
                upsert(source: source)
            }
            // Fallback: Husk simple format
            else if let simple = try? JSONDecoder().decode(HuskSimpleSource.self, from: data) {
                let apps = simple.apps.map {
                    SourceApp(name: $0.name, bundleIdentifier: $0.bundleIdentifier,
                              version: $0.version, downloadURL: $0.downloadURL,
                              iconURL: $0.iconURL, localizedDescription: $0.localizedDescription)
                }
                let source = AppSource(name: simple.name, identifier: urlString, apps: apps)
                upsert(source: source)
            } else {
                fetchErrors[urlString] = "Unrecognized source format"
                HuskLog.log("sources", "Unrecognized format at \(urlString)")
            }
        } catch {
            fetchErrors[urlString] = error.localizedDescription
            HuskLog.log("sources", "Failed to fetch \(urlString): \(error)")
        }
    }

    func removeSource(urlString: String) {
        sourceURLs.removeAll { $0 == urlString }
        sources.removeAll { $0.identifier == urlString }
        fetchErrors.removeValue(forKey: urlString)
    }

    private func upsert(source: AppSource) {
        if let idx = sources.firstIndex(where: { $0.identifier == source.identifier }) {
            sources[idx] = source
        } else {
            sources.append(source)
        }
    }

    // MARK: - APK Download + Install

    func downloadAndInstall(app: SourceApp) {
        guard let url = URL(string: app.downloadURL) else { return }
        downloadProgress[app.bundleIdentifier] = 0.01
        HuskLog.log("sources", "Downloading \(app.name)")

        let task = URLSession.shared.downloadTask(with: url) { localURL, _, error in
            Task { @MainActor in
                self.downloadProgress.removeValue(forKey: app.bundleIdentifier)
                guard let localURL, error == nil else {
                    HuskLog.log("sources", "Download failed: \(String(describing: error))")
                    return
                }
                let dest = FileManager.default.temporaryDirectory
                    .appendingPathComponent("\(app.bundleIdentifier)-\(app.version).apk")
                try? FileManager.default.removeItem(at: dest)
                if (try? FileManager.default.moveItem(at: localURL, to: dest)) != nil {
                    AndroidHost.shared.install([dest])
                }
            }
        }
        task.progress.observe(\.fractionCompleted) { progress, _ in
            Task { @MainActor in self.downloadProgress[app.bundleIdentifier] = progress.fractionCompleted }
        }
        task.resume()
    }
}
