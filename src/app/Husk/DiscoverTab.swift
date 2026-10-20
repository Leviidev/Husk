// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Apps from the repositories you add, to install into Android. Pushed from Settings, so it has no navigation stack of its own.
struct DiscoverView: View {
    @ObservedObject private var manager = SourceManager.shared
    @ObservedObject private var host = AndroidHost.shared

    @State private var showingSources = false
    @State private var showingAddSource = false
    @State private var newSourceURL = ""
    @State private var searchText = ""
    @State private var debouncedSearchText = ""

    var body: some View {
        Group {
            if manager.isLoading && manager.sources.isEmpty {
                ProgressView(L10n.tr("Fetching Repositories…"))
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else if manager.sources.isEmpty {
                EmptyState(title: L10n.tr("No Repositories"),
                           message: L10n.tr("Add a source to start discovering apps."),
                           systemImage: "tray",
                           actionTitle: L10n.tr("Add Source"),
                           action: { showingAddSource = true })
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                list
            }
        }
        .background(Theme.backdrop)
        .navigationTitle(L10n.tr("Discover"))
        .navigationBarTitleDisplayMode(.large)
        .searchable(text: $searchText, prompt: L10n.tr("Search \(totalAppCount) apps"))
        .toolbar {
            ToolbarItemGroup(placement: .topBarTrailing) {
                Button { Task { await manager.fetchSources() } } label: {
                    Label(L10n.tr("Refresh"), systemImage: "arrow.clockwise")
                }
                Menu {
                    Button { showingAddSource = true } label: {
                        Label(L10n.tr("Add Source"), systemImage: "plus")
                    }
                    Button { showingSources = true } label: {
                        Label(L10n.tr("Manage Repositories"), systemImage: "list.bullet")
                    }
                } label: { Label(L10n.tr("Repositories"), systemImage: "ellipsis.circle") }
            }
        }
        .sheet(isPresented: $showingSources) { sourcesSheet }
        .sheet(isPresented: $showingAddSource) { addSourceSheet }
        .onAppear {
            if manager.sources.isEmpty && !manager.isLoading {
                Task { await manager.fetchSources() }
            }
        }
        .task(id: searchText) {
            do {
                try await Task.sleep(nanoseconds: 200_000_000)
                debouncedSearchText = searchText
            } catch {}
        }
    }

    private var list: some View {
        List {
            ForEach(manager.sources) { source in
                let filtered = filteredApps(for: source)
                if !filtered.isEmpty {
                    sourceSection(source, apps: filtered)
                }
            }
            if !manager.fetchErrors.isEmpty {
                Section {
                    ForEach(manager.fetchErrors.keys.sorted(), id: \.self) { url in
                        Label {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(url).font(.footnote).lineLimit(1)
                                Text(manager.fetchErrors[url] ?? "").font(.caption).foregroundStyle(.secondary)
                            }
                        } icon: { Image(systemName: "exclamationmark.triangle.fill").foregroundStyle(.orange) }
                    }
                } header: { Text(L10n.tr("Could Not Load")) }
            }
        }
        .listStyle(.insetGrouped)
    }

    private var totalAppCount: Int {
        manager.sources.reduce(0) { $0 + $1.apps.count }
    }

    // MARK: - Helpers

    private func filteredApps(for source: AppSource) -> [SourceApp] {
        if debouncedSearchText.isEmpty { return source.apps }
        let query = debouncedSearchText.lowercased()
        return source.apps.filter {
            $0.name.lowercased().contains(query) ||
            $0.bundleIdentifier.lowercased().contains(query)
        }
    }

    private func sourceSection(_ source: AppSource, apps: [SourceApp]) -> some View {
        let displayApps = debouncedSearchText.isEmpty ? Array(apps.prefix(50)) : apps
        return Section {
            ForEach(displayApps) { app in appRow(app) }
        } header: {
            Text(source.name)
        } footer: {
            if debouncedSearchText.isEmpty && apps.count > 50 {
                Text(L10n.tr("Search to see \(apps.count - 50) more apps."))
            }
        }
    }

    private func appRow(_ app: SourceApp) -> some View {
        HStack(spacing: 14) {
            AsyncImage(url: URL(string: app.iconURL)) { phase in
                if let image = phase.image {
                    image.resizable().aspectRatio(contentMode: .fit)
                } else if phase.error != nil {
                    Image(systemName: "app.dashed").font(.title).foregroundStyle(.secondary)
                } else {
                    ProgressView()
                }
            }
            .frame(width: 48, height: 48)
            .clipShape(RoundedRectangle(cornerRadius: 11, style: .continuous))

            VStack(alignment: .leading, spacing: 3) {
                Text(app.name).font(.body.weight(.medium)).lineLimit(1)
                Text(app.localizedDescription)
                    .font(.caption).foregroundStyle(.secondary).lineLimit(2)
            }

            Spacer(minLength: 8)

            let isInstalled = host.packages.contains { $0.id == app.bundleIdentifier }
            let progress = manager.downloadProgress[app.bundleIdentifier]

            if isInstalled {
                Button(L10n.tr("Open")) {
                    let intent = "am start -n \(app.bundleIdentifier)/\(app.bundleIdentifier).MainActivity"
                    _ = try? GuestBridge.shared.shell(intent, timeout: 5)
                }
                .buttonStyle(.bordered)
                .buttonBorderShape(.capsule)
            } else if let progress = progress {
                ZStack {
                    Circle().stroke(Color(uiColor: .tertiarySystemFill), lineWidth: 3)
                    Circle().trim(from: 0, to: progress)
                        .stroke(Color.accentColor, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                        .rotationEffect(.degrees(-90))
                    Image(systemName: "stop.fill").font(.system(size: 9)).foregroundStyle(.secondary)
                }
                .frame(width: 28, height: 28)
            } else {
                Button(L10n.tr("Get")) { manager.downloadAndInstall(app: app) }
                    .buttonStyle(.borderedProminent)
                    .buttonBorderShape(.capsule)
            }
        }
        .buttonStyle(.borderless)
        .padding(.vertical, 2)
    }

    // MARK: - Add Source Sheet

    private var addSourceSheet: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("https://f-droid.org/repo/index-v1.json", text: $newSourceURL)
                        .keyboardType(.URL).textInputAutocapitalization(.never).autocorrectionDisabled()
                } header: {
                    Text(L10n.tr("Source URL"))
                } footer: {
                    Text(L10n.tr("Paste any F-Droid-compatible repository index URL."))
                }

                Section(L10n.tr("Presets")) {
                    ForEach([
                        ("F-Droid", "https://f-droid.org/repo/index-v1.json"),
                        ("IzzyOnDroid", "https://apt.izzysoft.de/fdroid/repo/index-v1.json"),
                        ("Guardian Project", "https://guardianproject.info/fdroid/repo/index-v1.json"),
                    ], id: \.0) { name, url in
                        Button { newSourceURL = url } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(name).foregroundStyle(.primary)
                                    Text(url).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                                }
                                Spacer()
                                if newSourceURL == url {
                                    Image(systemName: "checkmark").foregroundStyle(Color.accentColor)
                                }
                            }
                        }
                    }
                }
            }
            .navigationTitle(L10n.tr("Add Source"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button(L10n.tr("Cancel")) { newSourceURL = ""; showingAddSource = false }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button(L10n.tr("Add")) {
                        let url = newSourceURL
                        newSourceURL = ""
                        showingAddSource = false
                        guard !url.isEmpty, URL(string: url) != nil else { return }
                        Task { await manager.addSource(urlString: url) }
                    }
                    .disabled(newSourceURL.isEmpty)
                    .huskBold()
                }
            }
        }
    }

    // MARK: - Manage Sources Sheet

    private var sourcesSheet: some View {
        NavigationStack {
            List {
                Section {
                    ForEach(manager.sourceURLs, id: \.self) { url in
                        VStack(alignment: .leading, spacing: 2) {
                            if let source = manager.sources.first(where: { $0.identifier == url }) {
                                Text(source.name).font(.body.weight(.medium))
                                Text(L10n.tr("\(source.apps.count) apps")).font(.caption).foregroundStyle(.secondary)
                            }
                            Text(url).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                        }
                        .padding(.vertical, 2)
                    }
                    .onDelete { manager.sourceURLs.remove(atOffsets: $0) }
                } header: { Text(L10n.tr("Active Repositories")) }
            }
            .navigationTitle(L10n.tr("Repositories"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button(L10n.tr("Done")) { showingSources = false }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button { showingSources = false; showingAddSource = true } label: {
                        Image(systemName: "plus")
                    }
                }
            }
        }
    }
}
