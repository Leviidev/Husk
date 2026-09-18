import SwiftUI

struct DiscoverTab: View {
    @ObservedObject private var manager = SourceManager.shared
    @ObservedObject private var host = AndroidHost.shared
    
    @State private var showingSources = false
    @State private var newSourceURL = ""
    
    var body: some View {
        NavigationStack {
            ZStack {
                Theme.backdrop
                
                if manager.isLoading {
                    ProgressView("Fetching Sources...")
                        .foregroundStyle(Theme.textDim)
                } else if manager.sources.isEmpty {
                    VStack(spacing: 12) {
                        Image(systemName: "tray.fill")
                            .font(.system(size: 48))
                            .foregroundStyle(Theme.textDim)
                        Text("No Sources")
                            .font(.headline)
                        Text("Add a source to discover apps.")
                            .foregroundStyle(Theme.textDim)
                        Button("Manage Sources") { showingSources = true }
                            .buttonStyle(.borderedProminent)
                    }
                } else {
                    ScrollView {
                        LazyVStack(spacing: 24) {
                            ForEach(manager.sources) { source in
                                sourceSection(source)
                            }
                        }
                        .padding()
                    }
                }
            }
            .navigationTitle("Discover")
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
    
    private func sourceSection(_ source: AppSource) -> some View {
        VStack(alignment: .leading, spacing: 16) {
            Text(source.name)
                .font(.title2.bold())
                .foregroundStyle(Theme.text)
                .padding(.horizontal, 4)
            
            ForEach(source.apps) { app in
                appRow(app)
                Divider()
            }
        }
        .padding()
        .background(Theme.surfaceHigh)
        .cornerRadius(16)
    }
    
    private func appRow(_ app: SourceApp) -> some View {
        HStack(spacing: 16) {
            AsyncImage(url: URL(string: app.iconURL)) { phase in
                if let image = phase.image {
                    image.resizable()
                         .aspectRatio(contentMode: .fit)
                } else if phase.error != nil {
                    Color.red.opacity(0.3)
                } else {
                    ProgressView()
                }
            }
            .frame(width: 60, height: 60)
            .cornerRadius(12)
            
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
                    // Start the app in guest
                    let intent = "am start -n \(app.bundleIdentifier)/\(app.bundleIdentifier).MainActivity"
                    _ = try? GuestBridge.shared.shell(intent, timeout: 5)
                }
                .font(.subheadline.bold())
                .padding(.horizontal, 16)
                .padding(.vertical, 8)
                .background(Theme.surface)
                .foregroundStyle(Theme.text)
                .cornerRadius(16)
            } else if let progress = progress {
                // Downloading state
                ZStack {
                    Circle()
                        .stroke(Theme.surface, lineWidth: 3)
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
                Section("Active Sources") {
                    ForEach(manager.sourceURLs, id: \.self) { url in
                        Text(url).font(.caption).lineLimit(1)
                    }
                    .onDelete { indices in
                        manager.sourceURLs.remove(atOffsets: indices)
                    }
                }
                
                Section("Add Source") {
                    HStack {
                        TextField("https://...", text: $newSourceURL)
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
            .navigationTitle("Sources")
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button("Done") { showingSources = false }
                }
            }
        }
    }
}
