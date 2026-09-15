// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Where the apps are.
///
/// A launcher, not a view onto the bridge. The catalogue is written to disk the
/// first time the guest reports its apps, so the grid is on screen the instant
/// Husk opens — a minute before Android can answer for itself. Everything you
/// can do without the guest (look, read, decide) works straight away; the one
/// thing that needs it, opening an app, waits, and says so.
struct LibraryTab: View {
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var router = Router.shared

    let onOpenGuest: () -> Void
    let onStartAndroid: () -> Void
    let started: Bool

    @State private var importing = false
    @State private var searching = false
    @State private var query = ""
    @State private var filter = "All"
    @FocusState private var searchFocused: Bool

    private let columns = [GridItem(.adaptive(minimum: 100), spacing: 12)]

    var body: some View {
        NavigationStack(path: $router.library) {
            ZStack {
                Theme.backdrop
                content
            }
            .navigationTitle("Library")
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) { HuskMark(size: 30) }
                ToolbarItem(placement: .navigationBarTrailing) {
                    HStack(spacing: 10) {
                        CircleButton(systemImage: "magnifyingglass", active: searching) {
                            withAnimation(.snappy(duration: 0.22)) { searching.toggle() }
                            if searching { searchFocused = true } else { query = "" }
                        }
                        CircleButton(systemImage: "plus") { importing = true }
                    }
                }
            }
            .navigationDestination(for: AndroidHost.Package.self) { app in
                AppDetailView(app: app, onOpenGuest: onOpenGuest)
            }
            .fileImporter(isPresented: $importing, allowedContentTypes: [.item],
                          allowsMultipleSelection: true) { result in
                if case .success(let urls) = result, !urls.isEmpty {
                    HuskLog.log("ui", "importing \(urls.count) file(s): "
                              + urls.map(\.lastPathComponent).joined(separator: ", "))
                    host.install(urls)
                }
            }
        }
    }

    // MARK: content

    @ViewBuilder private var content: some View {
        ScrollView {
            VStack(spacing: 16) {
                if searching { searchField }
                if !host.isReady { machineStrip }
                if let busy = host.busy { busyStrip(busy) }
                if !categories.isEmpty && !searching { chips }

                if !shown.isEmpty {
                    LazyVGrid(columns: columns, spacing: 12) {
                        ForEach(shown) { app in
                            NavigationLink(value: app) {
                                AppCard(app: app, dimmed: !host.isReady)
                            }
                            .buttonStyle(CardButtonStyle())
                            .contextMenu {
                                Button {
                                    host.launch(app.name) { onOpenGuest() }
                                } label: { Label("Launch", systemImage: "play.fill") }
                                .disabled(!host.isReady || host.busy != nil)
                                Button {
                                    router.library.append(app)
                                } label: { Label("Details", systemImage: "info.circle") }
                            }
                        }
                    }
                } else if !query.isEmpty {
                    EmptyState(title: "No matches",
                               message: "Nothing installed is called “\(query)”.",
                               systemImage: "magnifyingglass")
                } else if host.packages.isEmpty && host.isReady {
                    EmptyState(title: "No apps yet",
                               message: "Install an APK and it appears here. Split sets "
                                      + "work too — pick every piece at once.",
                               systemImage: "square.grid.2x2",
                               actionTitle: "Install APK(s)",
                               action: { importing = true })
                }
            }
            .padding(.horizontal, 18)
            .padding(.top, 6)
            .padding(.bottom, 28)
        }
    }

    private var searchField: some View {
        HStack(spacing: 10) {
            Image(systemName: "magnifyingglass")
                .font(.system(size: 14, weight: .medium))
                .foregroundStyle(Theme.textDim)
            TextField("Search apps", text: $query)
                .focused($searchFocused)
                .foregroundStyle(Theme.text)
                .autocorrectionDisabled()
                .textInputAutocapitalization(.never)
            if !query.isEmpty {
                Button { query = "" } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundStyle(Theme.textDim)
                }
                .buttonStyle(.plain)
            }
        }
        .padding(.horizontal, 14).padding(.vertical, 11)
        .huskCard(RoundedRectangle(cornerRadius: Theme.rowCorner, style: .continuous),
                  high: true)
        .transition(.opacity.combined(with: .move(edge: .top)))
    }

    /// One line about the machine, only while it cannot open anything.
    private var machineStrip: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle().fill(Theme.accentSoft).frame(width: 32, height: 32)
                if started {
                    ProgressView().scaleEffect(0.6).tint(Theme.accent)
                } else {
                    Image(systemName: "power")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(Theme.accent)
                }
            }
            VStack(alignment: .leading, spacing: 3) {
                Text(started ? "Starting Android" : "Android is not running")
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(Theme.text)
                if started, runner.bootProgress > 0 {
                    ProgressView(value: Double(runner.bootProgress), total: 100)
                        .progressViewStyle(.linear).tint(Theme.accent)
                        .frame(height: 3)
                } else {
                    Text(started ? host.status
                                 : JITBootstrap.isDebuggerAttached
                                   ? "Your apps are here; start it to open them."
                                   : "Husk needs JIT, which only a debugger can grant.")
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.textDim)
                        .lineLimit(1)
                }
            }
            Spacer(minLength: 6)
            Button(started ? "Show" : JITBootstrap.isDebuggerAttached ? "Start" : "JIT") {
                if started { onOpenGuest() } else { onStartAndroid() }
            }
            .font(.system(size: 13, weight: .semibold))
            .foregroundStyle(.white)
            .padding(.horizontal, 14).padding(.vertical, 8)
            .background(Theme.accent, in: Capsule())
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 13).padding(.vertical, 11)
        .huskCard(RoundedRectangle(cornerRadius: Theme.rowCorner, style: .continuous))
    }

    private func busyStrip(_ text: String) -> some View {
        HStack(spacing: 11) {
            ProgressView().tint(Theme.accent)
            Text(text).font(.system(size: 13)).foregroundStyle(Theme.text).lineLimit(2)
            Spacer(minLength: 0)
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
        .huskCard(RoundedRectangle(cornerRadius: Theme.rowCorner, style: .continuous),
                  high: true)
    }

    private var chips: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                Chip(title: "All", selected: filter == "All") { filter = "All" }
                ForEach(categories, id: \.self) { c in
                    Chip(title: plural(c), selected: filter == c) { filter = c }
                }
            }
            .padding(.horizontal, 1)
        }
    }

    // MARK: what to show

    /// The categories Android actually reported. When it reported none — which
    /// for sideloaded APKs is the usual answer — there are no chips at all
    /// rather than a row of filters that all show the same thing.
    private var categories: [String] {
        let set = Set(host.packages.compactMap(\.category))
        return ["Game", "App", "Tool"].filter { set.contains($0) }
    }

    private func plural(_ c: String) -> String {
        c == "Game" ? "Games" : c == "App" ? "Apps" : "Tools"
    }

    private var shown: [AndroidHost.Package] {
        var list = host.packages
        if filter != "All" { list = list.filter { $0.category == filter } }
        let q = query.trimmingCharacters(in: .whitespaces)
        guard !q.isEmpty else { return list }
        return list.filter {
            $0.label.localizedCaseInsensitiveContains(q)
                || $0.name.localizedCaseInsensitiveContains(q)
        }
    }
}

/// One app, as a card: its icon, its name, and what kind of thing it is.
struct AppCard: View {
    let app: AndroidHost.Package
    var dimmed = false

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            AppIcon(path: app.iconPath, size: 46)
                .opacity(dimmed ? 0.5 : 1)
            VStack(alignment: .leading, spacing: 2) {
                Text(app.label)
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(Theme.text)
                    .lineLimit(1)
                Text(app.category ?? app.bitness ?? " ")
                    .font(.system(size: 11))
                    .foregroundStyle(Theme.textDim)
                    .lineLimit(1)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .huskCard()
    }
}
