// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Google Play Store tab in Husk.
/// Lets users sign in with their Google account, browse/search Play Store, view app details,
/// and download/install apps (triggering the IncomingFiles chooser for translation layer or Android).
struct StoreView: View {
    @ObservedObject private var compat = CompatibilityStore.shared
    @ObservedObject private var manager = PlayStoreManager.shared
    @State private var searchText = ""
    @State private var debouncedSearchText = ""
    @State private var showSignIn = false
    @State private var selectedApp: PlayAPI.PlayApp?
    @State private var shelf: StoreShelf = .featured
    @State private var opening: String?

    var body: some View {
        NavigationStack {
            Group {
                if !manager.isSignedIn {
                    signedOutState
                } else {
                    storeContent
                }
            }
            .background(Console.page.ignoresSafeArea())
            .navigationTitle("Store")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItemGroup(placement: .topBarTrailing) {
                    if !manager.downloadProgress.isEmpty {
                        Button { Router.shared.showDownloads = true } label: {
                            Label("\(manager.downloadProgress.count) Downloading", systemImage: "arrow.down.circle")
                                .labelStyle(.titleAndIcon)
                                .font(.subheadline.weight(.semibold))
                        }
                    }
                    if manager.isSignedIn {
                        Menu {
                            if let s = manager.session {
                                Text(s.isAnonymous ? "Guest Account (Aurora)" : s.email)
                                    .font(.caption)
                                if s.isAnonymous {
                                    Text(s.email).font(.caption2).foregroundStyle(.secondary)
                                }
                                Divider()
                            }
                            Button(role: .destructive) {
                                manager.signOut()
                            } label: {
                                Label("Sign Out", systemImage: "rectangle.portrait.and.arrow.right")
                            }
                        } label: {
                            Image(systemName: manager.session?.isAnonymous == true ? "person.badge.shield.checkmark" : "person.crop.circle")
                                .font(.system(size: 18))
                        }
                    } else {
                        Menu {
                            Button {
                                Task { await manager.signInAsGuest() }
                            } label: {
                                Label("Use Guest Account (Aurora)", systemImage: "person.crop.circle.badge.checkmark")
                            }
                            Button {
                                showSignIn = true
                            } label: {
                                Label("Sign In with Google", systemImage: "person.badge.key")
                            }
                        } label: {
                            Text("Sign In")
                                .font(.subheadline.weight(.semibold))
                        }
                    }
                }
            }
            .sheet(isPresented: $showSignIn) {
                GoogleSignInSheet()
            }
            .onReceive(Router.shared.$storePackage) { pkg in
                guard let pkg else { return }
                Router.shared.storePackage = nil
                open(pkg)
            }
            .sheet(item: $selectedApp) { app in
                PlayAppDetailSheet(app: app)
            }
            .onReceive(Router.shared.$tab) { tab in
                if tab != .store { selectedApp = nil }
            }
            .onChange(of: manager.canSearch) { allowed in
                if !allowed { searchText = ""; debouncedSearchText = ""; manager.clearSearch() }
            }
            .onChange(of: searchText) { value in
                if value.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
                    debouncedSearchText = ""
                    manager.clearSearch()
                }
            }
            .task(id: [searchText, manager.canSearch ? "ready" : "signed-out"]) {
                guard manager.canSearch else { return }
                let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
                guard !query.isEmpty else { return }
                do {
                    try await Task.sleep(nanoseconds: 350_000_000)
                    try Task.checkCancellation()
                    debouncedSearchText = query
                    await manager.search(query: query)
                } catch {}
            }
            .alert("Error", isPresented: Binding(
                get: { manager.errorMessage != nil },
                set: { if !$0 { manager.errorMessage = nil } }
            )) {
                Button("OK", role: .cancel) { manager.errorMessage = nil }
            } message: {
                Text(manager.errorMessage ?? "")
            }
        }
    }

    // MARK: - Signed Out State

    private var signedOutState: some View {
        VStack(spacing: 20) {
            Spacer()

            Image(systemName: "cart.fill")
                .font(.system(size: 64))
                .foregroundStyle(Color.accentColor)

            VStack(spacing: 8) {
                Text("Google Play Store")
                    .font(.title2.weight(.bold))
                    .foregroundStyle(Theme.text)

                Text("Browse and download apps using an anonymous Aurora guest account, or sign in with your Google account.")
                    .font(.subheadline)
                    .foregroundStyle(Theme.textDim)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 32)
            }

            if manager.isLoading {
                VStack(spacing: 12) {
                    ProgressView()
                    Text(manager.statusMessage ?? "Signing in…")
                        .font(.footnote)
                        .foregroundStyle(Theme.textDim)
                }
                .padding(.top, 12)
            } else {
                VStack(spacing: 12) {
                    // Aurora Guest Button
                    Button {
                        Task {
                            await manager.signInAsGuest()
                        }
                    } label: {
                        HStack(spacing: 8) {
                            Image(systemName: "person.crop.circle.badge.checkmark")
                            Text("Use Guest Account")
                        }
                        .font(.headline)
                        .frame(maxWidth: 240)
                        .padding(.vertical, 12)
                    }
                    .buttonStyle(.borderedProminent)
                    .buttonBorderShape(.capsule)

                    // Google Account Button
                    Button {
                        showSignIn = true
                    } label: {
                        HStack(spacing: 8) {
                            Image(systemName: "person.badge.key")
                            Text("Sign In with Google")
                        }
                        .font(.subheadline.weight(.medium))
                        .frame(maxWidth: 240)
                        .padding(.vertical, 10)
                    }
                    .buttonStyle(.bordered)
                    .buttonBorderShape(.capsule)
                }
                .padding(.top, 8)
            }

            Spacer()
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    // MARK: - Store Content

    private var storeContent: some View {
        StoreSearchContent(text: $searchText, enabled: manager.canSearch) {
            if manager.isLoading && manager.featuredApps.isEmpty && debouncedSearchText.isEmpty {
                ProgressView("Connecting to Google Play…")
                    .frame(maxWidth: .infinity).padding(.vertical, 24)
            } else if debouncedSearchText.isEmpty { discover } else { searchList }
        }
        .refreshable {
            if debouncedSearchText.isEmpty { await manager.loadBrowseApps() }
            else { await manager.search(query: debouncedSearchText) }
        }
    }

    // MARK: - Discover (the Store's front page)

    private var games: [PlayAPI.PlayApp] { manager.featuredApps.filter { !PlayAPI.featuredAppPackages.contains($0.id) } }
    private var apps: [PlayAPI.PlayApp] { manager.featuredApps.filter { PlayAPI.featuredAppPackages.contains($0.id) } }

    /// What runs best on Husk, from the compatibility list: the most reported among those that work, with an icon.
    private var trending: [CompatibilityStore.Entry] {
        compat.entries.values
            .filter { $0.status == .works && $0.icon != nil && !manager.featuredApps.map(\.id).contains($0.package) }
            .sorted { $0.community.reports > $1.community.reports }
            .prefix(12).map { $0 }
    }

    private var discover: some View {
        VStack(alignment: .leading, spacing: 22) {
            HStack(spacing: 8) {
                ForEach(StoreShelf.allCases) { s in
                    ConsoleChip(title: s.title, selected: shelf == s) { withAnimation(.easeOut(duration: 0.18)) { shelf = s } }
                }
                Spacer(minLength: 0)
            }
            let list = shelf == .apps ? apps : shelf == .games ? games : manager.featuredApps
            if let lead = list.first { banner(lead) }
            if shelf != .apps, !games.isEmpty {
                row(shelf == .games ? "Games" : "Popular Games", games.dropFirst(shelf == .featured ? 1 : 1).map { $0 })
            }
            if shelf != .games, !apps.isEmpty { row("Apps", shelf == .apps ? Array(apps.dropFirst()) : apps) }
            if shelf != .apps, !trending.isEmpty {
                VStack(alignment: .leading, spacing: 12) {
                    Text("Trending Now").font(.system(size: 20, weight: .bold)).foregroundStyle(Console.text)
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(alignment: .top, spacing: 14) {
                            ForEach(trending, id: \.package) { e in
                                Button { open(e.package) } label: {
                                    tile(title: e.name, icon: e.icon.flatMap { URL(string: $0, relativeTo: CompatibilityStore.site) },
                                         status: e.status, busy: opening == e.package)
                                }
                                .buttonStyle(CardButtonStyle())
                            }
                        }
                    }
                }
            }
            if manager.featuredApps.isEmpty {
                Text("No featured apps right now. Search Google Play above.")
                    .font(.subheadline).foregroundStyle(Console.textDim)
            }
        }
    }

    /// The first app of the shelf, big: its own screenshot behind "Discover new worlds."
    private func banner(_ app: PlayAPI.PlayApp) -> some View {
        Button { selectedApp = app } label: {
            ZStack(alignment: .bottomLeading) {
                Console.card
                AsyncImage(url: URL(string: app.feature ?? app.screenshotURLs.first ?? app.iconURL ?? "")) { phase in
                    if let image = phase.image { image.resizable().scaledToFill() } else { Console.card }
                }
                LinearGradient(colors: [.black.opacity(0.75), .clear], startPoint: .leading, endPoint: .trailing)
                VStack(alignment: .leading, spacing: 6) {
                    Text(shelf == .apps ? "Apps for\nevery day." : "Discover\nnew worlds.")
                        .font(.system(size: 28, weight: .bold)).foregroundStyle(.white)
                        .multilineTextAlignment(.leading)
                    Text("\(app.title) and more, running on your iPhone with Husk.")
                        .font(.system(size: 13)).foregroundStyle(.white.opacity(0.85))
                        .lineLimit(2).multilineTextAlignment(.leading)
                }
                .padding(18)
            }
            .frame(height: 180)
            .frame(maxWidth: .infinity)
            .clipShape(RoundedRectangle(cornerRadius: 20, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).stroke(Console.outline, lineWidth: 0.5))
            .contentShape(RoundedRectangle(cornerRadius: 20, style: .continuous))
        }
        .buttonStyle(CardButtonStyle())
    }

    private func row(_ title: String, _ list: [PlayAPI.PlayApp]) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title).font(.system(size: 20, weight: .bold)).foregroundStyle(Console.text)
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(alignment: .top, spacing: 14) {
                    ForEach(list) { app in
                        Button { selectedApp = app } label: {
                            tile(title: app.title, icon: URL(string: app.iconURL ?? ""), status: compat.entry(app.id)?.status, busy: false)
                        }
                        .buttonStyle(CardButtonStyle())
                    }
                }
            }
        }
    }

    private func tile(title: String, icon: URL?, status: CompatibilityStore.Status?, busy: Bool) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            AsyncImage(url: icon) { phase in
                if let image = phase.image { image.resizable().scaledToFill() } else { Console.raised }
            }
            .frame(width: 104, height: 104)
            .clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
            .overlay { if busy { ProgressView().tint(.white) } }
            Text(title).font(.system(size: 13, weight: .semibold)).foregroundStyle(Console.text).lineLimit(1)
            HStack(spacing: 4) {
                Circle().fill(status?.color ?? Console.textDim).frame(width: 6, height: 6)
                Text("Android").font(.system(size: 11)).foregroundStyle(Console.textDim)
            }
        }
        .frame(width: 104, alignment: .leading)
    }

    /// An app from the compatibility list: its Play page, fetched first.
    private func open(_ package: String) {
        guard opening == nil, let s = manager.session else { return }
        opening = package
        Task {
            let app = try? await PlayAPI.details(packageName: package, session: s)
            opening = nil
            if let app { selectedApp = app } else { manager.errorMessage = "Google Play has no page for \(package) here." }
        }
    }

    // MARK: - Search results

    private var searchList: some View {
        LazyVStack(alignment: .leading, spacing: 12) {
            Text("Search Results").font(.headline).foregroundStyle(Theme.textDim)
            if manager.isSearching {
                ProgressView("Searching…").frame(maxWidth: .infinity).padding(.vertical, 12)
            } else if manager.searchResults.isEmpty {
                Text("No apps found for \"\(debouncedSearchText)\".")
                    .font(.subheadline).foregroundStyle(Theme.textDim).padding(.vertical, 8)
            } else {
                ForEach(manager.searchResults) { app in
                    appRow(app)
                        .padding(14)
                        .background(Console.card, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                }
            }
        }
    }

    // MARK: - App Row

    private func appRow(_ app: PlayAPI.PlayApp) -> some View {
        HStack(spacing: 14) {
            Button {
                selectedApp = app
            } label: {
                HStack(spacing: 14) {
                    // App Icon
                    AsyncImage(url: URL(string: app.iconURL ?? "")) { phase in
                        if let image = phase.image {
                            image.resizable().aspectRatio(contentMode: .fit)
                        } else if phase.error != nil {
                            Image(systemName: "app.fill")
                                .font(.title)
                                .foregroundStyle(Color.accentColor.opacity(0.8))
                        } else {
                            Color(uiColor: .tertiarySystemFill)
                        }
                    }
                    .frame(width: 52, height: 52)
                    .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))

                    VStack(alignment: .leading, spacing: 3) {
                        Text(app.title)
                            .font(.body.weight(.semibold))
                            .foregroundStyle(Theme.text)
                            .lineLimit(1)

                        Text(app.developer.isEmpty ? app.id : app.developer)
                            .font(.caption)
                            .foregroundStyle(Theme.textDim)
                            .lineLimit(1)

                        if let status = compat.entry(app.id)?.status { CompatBadge(status: status) }

                        if app.rating > 0 || !app.installs.isEmpty {
                            HStack(spacing: 3) {
                                if app.rating > 0 {
                                    Image(systemName: "star.fill")
                                        .font(.system(size: 10))
                                        .foregroundStyle(.yellow)
                                    Text(String(format: "%.1f", app.rating))
                                }
                                if app.rating > 0 && !app.installs.isEmpty { Text("·") }
                                if !app.installs.isEmpty { Text(app.installs) }
                            }
                            .font(.caption2.weight(.medium))
                            .foregroundStyle(Theme.textDim)
                        }
                    }
                }
            }
            .buttonStyle(.plain)

            Spacer(minLength: 8)

            // Install Button / Progress
            installButton(for: app)
        }
        .padding(.vertical, 4)
    }

    private func installButton(for app: PlayAPI.PlayApp) -> some View {
        let progress = manager.downloadProgress[app.id]
        let isDownloading = manager.installingPackages.contains(app.id)

        return Group {
            if isDownloading || progress != nil {
                Button {
                    manager.cancelDownload(app.id)
                } label: {
                    ZStack {
                        Circle()
                            .stroke(Color(uiColor: .tertiarySystemFill), lineWidth: 3)
                        if let fraction = progress?.fraction {
                            Circle()
                                .trim(from: 0, to: fraction)
                                .stroke(Color.accentColor, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                                .rotationEffect(.degrees(-90))
                        }
                        Image(systemName: "stop.fill").font(.system(size: 10)).foregroundStyle(Color.accentColor)
                    }
                    .frame(width: 32, height: 32)
                }
                .buttonStyle(.borderless)
                .accessibilityLabel("Stop Download")
            } else {
                Button {
                    manager.downloadAndInstall(app: app)
                } label: {
                    Text(app.isFree ? "GET" : app.formattedPrice)
                        .font(.subheadline.weight(.bold))
                        .padding(.horizontal, 14)
                        .padding(.vertical, 6)
                }
                .buttonStyle(.borderedProminent)
                .buttonBorderShape(.capsule)
            }
        }
    }
}

/// The Store's pills.
enum StoreShelf: String, CaseIterable, Identifiable {
    case featured, games, apps
    var id: String { rawValue }
    var title: String {
        switch self {
        case .featured: return "Featured"
        case .games: return "Games"
        case .apps: return "Apps"
        }
    }
}

// MARK: - Search field

/// Keep the field in the same scroll view as both discovery and results. It starts visible,
/// scrolls out with the page, and keeps keyboard focus when a debounced search completes.
struct StoreSearchContent<Content: View>: View {
    @Binding var text: String
    let enabled: Bool
    @ViewBuilder var content: Content
    @FocusState private var focused: Bool

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                if enabled {
                    HStack(spacing: 8) {
                        Image(systemName: "magnifyingglass").foregroundStyle(.secondary)
                        TextField("Search Google Play", text: $text)
                            .textFieldStyle(.plain)
                            .autocapitalization(.none)
                            .disableAutocorrection(true)
                            .submitLabel(.search)
                            .focused($focused)
                            .onSubmit { focused = false }
                            .accessibilityIdentifier("husk.store.search")
                        if !text.isEmpty {
                            Button { text = "" } label: {
                                Image(systemName: "xmark.circle.fill").foregroundStyle(.secondary)
                            }
                            .buttonStyle(.plain)
                            .accessibilityLabel("Clear search")
                        }
                    }
                    .padding(12)
                    .background(Console.raised, in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                }
                content
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 24)
        }
        .onChange(of: enabled) { if !$0 { focused = false } }
    }
}
