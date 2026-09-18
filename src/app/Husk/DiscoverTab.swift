import SwiftUI

struct DiscoverTab: View {
    @ObservedObject private var manager = SourceManager.shared
    @ObservedObject private var host = AndroidHost.shared
    
    @State private var showingSources = false
    @State private var newSourceURL = ""
    @State private var searchText = ""
    
    var body: some View {
        NavigationStack {
            ZStack {
                Theme.backdrop
                
                if manager.isLoading {
                    ProgressView("Fetching Repositories...")
                        .foregroundStyle(Theme.textDim)
                } else if manager.sources.isEmpty {
                    VStack(spacing: 12) {
                        Image(systemName: "tray.fill")
                            .font(.system(size: 48))
                            .foregroundStyle(Theme.textDim)
                        Text("No Sources")
                            .font(.headline)
                        Text("Add a repository to discover apps.")
                            .foregroundStyle(Theme.textDim)
                        Button("Manage Sources") { showingSources = true }
                            .buttonStyle(.borderedProminent)
                    }
                } else {
                    ScrollView {
                        LazyVStack(spacing: 24) {
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
            .searchable(text: $searchText, prompt: "Search apps or packages")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button { showingSources = true } label: {
                        Image(systemName: "list.bullet.rectangle")
                    }
                }
            }
            .sheet(isPresented: $showingSources) {
                sourcesSheet
            }
            .onAppear {
                if manager.sources.isEmpty && !manager.isLoading {
                    Task { await manager.fetchSources() }
                }
            }
        }
    }
    
    private func filteredApps(for source: AppSource) -> [SourceApp] {
        if searchText.isEmpty { return source.apps }
        return source.apps.filter { app in
            app.name.localizedCaseInsensitiveContains(searchText) ||
            app.bundleIdentifier.localizedCaseInsensitiveContains(searchText)
        }
    }
    
    private func sourceSection(_ source: AppSource, apps: [SourceApp]) -> some View {
        VStack(alignment: .leading, spacing: 16) {
            Text(source.name)
                .font(.title2.bold())
                .foregroundStyle(Theme.text)
                .padding(.horizontal, 4)
            
            // For a massive repo like F-Droid, we cap what's visible until searched
            // or just rely on LazyVStack.
            let displayApps = searchText.isEmpty ? Array(apps.prefix(50)) : apps
            
            ForEach(displayApps) { app in
                appRow(app)
                Divider()
            }
            
            if searchText.isEmpty && apps.count > 50 {
                Text("Search to see \(apps.count - 50) more apps...")
                    .font(.footnote)
                    .foregroundStyle(Theme.textDim)
                    .frame(maxWidth: .infinity, alignment: .center)
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
                    image.resizable()
                         .aspectRatio(contentMode: .fit)
                } else if phase.error != nil {
                    Image(systemName: "app.dashed")
                        .font(.title)
                        .foregroundStyle(Theme.textDim)
                } else {
                    ProgressView()
                }
            }
            .frame(width: 50, height: 50)
            .cornerRadius(10)
            
            VStack(alignment: .leading, spacing: 4) {
                Text(app.name).font(.headline).foregroundStyle(Theme.text)
                Text(app.localizedDescription)
                    .font(.caption)
                    .foregroundStyle(Theme.textDim)
                    .lineLimit(2)
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
                .padding(.horizontal, 16)
                .padding(.vertical, 8)
                .background(Theme.surfaceHigh)
                .foregroundStyle(Theme.text)
                .cornerRadius(16)
            } else if let progress = progress {
                ZStack {
                    Circle()
                        .stroke(Theme.surfaceHigh, lineWidth: 3)
                        .frame(width: 28, height: 28)
                    Circle()
                        .trim(from: 0, to: progress)
                        .stroke(Color.blue, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                        .frame(width: 28, height: 28)
                        .rotationEffect(.degrees(-90))
                    Image(systemName: "stop.fill")
                        .font(.system(size: 10))
                        .foregroundStyle(Theme.textDim)
                }
            } else {
                Button("GET") {
                    manager.downloadAndInstall(app: app)
                }
                .font(.subheadline.bold())
                .padding(.horizontal, 16)
                .padding(.vertical, 8)
                .background(Color.blue.opacity(0.2))
                .foregroundStyle(.blue)
                .cornerRadius(16)
            }
        }
    }
    
    private var sourcesSheet: some View {
        NavigationStack {
            List {
                Section("Active Repositories") {
                    ForEach(manager.sourceURLs, id: \.self) { url in
                        Text(url).font(.caption).lineLimit(1)
                    }
                    .onDelete { indices in
                        manager.sourceURLs.remove(atOffsets: indices)
                    }
                }
                
                Section("Add Repository") {
                    HStack {
                        TextField("https://f-droid.org/repo/index-v1.json", text: $newSourceURL)
                            .keyboardType(.URL)
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled()
                        Button("Add") {
                            guard URL(string: newSourceURL) != nil else { return }
                            manager.sourceURLs.append(newSourceURL)
                            newSourceURL = ""
                        }
                        .disabled(newSourceURL.isEmpty)
                    }
                }
            }
            .navigationTitle("Repositories")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { showingSources = false }
                }
            }
        }
    }
}
