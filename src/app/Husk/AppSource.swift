import Foundation

struct AppSource: Codable, Identifiable, Equatable {
    let name: String
    let identifier: String
    let apps: [SourceApp]
    
    var id: String { identifier }
}

struct SourceApp: Codable, Identifiable, Equatable {
    let name: String
    let bundleIdentifier: String
    let version: String
    let downloadURL: String
    let iconURL: String
    let localizedDescription: String
    
    var id: String { bundleIdentifier }
}

@MainActor
final class SourceManager: ObservableObject {
    static let shared = SourceManager()
    
    @Published var sources: [AppSource] = []
    @Published var isLoading = false
    @Published var error: String? = nil
    
    // A mapping of bundleIdentifier -> download progress (0.0 to 1.0)
    @Published var downloadProgress: [String: Double] = [:]
    
    // The URLs the user has added. Hardcoded default.
    @Published var sourceURLs: [String] = [
        "https://raw.githubusercontent.com/Leviidev/Husk/main/catalog/source.json"
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
                let source = try JSONDecoder().decode(AppSource.self, from: data)
                fetched.append(source)
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
                
                // Move file to a temporary .apk file
                let tempDir = FileManager.default.temporaryDirectory
                let apkURL = tempDir.appendingPathComponent("\(app.bundleIdentifier)-\(app.version).apk")
                
                try? FileManager.default.removeItem(at: apkURL)
                do {
                    try FileManager.default.moveItem(at: localURL, to: apkURL)
                    HuskLog.log("sources", "Download complete, installing \(app.name)")
                    AndroidHost.shared.install([apkURL])
                } catch {
                    HuskLog.log("sources", "Failed to move APK: \(error)")
                }
            }
        }
        
        // Use an observer for progress
        let observation = task.progress.observe(\.fractionCompleted) { progress, _ in
            Task { @MainActor in
                self.downloadProgress[app.bundleIdentifier] = progress.fractionCompleted
            }
        }
        
        // Retain observation by attaching it to the task via associated objects or just fire and forget
        // URLSession downloadTask retains its progress object.
        task.resume()
    }
}
