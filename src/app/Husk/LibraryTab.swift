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
    @ObservedObject private var jit = JITCoordinator.shared

    let onOpenGuest: () -> Void
    let onStartAndroid: () -> Void
    let started: Bool

    @State private var importing = false
    @State private var query = ""
    @State private var filter = "All"

    private let columns = [GridItem(.adaptive(minimum: 104), spacing: 12)]

    var body: some View {
        NavigationStack(path: $router.library) {
            ScrollView {
                VStack(spacing: 16) {
                    if !host.isReady { machineStrip }
                    if let busy = host.busy { busyStrip(busy) }
                    if jit.busy, !jit.showSetup { busyStrip(jit.status ?? "Turning on JIT…") }
                    if !categories.isEmpty { filterPicker }

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
                        EmptyState(title: "No Results",
                                   message: "Nothing installed is called “\(query)”.",
                                   systemImage: "magnifyingglass")
                    } else if host.packages.isEmpty && host.isReady {
                        EmptyState(title: "No Apps Yet",
                                   message: localizedKey("Install an APK and it appears here. Split sets "
                                          + "work too — pick every piece at once."),
                                   systemImage: "square.grid.2x2",
                                   actionTitle: "Install APK(s)",
                                   action: { importing = true })
                    }
                }
                .padding(.horizontal, 16)
                .padding(.top, 8)
                .padding(.bottom, 24)
            }
            .background(Theme.backdrop)
            .navigationTitle("Library")
            .searchable(text: $query, prompt: "Search apps")
            .autocorrectionDisabled()
            .textInputAutocapitalization(.never)
            .toolbar {
                ToolbarItemGroup(placement: .topBarTrailing) {
                    // Android itself, from the library, whenever it is up. It used to be
                    // reachable only while it was starting, or by opening an app -- so once
                    // it was ready there was no way to simply look at it.
                    if started {
                        Button(action: onOpenGuest) {
                            Label("Show Android", systemImage: "rectangle.inset.filled")
                        }
                    }
                    Button { importing = true } label: { Label("Install APK", systemImage: "plus") }
                }
            }
            .navigationDestination(for: AndroidHost.Package.self) { app in
                AppDetailView(app: app, onOpenGuest: onOpenGuest)
            }
            .huskFilePicker(isPresented: $importing) { urls in
                HuskLog.log("ui", "importing \(urls.count) file(s): "
                          + urls.map(\.lastPathComponent).joined(separator: ", "))
                host.install(urls)
            }
        }
    }

    // MARK: status

    /// One line about the machine, only while it cannot open anything.
    private var machineStrip: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle().fill(Color.accentColor.opacity(0.16)).frame(width: 34, height: 34)
                if started {
                    ProgressView()
                } else {
                    Image(systemName: "power")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(Color.accentColor)
                }
            }
            VStack(alignment: .leading, spacing: 4) {
                Text(localizedKey(started ? "Starting Android" : "Android Is Not Running"))
                    .font(.subheadline.weight(.semibold))
                if started, runner.bootProgress > 0 {
                    ProgressView(value: Double(runner.bootProgress), total: 100)
                } else {
                    Text(localizedKey(started ? host.status
                                 : JITBootstrap.isDebuggerAttached
                                   ? "Your apps are here; start it to open them."
                                   : "Needs JIT. StikJIT is built in — the recommended way."))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(2)
                }
            }
            Spacer(minLength: 6)
            Button(localizedKey(started ? "Show"
                                        : JITBootstrap.isDebuggerAttached
                                          ? "Start"
                                          : "Enable JIT")) {
                if started { onOpenGuest() } else { onStartAndroid() }
            }
            .buttonStyle(.borderedProminent)
            .buttonBorderShape(.capsule)
            .controlSize(.small)
        }
        .padding(14)
        .huskCard()
    }

    private func busyStrip(_ text: String) -> some View {
        HStack(spacing: 12) {
            ProgressView()
            Text(localizedKey(text)).font(.subheadline).lineLimit(2)
            Spacer(minLength: 0)
        }
        .padding(14)
        .huskCard()
    }

    private var filterPicker: some View {
        Picker("Show", selection: $filter) {
            Text("All").tag("All")
            ForEach(categories, id: \.self) { c in Text(plural(c)).tag(c) }
        }
        .pickerStyle(.segmented)
    }

    // MARK: what to show

    /// The categories Android actually reported. When it reported none — which
    /// for sideloaded APKs is the usual answer — there are no chips at all
    /// rather than a row of filters that all show the same thing.
    private var categories: [String] {
        let set = Set(host.packages.compactMap(\.category))
        return ["Game", "App", "Tool"].filter { set.contains($0) }
    }

    private func plural(_ c: String) -> LocalizedStringKey {
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
            AppIcon(path: app.iconPath, size: 50)
                .opacity(dimmed ? 0.5 : 1)
            VStack(alignment: .leading, spacing: 2) {
                Text(app.label)
                    .font(.footnote.weight(.semibold))
                    .foregroundStyle(.primary)
                    .lineLimit(1)
                Text(localizedKey(app.category ?? app.bitness ?? " "))
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .huskCard()
    }
}
