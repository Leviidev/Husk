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

    var body: some View {
        NavigationStack {
            Group {
                if !manager.isSignedIn {
                    signedOutState
                } else if manager.isLoading && manager.featuredApps.isEmpty && !manager.isSearching {
                    ProgressView(L10n.tr("Connecting to Google Play…"))
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    storeContent
                }
            }
            .background(Theme.backdrop)
            .navigationTitle(L10n.tr("Store"))
            .navigationBarTitleDisplayMode(.large)
            .searchable(text: $searchText, prompt: L10n.tr("Search Google Play"))
            .toolbar {
                ToolbarItemGroup(placement: .topBarTrailing) {
                    if !manager.downloadProgress.isEmpty {
                        Button { Router.shared.showDownloads = true } label: {
                            Label(L10n.tr("\(manager.downloadProgress.count) Downloading"), systemImage: "arrow.down.circle")
                                .labelStyle(.titleAndIcon)
                                .font(.subheadline.weight(.semibold))
                        }
                    }
                    if manager.isSignedIn {
                        Menu {
                            if let s = manager.session {
                                Text(s.isAnonymous ? L10n.tr("Guest Account (Aurora)") : s.email)
                                    .font(.caption)
                                if s.isAnonymous {
                                    Text(s.email).font(.caption2).foregroundStyle(.secondary)
                                }
                                Divider()
                            }
                            Button(role: .destructive) {
                                manager.signOut()
                            } label: {
                                Label(L10n.tr("Sign Out"), systemImage: "rectangle.portrait.and.arrow.right")
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
                                Label(L10n.tr("Use Guest Account (Aurora)"), systemImage: "person.crop.circle.badge.checkmark")
                            }
                            Button {
                                showSignIn = true
                            } label: {
                                Label(L10n.tr("Sign In with Google"), systemImage: "person.badge.key")
                            }
                        } label: {
                            Text(L10n.tr("Sign In"))
                                .font(.subheadline.weight(.semibold))
                        }
                    }
                }
            }
            .sheet(isPresented: $showSignIn) {
                GoogleSignInSheet()
            }
            .sheet(item: $selectedApp) { app in
                PlayAppDetailSheet(app: app)
            }
            .task(id: searchText) {
                do {
                    try await Task.sleep(nanoseconds: 350_000_000)
                    debouncedSearchText = searchText
                    if !searchText.isEmpty {
                        await manager.search(query: searchText)
                    }
                } catch {}
            }
            .alert(L10n.tr("Error"), isPresented: Binding(
                get: { manager.errorMessage != nil },
                set: { if !$0 { manager.errorMessage = nil } }
            )) {
                Button(L10n.tr("OK"), role: .cancel) { manager.errorMessage = nil }
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
                Text(L10n.tr("Google Play Store"))
                    .font(.title2.weight(.bold))
                    .foregroundStyle(Theme.text)

                Text(L10n.tr("Browse and download apps using an anonymous Aurora guest account, or sign in with your Google account."))
                    .font(.subheadline)
                    .foregroundStyle(Theme.textDim)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 32)
            }

            if manager.isLoading {
                VStack(spacing: 12) {
                    ProgressView()
                    Text(manager.statusMessage ?? L10n.tr("Signing in…"))
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
                            Text(L10n.tr("Use Guest Account"))
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
                            Text(L10n.tr("Sign In with Google"))
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
        List {
            if !debouncedSearchText.isEmpty {
                Section {
                    if manager.isSearching {
                        HStack {
                            Spacer()
                            ProgressView(L10n.tr("Searching…"))
                            Spacer()
                        }
                        .padding(.vertical, 12)
                    } else if manager.searchResults.isEmpty {
                        Text(L10n.tr("No apps found for \"\(debouncedSearchText)\"."))
                            .font(.subheadline)
                            .foregroundStyle(Theme.textDim)
                            .padding(.vertical, 8)
                    } else {
                        ForEach(manager.searchResults) { app in
                            appRow(app)
                        }
                    }
                } header: {
                    Text(L10n.tr("Search Results"))
                }
            } else {
                if !manager.featuredApps.isEmpty {
                    Section {
                        ForEach(manager.featuredApps) { app in
                            appRow(app)
                        }
                    } header: {
                        Text(L10n.tr("Featured Apps"))
                    }
                } else {
                    Section {
                        Text(L10n.tr("No featured apps available. Try searching above."))
                            .font(.subheadline)
                            .foregroundStyle(Theme.textDim)
                    }
                }
            }
        }
        .listStyle(.insetGrouped)
        .refreshable {
            if debouncedSearchText.isEmpty {
                await manager.loadBrowseApps()
            } else {
                await manager.search(query: debouncedSearchText)
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

                        if app.rating > 0 {
                            HStack(spacing: 3) {
                                Image(systemName: "star.fill")
                                    .font(.system(size: 10))
                                    .foregroundStyle(.yellow)
                                Text(String(format: "%.1f", app.rating))
                                    .font(.caption2.weight(.medium))
                                    .foregroundStyle(Theme.textDim)
                            }
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
                        Circle()
                            .trim(from: 0, to: progress ?? 0.05)
                            .stroke(Color.accentColor, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                            .rotationEffect(.degrees(-90))
                        Image(systemName: "stop.fill").font(.system(size: 10)).foregroundStyle(Color.accentColor)
                    }
                    .frame(width: 32, height: 32)
                }
                .buttonStyle(.borderless)
                .accessibilityLabel(L10n.tr("Stop Download"))
            } else {
                Button {
                    manager.downloadAndInstall(app: app)
                } label: {
                    Text(app.isFree ? L10n.tr("GET") : app.formattedPrice)
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

// MARK: - App Detail Sheet

struct PlayAppDetailSheet: View {
    let app: PlayAPI.PlayApp
    @Environment(\.dismiss) private var dismiss
    @ObservedObject private var compat = CompatibilityStore.shared
    @ObservedObject private var manager = PlayStoreManager.shared

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    // Header: Icon, Title, Developer, Get Button
                    HStack(spacing: 16) {
                        AsyncImage(url: URL(string: app.iconURL ?? "")) { phase in
                            if let image = phase.image {
                                image.resizable().aspectRatio(contentMode: .fit)
                            } else {
                                Color(uiColor: .tertiarySystemFill)
                            }
                        }
                        .frame(width: 76, height: 76)
                        .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))

                        VStack(alignment: .leading, spacing: 4) {
                            Text(app.title)
                                .font(.title3.weight(.bold))
                                .foregroundStyle(Theme.text)
                                .lineLimit(2)

                            Text(app.developer)
                                .font(.subheadline)
                                .foregroundStyle(Theme.textDim)
                                .lineLimit(1)

                            if let status = compat.entry(app.id)?.status { CompatBadge(status: status) }

                            Spacer(minLength: 4)

                            installAction
                        }
                    }

                    Divider()

                    // Metrics row
                    HStack {
                        metricItem(title: L10n.tr("Rating"), value: app.rating > 0 ? String(format: "%.1f ★", app.rating) : L10n.tr("N/A"))
                        Spacer()
                        metricItem(title: L10n.tr("Downloads"), value: app.downloads.isEmpty ? "—" : app.downloads)
                        Spacer()
                        metricItem(title: L10n.tr("Version"), value: app.versionString.isEmpty ? L10n.tr("Current") : app.versionString)
                        if app.sizeBytes > 0 {
                            Spacer()
                            metricItem(title: L10n.tr("Size"), value: ByteCountFormatter.string(fromByteCount: app.sizeBytes, countStyle: .file))
                        }
                    }
                    .padding(.horizontal, 8)

                    // Screenshots carousel
                    if !app.screenshotURLs.isEmpty {
                        Divider()
                        VStack(alignment: .leading, spacing: 10) {
                            Text(L10n.tr("Preview"))
                                .font(.headline)
                                .foregroundStyle(Theme.text)

                            ScrollView(.horizontal, showsIndicators: false) {
                                HStack(spacing: 12) {
                                    ForEach(app.screenshotURLs, id: \.self) { url in
                                        AsyncImage(url: URL(string: url)) { phase in
                                            if let image = phase.image {
                                                image.resizable().aspectRatio(contentMode: .fit)
                                            } else {
                                                Color(uiColor: .tertiarySystemFill)
                                            }
                                        }
                                        .frame(height: 220)
                                        .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
                                    }
                                }
                            }
                        }
                    }

                    // Description
                    if !app.summary.isEmpty {
                        Divider()
                        VStack(alignment: .leading, spacing: 8) {
                            Text(L10n.tr("About this app"))
                                .font(.headline)
                                .foregroundStyle(Theme.text)

                            Text(app.summary)
                                .font(.body)
                                .foregroundStyle(Theme.textDim)
                        }
                    }
                }
                .padding(20)
            }
            .background(Theme.backdrop)
            .navigationTitle(app.title)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(L10n.tr("Done")) { dismiss() }
                }
            }
        }
    }

    private var installAction: some View {
        let progress = manager.downloadProgress[app.id]
        let isDownloading = manager.installingPackages.contains(app.id)

        return Group {
            if isDownloading || progress != nil {
                HStack(spacing: 8) {
                    ProgressView()
                    Text(progress.map { L10n.tr("Downloading… \(Int($0 * 100))%") } ?? L10n.tr("Downloading…"))
                        .font(.caption.weight(.medium))
                        .foregroundStyle(Theme.textDim)
                    Button(L10n.tr("Cancel"), role: .destructive) { manager.cancelDownload(app.id) }
                        .font(.caption.weight(.semibold))
                        .buttonStyle(.borderless)
                }
            } else {
                Button {
                    manager.downloadAndInstall(app: app)
                    dismiss()
                } label: {
                    Text(app.isFree ? L10n.tr("GET") : app.formattedPrice)
                        .font(.headline)
                        .padding(.horizontal, 24)
                        .padding(.vertical, 6)
                }
                .buttonStyle(.borderedProminent)
                .buttonBorderShape(.capsule)
            }
        }
    }

    private func metricItem(title: String, value: String) -> some View {
        VStack(spacing: 4) {
            Text(value)
                .font(.subheadline.weight(.semibold))
                .foregroundStyle(Theme.text)
            Text(title)
                .font(.caption2)
                .foregroundStyle(Theme.textDim)
        }
    }
}
