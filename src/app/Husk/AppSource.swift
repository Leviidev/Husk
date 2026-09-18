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
struct FDroidIndex: Codable {
    let repo: FDroidRepo
    let apps: [FDroidApp]
    let packages: [String: [FDroidPackage]]
}

struct FDroidRepo: Codable {
    let name: String
    let address: String
}

struct FDroidApp: Codable {
    let packageName: String
    let name: String
    let summary: String?
    let description: String?
    let icon: String?
}

struct FDroidPackage: Codable {
    let apkName: String
    let versionName: String
    let versionCode: Int
}

// MARK: - Husk Simple Schema
struct HuskSimpleSource: Codable {
    let name: String
    let identifier: String
    let apps: [SourceAppCodable]
}

struct SourceAppCodable: Codable {
    let name: String
    let bundleIdentifier: String
    let version: String
    let downloadURL: String
    let iconURL: String
    let localizedDescription: String
}

// MARK: - Manager
@MainActor
final class SourceManager: ObservableObject {
    static let shared = SourceManager()
    
    @Published var sources: [AppSource] = []
    @Published var isLoading = false
    @Published var error: String? = nil
    
    @Published var downloadProgress: [String: Double] = [:]
    
    @Published var sourceURLs: [String] = [
        "https://f-droid.org/repo/index-v1.json"
    ] {
        didSet {
            UserDefaults.standard.set(sourceURLs, forKey: "HuskSourceURLs")
            Task { await fetchSources() }
        }
    }
    
    init() {
        if let saved = UserDefaults.standard.stringArray(forKey: "HuskSourceURLs"), !saved.isEmpty {
            self.sourceURLs = saved
        }
    }
    
    func fetchSources() async {
        isLoading = true
        error = nil
        var fetched: [AppSource] = []
        
        for urlString in sourceURLs {
            guard let url = URL(string: urlString) else { continue }
            do {
                let (data, _) = try await URLSession.shared.data(from: url)
                
                // Try F-Droid format first
                if let fdroid = try? JSONDecoder().decode(FDroidIndex.self, from: data) {
                    var apps: [SourceApp] = []
                    let baseURL = fdroid.repo.address
                    for fApp in fdroid.apps {
                        guard let pkgs = fdroid.packages[fApp.packageName], let latest = pkgs.first else { continue }
                        
                        let iconURL = fApp.icon != nil ? "\(baseURL)/icons/\(fApp.icon!)" : ""
                        let downloadURL = "\(baseURL)/\(latest.apkName)"
                        
                        apps.append(SourceApp(
                            name: fApp.name,
                            bundleIdentifier: fApp.packageName,
                            version: latest.versionName,
                            downloadURL: downloadURL,
                            iconURL: iconURL,
                            localizedDescription: fApp.summary ?? fApp.description ?? ""
                        ))
                    }
                    // Sort apps alphabetically
                    apps.sort { $0.name.lowercased() < $1.name.lowercased() }
                    
                    fetched.append(AppSource(
                        name: fdroid.repo.name,
                        identifier: urlString,
                        apps: apps
                    ))
                }
                // Fallback to simple format
                else if let simple = try? JSONDecoder().decode(HuskSimpleSource.self, from: data) {
                    let apps = simple.apps.map {
                        SourceApp(name: $0.name, bundleIdentifier: $0.bundleIdentifier, version: $0.version, downloadURL: $0.downloadURL, iconURL: $0.iconURL, localizedDescription: $0.localizedDescription)
                    }
                    fetched.append(AppSource(name: simple.name, identifier: simple.identifier, apps: apps))
                } else {
                    HuskLog.log("sources", "Failed to parse \(url) as any known format")
                }
            } catch {
                HuskLog.log("sources", "Failed to fetch \(url): \(error)")
            }
        }
        
        self.sources = fetched
        self.isLoading = false
    }
    
    func downloadAndInstall(app: SourceApp) {
        guard let url = URL(string: app.downloadURL) else { return }
        
        downloadProgress[app.bundleIdentifier] = 0.01
        HuskLog.log("sources", "Starting download for \(app.name)")
        
        let task = URLSession.shared.downloadTask(with: url) { localURL, response, error in
            Task { @MainActor in
                self.downloadProgress.removeValue(forKey: app.bundleIdentifier)
                
                guard let localURL = localURL, error == nil else {
                    HuskLog.log("sources", "Download failed for \(app.name): \(String(describing: error))")
                    return
                }
                
                let tempDir = FileManager.default.temporaryDirectory
                let apkURL = tempDir.appendingPathComponent("\(app.bundleIdentifier)-\(app.version).apk")
                
                try? FileManager.default.removeItem(at: apkURL)
                do {
                    try FileManager.default.moveItem(at: localURL, to: apkURL)
                    AndroidHost.shared.install([apkURL])
                } catch {
                    HuskLog.log("sources", "Failed to move APK: \(error)")
                }
            }
        }
        
        let observation = task.progress.observe(\.fractionCompleted) { progress, _ in
            Task { @MainActor in
                self.downloadProgress[app.bundleIdentifier] = progress.fractionCompleted
            }
        }
        task.resume()
    }
}
