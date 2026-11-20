// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import Combine

/// Manages Google Play authentication, session persistence, searching, browsing, and downloads.
@MainActor
final class PlayStoreManager: ObservableObject {
    static let shared = PlayStoreManager()

    private static let sessionKey = "husk_play_session"
    private let defaults: UserDefaults
    private let searchRequest: (String, PlaySession) async throws -> [PlayAPI.PlayApp]
    private var searchGeneration = UUID()

    @Published var session: PlaySession?
    @Published var isLoading = false
    @Published var statusMessage: String?
    @Published var errorMessage: String?

    // Browse / Search state
    @Published var featuredApps: [PlayAPI.PlayApp] = []
    @Published var searchResults: [PlayAPI.PlayApp] = []
    @Published var isSearching = false

    // Bytes received across the base APK, splits and expansion files. Unknown totals stay indeterminate.
    @Published var downloadProgress: [String: PlayAPI.DownloadProgress] = [:]
    // Installing packages
    @Published var installingPackages: Set<String> = []
    // Running downloads by package, so the Store can stop them; their titles for the Downloads list
    private var downloadTasks: [String: Task<Void, Never>] = [:]
    private var downloadIDs: [String: UUID] = [:]
    @Published private(set) var downloadTitles: [String: String] = [:]

    /// Stops a running download; it ends without an error message.
    func cancelDownload(_ pkg: String) {
        downloadIDs[pkg] = nil
        downloadTasks[pkg]?.cancel()
        downloadTasks[pkg] = nil
        downloadProgress.removeValue(forKey: pkg)
        installingPackages.remove(pkg)
        downloadTitles.removeValue(forKey: pkg)
        HuskLog.log("store", "Cancelled the download of \(pkg)")
    }

    init(defaults: UserDefaults = .standard,
         searchRequest: @escaping (String, PlaySession) async throws -> [PlayAPI.PlayApp] = { try await PlayAPI.search(query: $0, session: $1) }) {
        self.defaults = defaults
        self.searchRequest = searchRequest
        loadSavedSession()
    }

    var isSignedIn: Bool {
        session != nil && !(session?.email.isEmpty ?? true)
    }

    var canSearch: Bool { isSignedIn && !(session?.bearer.isEmpty ?? true) }

    // MARK: - Session Persistence

    private func loadSavedSession() {
        if let data = defaults.data(forKey: Self.sessionKey),
           let s = try? JSONDecoder().decode(PlaySession.self, from: data) {
            self.session = s
            Task {
                await refreshSessionIfNeeded()
            }
        }
    }

    func saveSession(_ s: PlaySession) {
        if session?.email != s.email || session?.gsfID != s.gsfID || session?.isAnonymous != s.isAnonymous { clearSearch() }
        self.session = s
        if let data = try? JSONEncoder().encode(s) {
            defaults.set(data, forKey: Self.sessionKey)
        }
    }

    func signOut() {
        clearSearch()
        for package in Array(downloadTasks.keys) { cancelDownload(package) }
        self.session = nil
        self.featuredApps = []
        self.downloadProgress = [:]
        isLoading = false
        statusMessage = nil
        errorMessage = nil
        defaults.removeObject(forKey: Self.sessionKey)
    }

    // MARK: - Login Flow with EmbeddedSetup oauth_token

    /// Trades oauth_token from EmbeddedSetup for full Play Store session
    func signIn(email: String, oauthToken: String) async {
        isLoading = true
        errorMessage = nil
        statusMessage = "Exchanging token with Google…"

        do {
            // 1. Trade oauth_token for aasToken
            let aas = try await PlayAPI.accountToken(email: email, oauthToken: oauthToken)
            var s = PlaySession(email: email, aasToken: aas)

            // 2. Checkin (register device)
            statusMessage = "Registering device with Google Play…"
            let (gsfID, consistency) = try await PlayAPI.checkin()
            s.gsfID = gsfID
            s.consistencyToken = consistency

            // 3. Acquire bearer token for Play Store
            statusMessage = "Authorizing Google Play session…"
            let bearer = try await PlayAPI.playToken(s)
            s.bearer = bearer

            // 4. Upload device config (Pixel 8)
            statusMessage = "Uploading device profile…"
            let configToken = try await PlayAPI.uploadDeviceConfig(s)
            s.deviceConfigToken = configToken

            saveSession(s)
            statusMessage = nil
            isLoading = false

            // Load initial apps
            await loadBrowseApps()
        } catch {
            isLoading = false
            statusMessage = nil
            errorMessage = error.localizedDescription
            HuskLog.log("store", "Sign-in error: \(error.localizedDescription)")
        }
    }

    // MARK: - Aurora Guest Flow

    /// Obtains an anonymous guest session using Aurora's token dispenser server
    func signInAsGuest(customDispenserURL: String? = nil) async {
        isLoading = true
        errorMessage = nil
        statusMessage = "Connecting to Aurora guest server…"

        do {
            let url = customDispenserURL ?? PlayAPI.defaultDispenserURL
            let s = try await PlayAPI.fetchAuroraGuestSession(dispenserURL: url)
            saveSession(s)
            statusMessage = nil
            isLoading = false
            await loadBrowseApps()
        } catch {
            isLoading = false
            statusMessage = nil
            errorMessage = error.localizedDescription
            HuskLog.log("store", "Aurora guest sign-in error: \(error.localizedDescription)")
        }
    }

    /// Refresh Play Store bearer token if needed
    func refreshSessionIfNeeded() async {
        guard var s = session else { return }
        let expectedSession = s
        if s.isAnonymous {
            // Refresh anonymous guest token from dispenser
            do {
                let fresh = try await PlayAPI.fetchAuroraGuestSession()
                guard session == expectedSession else { return }
                saveSession(fresh)
                await loadBrowseApps()
            } catch {
                guard session == expectedSession else { return }
                errorMessage = error.localizedDescription
                HuskLog.log("store", "Failed to refresh Aurora guest token: \(error.localizedDescription)")
            }
            return
        }
        guard !s.aasToken.isEmpty else { return }
        do {
            let bearer = try await PlayAPI.playToken(s)
            guard session == expectedSession else { return }
            s.bearer = bearer
            saveSession(s)
            await loadBrowseApps()
        } catch {
            guard session == expectedSession else { return }
            HuskLog.log("store", "Failed to refresh Play token: \(error.localizedDescription)")
        }
    }

    // MARK: - Store Browsing & Searching

    func loadBrowseApps() async {
        guard let s = session, !s.bearer.isEmpty else { return }
        if !featuredApps.isEmpty { return }
        isLoading = true
        do {
            let apps = try await PlayAPI.browse(session: s)
            self.featuredApps = apps
            self.isLoading = false
        } catch {
            self.isLoading = false
            HuskLog.log("store", "Failed to browse apps: \(error.localizedDescription)")
        }
    }

    func search(query: String) async {
        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard canSearch, let s = session, !trimmed.isEmpty else { clearSearch(); return }
        let generation = UUID()
        searchGeneration = generation
        isSearching = true
        do {
            try Task.checkCancellation()
            let results = try await searchRequest(trimmed, s)
            try Task.checkCancellation()
            guard searchGeneration == generation, canSearch else { return }
            self.searchResults = results
            self.isSearching = false
        } catch {
            guard searchGeneration == generation else { return }
            self.isSearching = false
            if Task.isCancelled || error is CancellationError || (error as? URLError)?.code == .cancelled { return }
            errorMessage = error.localizedDescription
            HuskLog.log("store", "Failed to search '\(query)': \(error.localizedDescription)")
        }
    }

    func clearSearch() {
        searchGeneration = UUID()
        searchResults = []
        isSearching = false
    }

    func fetchDetails(for packageName: String) async -> PlayAPI.PlayApp? {
        guard let s = session, !s.bearer.isEmpty else { return nil }
        return try? await PlayAPI.details(packageName: packageName, session: s)
    }

    // MARK: - Download & Install Flow

    func downloadAndInstall(app: PlayAPI.PlayApp) {
        guard let s = session else { return }
        let pkg = app.id
        guard downloadProgress[pkg] == nil else { return }

        let downloadID = UUID()
        downloadIDs[pkg] = downloadID
        downloadProgress[pkg] = PlayAPI.DownloadProgress(received: 0, total: nil)
        installingPackages.insert(pkg)
        downloadTitles[pkg] = app.title

        downloadTasks[pkg] = Task {
            do {
                // If versionCode is 0, fetch details first to get current versionCode
                var versionCode = app.versionCode
                if versionCode == 0 {
                    if let fresh = try? await PlayAPI.details(packageName: pkg, session: s), fresh.versionCode > 0 {
                        versionCode = fresh.versionCode
                    } else {
                        versionCode = 1
                    }
                }

                HuskLog.log("store", "Starting download for \(pkg) (vc: \(versionCode))")
                let files = try await PlayAPI.downloadApp(
                    packageName: pkg,
                    versionCode: versionCode,
                    session: s
                ) { [weak self] p in
                    guard let self, self.downloadIDs[pkg] == downloadID else { return }
                    self.downloadProgress[pkg] = p
                }

                try Task.checkCancellation()
                guard self.downloadIDs[pkg] == downloadID else { return }
                HuskLog.log("store", "Downloaded \(files.count) files for \(pkg), handing to IncomingFiles")
                self.downloadProgress.removeValue(forKey: pkg)
                self.installingPackages.remove(pkg)
                self.downloadTasks[pkg] = nil
                self.downloadIDs[pkg] = nil
                self.downloadTitles.removeValue(forKey: pkg)

                // Hand to Husk's IncomingFiles flow, which automatically presents IncomingChooser!
                IncomingFiles.shared.receive(files)
            } catch {
                guard self.downloadIDs[pkg] == downloadID else { return }
                self.downloadProgress.removeValue(forKey: pkg)
                self.installingPackages.remove(pkg)
                self.downloadTasks[pkg] = nil
                self.downloadIDs[pkg] = nil
                self.downloadTitles.removeValue(forKey: pkg)
                if Task.isCancelled || error is CancellationError || (error as? URLError)?.code == .cancelled { return }
                self.errorMessage = "Failed to download \(app.title): \(error.localizedDescription)"
                HuskLog.log("store", "Download error for \(pkg): \(error.localizedDescription)")
            }
        }
    }
}
