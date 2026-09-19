import SwiftUI

struct DiscoverTab: View {
    @ObservedObject private var manager = SourceManager.shared
    @ObservedObject private var host = AndroidHost.shared

    @State private var showingSources = false
    @State private var showingAddSource = false
    @State private var newSourceURL = ""
    @State private var searchText = ""

    var body: some View {
        NavigationStack {
            ZStack {
                Theme.backdrop

                if manager.isLoading && manager.sources.isEmpty {
                    ProgressView("Fetching Repositories...")
                        .foregroundStyle(Theme.textDim)
                } else if manager.sources.isEmpty {
                    VStack(spacing: 16) {
                        Image(systemName: "tray.fill")
                            .font(.system(size: 48))
                            .foregroundStyle(Theme.textDim)
                        Text("No Repositories")
                            .font(.headline).foregroundStyle(Theme.text)
                        Text("Add a source to start discovering apps.")
                            .foregroundStyle(Theme.textDim).multilineTextAlignment(.center)
                        Button {
                            showingAddSource = true
                        } label: {
                            Label("Add Source", systemImage: "plus.circle.fill")
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    .padding(32)
                } else {
                    ScrollView {
                        LazyVStack(spacing: 24) {
                            sourcesHeader
                            ForEach(manager.sources) { source in
                                let filtered = filteredApps(for: source)
                                if !filtered.isEmpty {
                                    sourceSection(source, apps: filtered)
                                }
                            }
                        }
                        .padding()
                    }
                }
            }
            .navigationTitle("Discover")
            .searchable(text: $searchText, prompt: "Search \(totalAppCount) apps...")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    HStack(spacing: 14) {
                        Button { Task { await manager.fetchSources() } } label: {
                            Image(systemName: "arrow.clockwise")
                        }
                        Button { showingAddSource = true } label: {
                            Image(systemName: "plus")
                        }
                    }
                }
            }
            .sheet(isPresented: $showingSources) { sourcesSheet }
            .sheet(isPresented: $showingAddSource) { addSourceSheet }
            .onAppear {
                if manager.sources.isEmpty && !manager.isLoading {
                    Task { await manager.fetchSources() }
                }
            }
        }
    }

    private var totalAppCount: Int {
        manager.sources.reduce(0) { $0 + $1.apps.count }
    }

    // MARK: - Source Header Chips

    private var sourcesHeader: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 12) {
                ForEach(manager.sourceURLs, id: \.self) { url in
                    let isLoading = manager.loadingSources.contains(url)
                    let hasError = manager.fetchErrors[url] != nil
                    let source = manager.sources.first(where: { $0.identifier == url })
                    Button { showingSources = true } label: {
                        HStack(spacing: 6) {
                            VStack(alignment: .leading, spacing: 3) {
                                Text(source?.name ?? (isLoading ? "Loading..." : "Failed"))
                                    .font(.subheadline.bold())
                                    .foregroundStyle(hasError ? .red : Theme.text)
                                Text(isLoading ? "Fetching..." : "\(source?.apps.count ?? 0) apps")
                                    .font(.caption2)
                                    .foregroundStyle(Theme.textDim)
                            }
                            if isLoading { ProgressView().scaleEffect(0.7) }
                        }
                        .padding(.horizontal, 14).padding(.vertical, 10)
                        .background(Theme.surfaceHigh)
                        .cornerRadius(12)
                    }
                    .buttonStyle(.plain)
                }
                Button { showingAddSource = true } label: {
                    Label("Add Source", systemImage: "plus")
                        .font(.subheadline.bold())
                        .foregroundStyle(.blue)
                        .padding(.horizontal, 14).padding(.vertical, 10)
                        .background(Color.blue.opacity(0.12))
                        .cornerRadius(12)
                }
                .buttonStyle(.plain)
            }
        }
    }

    // MARK: - Helpers

    private func filteredApps(for source: AppSource) -> [SourceApp] {
        if searchText.isEmpty { return source.apps }
        return source.apps.filter {
            $0.name.localizedCaseInsensitiveContains(searchText) ||
            $0.bundleIdentifier.localizedCaseInsensitiveContains(searchText)
        }
    }

    private func sourceSection(_ source: AppSource, apps: [SourceApp]) -> some View {
        VStack(alignment: .leading, spacing: 16) {
            Text(source.name)
                .font(.title2.bold()).foregroundStyle(Theme.text).padding(.horizontal, 4)

            let displayApps = searchText.isEmpty ? Array(apps.prefix(50)) : apps
            ForEach(displayApps) { app in
                appRow(app)
                Divider()
            }

            if searchText.isEmpty && apps.count > 50 {
                Text("Search to see \(apps.count - 50) more apps...")
                    .font(.footnote).foregroundStyle(Theme.textDim)
                    .frame(maxWidth: .infinity, alignment: .center).padding(.bottom, 4)
            }
        }
        .padding()
        .background(Theme.surface)
        .cornerRadius(16)
    }

    private func appRow(_ app: SourceApp) -> some View {
        HStack(spacing: 16) {
            AsyncImage(url: URL(string: app.iconURL)) { phase in
                if let image = phase.image {
                    image.resizable().aspectRatio(contentMode: .fit)
                } else if phase.error != nil {
                    Image(systemName: "app.dashed").font(.title).foregroundStyle(Theme.textDim)
                } else {
                    ProgressView()
                }
            }
            .frame(width: 50, height: 50)
            .cornerRadius(10)

            VStack(alignment: .leading, spacing: 4) {
                Text(app.name).font(.headline).foregroundStyle(Theme.text)
                Text(app.localizedDescription)
                    .font(.caption).foregroundStyle(Theme.textDim).lineLimit(2)
            }

            Spacer()

            let isInstalled = host.packages.contains { $0.id == app.bundleIdentifier }
            let progress = manager.downloadProgress[app.bundleIdentifier]

            if isInstalled {
                Button("OPEN") {
                    let intent = "am start -n \(app.bundleIdentifier)/\(app.bundleIdentifier).MainActivity"
                    _ = try? GuestBridge.shared.shell(intent, timeout: 5)
                }
                .font(.subheadline.bold())
                .padding(.horizontal, 16).padding(.vertical, 8)
                .background(Theme.surfaceHigh).foregroundStyle(Theme.text)
                .cornerRadius(16)
            } else if let progress = progress {
                ZStack {
                    Circle().stroke(Theme.surfaceHigh, lineWidth: 3).frame(width: 28, height: 28)
                    Circle().trim(from: 0, to: progress)
                        .stroke(Color.blue, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                        .frame(width: 28, height: 28).rotationEffect(.degrees(-90))
                    Image(systemName: "stop.fill").font(.system(size: 10)).foregroundStyle(Theme.textDim)
                }
            } else {
                Button("GET") { manager.downloadAndInstall(app: app) }
                    .font(.subheadline.bold())
                    .padding(.horizontal, 16).padding(.vertical, 8)
                    .background(Color.blue.opacity(0.2)).foregroundStyle(.blue)
                    .cornerRadius(16)
            }
        }
    }

    // MARK: - Add Source Sheet

    private var addSourceSheet: some View {
        NavigationStack {
            VStack(spacing: 24) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Source URL")
                        .font(.headline).foregroundStyle(Theme.text)
                    TextField("https://f-droid.org/repo/index-v1.json", text: $newSourceURL)
                        .keyboardType(.URL).textInputAutocapitalization(.never).autocorrectionDisabled()
                        .padding().background(Theme.surfaceHigh).cornerRadius(12)
                    Text("Paste any F-Droid-compatible repository index URL.")
                        .font(.caption).foregroundStyle(Theme.textDim)
                }

                VStack(alignment: .leading, spacing: 8) {
                    Text("Presets")
                        .font(.headline).foregroundStyle(Theme.text)
                    ForEach([
                        ("F-Droid", "https://f-droid.org/repo/index-v1.json"),
                        ("IzzyOnDroid", "https://apt.izzysoft.de/fdroid/repo/index-v1.json"),
                        ("Guardian Project", "https://guardianproject.info/fdroid/repo/index-v1.json"),
                    ], id: \.0) { name, url in
                        Button { newSourceURL = url } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(name).font(.subheadline.bold()).foregroundStyle(Theme.text)
                                    Text(url).font(.caption2).foregroundStyle(Theme.textDim).lineLimit(1)
                                }
                                Spacer()
                                if newSourceURL == url {
                                    Image(systemName: "checkmark").foregroundStyle(.blue)
                                }
                            }
                            .padding().background(Theme.surfaceHigh).cornerRadius(12)
                        }
                        .buttonStyle(.plain)
                    }
                }

                Spacer()
            }
            .padding()
            .background(Theme.backdrop.ignoresSafeArea())
            .navigationTitle("Add Source")
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Cancel") { newSourceURL = ""; showingAddSource = false }
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Add") {
                        let url = newSourceURL
                        newSourceURL = ""
                        showingAddSource = false
                        guard !url.isEmpty, URL(string: url) != nil else { return }
                        Task { await manager.addSource(urlString: url) }
                    }
                    .disabled(newSourceURL.isEmpty)
                    .bold()
                }
            }
        }
    }

    // MARK: - Manage Sources Sheet

    private var sourcesSheet: some View {
        NavigationStack {
            List {
                Section("Active Repositories") {
                    ForEach(manager.sourceURLs, id: \.self) { url in
                        VStack(alignment: .leading, spacing: 2) {
                            if let source = manager.sources.first(where: { $0.identifier == url }) {
                                Text(source.name).font(.subheadline.bold()).foregroundStyle(Theme.text)
                                Text("\(source.apps.count) apps").font(.caption).foregroundStyle(Theme.textDim)
                            }
                            Text(url).font(.caption2).foregroundStyle(Theme.textDim).lineLimit(1)
                        }
                        .padding(.vertical, 4)
                    }
                    .onDelete { manager.sourceURLs.remove(atOffsets: $0) }
                }
            }
            .navigationTitle("Repositories")
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button("Done") { showingSources = false }
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
