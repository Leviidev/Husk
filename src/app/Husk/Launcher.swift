// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

// MARK: - one library

/// Anything Husk can open: a game run on the translation layer, an app installed in Android, or Android itself.
///
/// The person never has to know which way something runs -- Husk does -- so the launcher sorts by what a thing is (a game,
/// an app), not by how it is run. The only trace of the second is a small badge on what opens inside Android, because that
/// needs Android started first.
enum LibraryItem: Identifiable, Hashable {
    case game(TLApp)
    case app(AndroidHost.Package)
    case android

    var id: String {
        switch self {
        case .game(let g): return "tl:" + g.id
        case .app(let p): return "android:" + p.name
        case .android: return "android-system"
        }
    }

    static func == (a: LibraryItem, b: LibraryItem) -> Bool { a.id == b.id }
    func hash(into h: inout Hasher) { h.combine(id) }

    var title: String {
        switch self {
        case .game(let g): return g.label
        case .app(let p): return p.label
        case .android: return "Android"
        }
    }

    var iconPath: String? {
        switch self {
        case .game(let g): return g.iconPath
        case .app(let p): return p.iconPath
        case .android: return nil
        }
    }

    /// Games on the translation layer (a Flutter app is an app unless it calls itself a game), and what Android itself files as a game.
    var isGame: Bool {
        switch self {
        case .game(let g): return g.report?.nativeEngine != .flutter || g.isGameCategory
        case .app(let p): return p.category == "Game"
        case .android: return false
        }
    }

    var packageName: String? {
        switch self {
        case .game(let g): return g.packageName
        case .app(let p): return p.name
        case .android: return nil
        }
    }

    /// A gameplay picture, for apps that have been run on an iPhone (fetched from GitHub; see ShowcaseStore).
    @MainActor var artworkPath: String? { ShowcaseStore.shared.pictures(for: packageName).first }

    var runsInAndroid: Bool {
        if case .app = self { return true }
        return false
    }

    var lastUsed: Date? {
        switch self {
        case .game(let g): return g.lastPlayed
        case .app(let p): return p.lastUsed
        case .android: return UserDefaults.standard.object(forKey: "husk.android.lastOpened") as? Date
        }
    }

    var route: LibraryRoute {
        switch self {
        case .game(let g): return .game(g.id)
        case .app(let p): return .android(p)
        case .android: return .androidSystem
        }
    }

    /// "Played 2 hours ago", or nothing for what was never opened from Husk.
    var usedText: String? {
        guard let date = lastUsed else { return nil }
        let rel = RelativeDateTimeFormatter()
        rel.unitsStyle = .full
        let ago = date.timeIntervalSinceNow > -60 ? "just now" : rel.localizedString(for: date, relativeTo: Date())
        return (isGame ? "Played " : "Opened ") + ago
    }
}

/// Which side Home shows: games run on the iPhone itself, or Android and its apps.
enum HomeMode: String, CaseIterable, Identifiable {
    case translation, android
    var id: String { rawValue }
    var title: String { self == .translation ? "Translation Layer" : "Android" }
    var detail: String { self == .translation ? "Games running directly on iPhone" : "Apps inside the emulated Android" }
    var systemImage: String { self == .translation ? "bolt.fill" : "apps.iphone" }
}

/// Which items a library page shows.
enum LibraryFilter: String, CaseIterable, Identifiable {
    case all, games, apps
    var id: String { rawValue }
    var title: String {
        switch self {
        case .all: return "All"
        case .games: return "Games"
        case .apps: return "Apps"
        }
    }
}

/// Everything, from both stores, in one list.
@MainActor
enum Launcher {
    static func items(store: TranslationLayerStore, host: AndroidHost) -> [LibraryItem] {
        store.apps.map(LibraryItem.game) + host.packages.map(LibraryItem.app)
    }

    /// Most recently used first; never-used ones after, by name.
    static func byRecent(_ items: [LibraryItem]) -> [LibraryItem] {
        items.sorted { a, b in
            switch (a.lastUsed, b.lastUsed) {
            case let (x?, y?): return x > y
            case (_?, nil): return true
            case (nil, _?): return false
            default: return a.title.localizedCaseInsensitiveCompare(b.title) == .orderedAscending
            }
        }
    }

    static var jitOn: Bool { JITBootstrap.ready }
}

// MARK: - Home

/// What you used last, then your games, then your apps.
struct HomeView: View {
    @ObservedObject private var router = Router.shared
    @ObservedObject private var store = TranslationLayerStore.shared
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var jit = JITCoordinator.shared
    @ObservedObject private var incoming = IncomingFiles.shared
    @AppStorage("husk.home.mode") private var mode: HomeMode = .translation

    /// This side's items, most recent first. On the Android side Android itself leads until an app has been opened.
    private var all: [LibraryItem] {
        switch mode {
        case .translation: return Launcher.byRecent(store.apps.map(LibraryItem.game))
        case .android: return Launcher.byRecent(host.packages.map(LibraryItem.app)) + [.android]
        }
    }

    var body: some View {
        NavigationStack(path: $router.home) {
            ScrollView {
                VStack(alignment: .leading, spacing: 24) {
                    HStack(spacing: 10) {
                        Text("Husk")
                            .font(.system(size: 34, weight: .heavy, design: .rounded))
                        ModeMenu(mode: $mode)
                        Spacer()
                        HeaderButton(systemImage: "plus") { router.addSomething() }
                            .accessibilityLabel("Add a Game or App")
                    }
                    if !Launcher.jitOn { JITCard(compact: true) }
                    if let busy = incoming.preparing { BusyStrip(text: busy) }
                    if let busy = store.busy { BusyStrip(text: busy) }
                    if let busy = host.busy { BusyStrip(text: busy) }
                    if jit.busy, !jit.showSetup { BusyStrip(text: jit.status ?? "Turning on JIT…") }

                    let items = all
                    if items.isEmpty {
                        WelcomeCard()
                    } else {
                        let lead = items.first { $0.lastUsed != nil } ?? (mode == .android ? .android : items[0])
                        HeroCard(item: lead)

                        let games = items.filter { $0.isGame && $0 != lead }
                        if !games.isEmpty {
                            ShelfHeader(title: "Games") { router.showLibrary(.games) }
                            ScrollView(.horizontal) {
                                LazyHStack(alignment: .top, spacing: 12) {
                                    ForEach(games) { item in
                                        NavigationLink(value: item.route) { CoverTile(item: item) }
                                            .buttonStyle(CardButtonStyle())
                                    }
                                }
                                .padding(.horizontal, 20)
                            }
                            .scrollIndicators(.hidden)
                            .padding(.horizontal, -20)
                        }

                        let apps = items.filter { !$0.isGame && $0 != lead }
                        if !apps.isEmpty {
                            ShelfHeader(title: "Apps") { router.showLibrary(.apps) }
                            LazyVGrid(columns: [GridItem(.adaptive(minimum: 64, maximum: 90), spacing: 8, alignment: .top)],
                                      spacing: 14) {
                                ForEach(apps.prefix(UIDevice.current.userInterfaceIdiom == .pad ? 16 : 8)) { item in
                                    NavigationLink(value: item.route) {
                                        LauncherTile(title: item.title, iconPath: item.iconPath, size: 50)
                                    }
                                    .buttonStyle(CardButtonStyle())
                                }
                            }
                            .padding(14)
                            .background(Color(uiColor: .secondarySystemBackground),
                                        in: RoundedRectangle(cornerRadius: 22, style: .continuous))
                        }
                    }
                }
                .padding(.horizontal, 20)
                .padding(.top, 8)
                .padding(.bottom, 32)
                .id(jit.attachGeneration)
            }
            .scrollIndicators(.hidden)
            .background(Color(uiColor: .systemBackground).ignoresSafeArea())
            .toolbar(.hidden, for: .navigationBar)
            .libraryDestinations()
        }
    }
}

/// The first time: nothing added yet, and the two ways to start.
private struct WelcomeCard: View {
    @ObservedObject private var router = Router.shared

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Image(systemName: "gamecontroller.fill")
                .font(.system(size: 28, weight: .semibold))
                .foregroundStyle(Color.accentColor)
                .frame(width: 60, height: 60)
                .background(Color.accentColor.opacity(0.15), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            VStack(alignment: .leading, spacing: 6) {
                Text("Add your first game")
                    .font(.system(.title2, design: .rounded).weight(.bold))
                Text("Pick an APK or a bundle (.xapk, .apkm, .apks), or share one to Husk from Files or Safari. Games run "
                   + "straight on your iPhone. Other apps run inside Android.")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Button { router.addSomething() } label: {
                Text("Add a Game or App").frame(maxWidth: .infinity)
            }
            .buttonStyle(PrimaryButtonStyle())
        }
        .padding(20)
        .background(Color(uiColor: .secondarySystemBackground), in: RoundedRectangle(cornerRadius: 24, style: .continuous))
    }
}

/// The side Home shows, as a pull-down beside the title: the current one, and a tap away the other, each saying what it is.
private struct ModeMenu: View {
    @Binding var mode: HomeMode

    var body: some View {
        Menu {
            Picker("Show", selection: $mode) {
                ForEach(HomeMode.allCases) { m in
                    Label { Text(m.title); Text(m.detail) } icon: { Image(systemName: m.systemImage) }
                        .tag(m)
                }
            }
        } label: {
            HStack(spacing: 4) {
                Text(mode == .translation ? "Translated" : "Android")
                    .font(.subheadline.weight(.semibold))
                Image(systemName: "chevron.up.chevron.down")
                    .font(.system(size: 10, weight: .bold))
            }
            .foregroundStyle(Color.accentColor)
            .padding(.horizontal, 10)
            .frame(height: 28)
            .background(Color.accentColor.opacity(0.14), in: Capsule())
        }
        .padding(.top, 4)
    }
}

private struct ShelfHeader: View {
    let title: String
    let seeAll: () -> Void

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title).font(.system(.title3, design: .rounded).weight(.bold))
            Spacer()
            Button("See All", action: seeAll).font(.subheadline.weight(.semibold))
        }
        .padding(.bottom, -10)
    }
}

// MARK: - artwork

/// A picture to stand in for artwork, made from the icon: the icon itself, enlarged and blurred to fill the space,
/// under a dark fade so text on it reads. Android icons are all most apps carry, and this is what makes them big.
struct IconBackdrop: View {
    let path: String?
    var fallback: Color = Color(uiColor: .secondarySystemBackground)

    var body: some View {
        ZStack {
            fallback
            if path != nil {
                AppIcon(path: path, size: 120)
                    .scaleEffect(3.2)
                    .blur(radius: 28)
                    .saturation(1.3)
                    .opacity(0.9)
            }
        }
        .clipped()
        // Scaled up, the icon reaches far past the card; clipping hides that but does not stop it taking touches, and a
        // tap on one cover used to land on its neighbour's backdrop.
        .allowsHitTesting(false)
    }
}

/// What sits behind an item's name: its gameplay picture when Husk ships one, its blurred icon otherwise.
struct ItemBackdrop: View {
    let item: LibraryItem
    var pixels: Int = 1200
    var fallback: Color = Color(uiColor: .secondarySystemBackground)
    @ObservedObject private var showcase = ShowcaseStore.shared

    var body: some View {
        Group {
            if let art = item.artworkPath {
                Color.clear.overlay { PictureView(path: art, pixels: pixels) }.clipped()
            } else {
                IconBackdrop(path: item.iconPath, fallback: item == .android ? AndroidMark.green.opacity(0.55) : fallback)
            }
        }
        .allowsHitTesting(false)
    }
}

/// The big card on Home: the thing you used last, ready to go again.
private struct HeroCard: View {
    let item: LibraryItem
    @ObservedObject private var router = Router.shared

    var body: some View {
        NavigationLink(value: item.route) {
            ZStack(alignment: .bottomLeading) {
                ItemBackdrop(item: item)
                LinearGradient(colors: [.clear, .black.opacity(0.75)], startPoint: .top, endPoint: .bottom)
                HStack(alignment: .bottom, spacing: 14) {
                    ItemIcon(item: item, size: 64)
                        .shadow(color: .black.opacity(0.35), radius: 10, y: 4)
                    VStack(alignment: .leading, spacing: 3) {
                        Text(item.title)
                            .font(.system(.title3, design: .rounded).weight(.heavy))
                            .lineLimit(2)
                        Text(item.usedText ?? (item.isGame ? "Ready to play" : "Ready to open"))
                            .font(.caption)
                            .foregroundStyle(.white.opacity(0.75))
                    }
                    .foregroundStyle(.white)
                    Spacer(minLength: 8)
                    Text(item.isGame ? "Play" : "Open")
                        .font(.subheadline.weight(.bold))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 18)
                        .frame(height: 36)
                        .background(Color.accentColor, in: Capsule())
                }
                .padding(16)
            }
            .frame(height: 200)
            .clipShape(RoundedRectangle(cornerRadius: 26, style: .continuous))
            .contentShape(RoundedRectangle(cornerRadius: 26, style: .continuous))
        }
        .buttonStyle(CardButtonStyle())
    }
}

/// A game on Home's shelf: a tall card with the game's own colours behind its icon.
private struct CoverTile: View {
    let item: LibraryItem

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            ZStack {
                ItemBackdrop(item: item, pixels: 420)
                LinearGradient(colors: [.clear, .black.opacity(0.35)], startPoint: .center, endPoint: .bottom)
                ItemIcon(item: item, size: 58)
                    .shadow(color: .black.opacity(0.3), radius: 8, y: 3)
            }
            .frame(width: 104, height: 136)
            .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
            .contentShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
            .overlay(alignment: .topTrailing) { if item.runsInAndroid { AndroidBadge().padding(7) } }
            Text(item.title)
                .font(.caption.weight(.semibold))
                .foregroundStyle(.primary)
                .lineLimit(1)
            Text(item.usedText ?? " ")
                .font(.caption2)
                .foregroundStyle(.secondary)
                .lineLimit(1)
        }
        .frame(width: 104)
    }
}

/// An item's icon: the app's own, or Android's mark for Android itself.
struct ItemIcon: View {
    let item: LibraryItem
    let size: CGFloat

    var body: some View {
        if item == .android {
            AndroidMark(size: size)
        } else {
            AppIcon(path: item.iconPath, size: size)
                .overlay {
                    RoundedRectangle(cornerRadius: size * 0.225, style: .continuous)
                        .strokeBorder(Color.primary.opacity(0.08), lineWidth: 0.5)
                }
        }
    }
}

/// Android itself, as an icon.
struct AndroidMark: View {
    static let green = Color(red: 0.24, green: 0.86, blue: 0.52)
    var size: CGFloat = 64

    var body: some View {
        Image(systemName: "apps.iphone")
            .font(.system(size: size * 0.46, weight: .semibold))
            .foregroundStyle(.white)
            .frame(width: size, height: size)
            .background(Self.green.gradient, in: RoundedRectangle(cornerRadius: size * 0.225, style: .continuous))
    }
}

/// "Opens inside Android".
struct AndroidBadge: View {
    var body: some View {
        Text("ANDROID")
            .font(.system(size: 8, weight: .heavy, design: .rounded))
            .tracking(0.4)
            .foregroundStyle(Color(red: 0.03, green: 0.13, blue: 0.06))
            .padding(.horizontal, 5).padding(.vertical, 2)
            .background(AndroidMark.green, in: Capsule())
    }
}

// MARK: - Library

/// Every game and app, in one grid: search, and All / Games / Apps.
struct LibraryScreen: View {
    @ObservedObject private var router = Router.shared
    @ObservedObject private var store = TranslationLayerStore.shared
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var guest = GuestImage.shared
    @State private var query = ""

    var body: some View {
        NavigationStack(path: $router.library) {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    HStack {
                        Text("Library")
                            .font(.system(size: 34, weight: .heavy, design: .rounded))
                        Spacer()
                        HeaderButton(systemImage: "plus") { router.addSomething() }
                            .accessibilityLabel("Add a Game or App")
                    }
                    SearchField(text: $query, prompt: "Search games and apps")
                    FilterChips(filter: $router.libraryFilter)
                    if let busy = store.busy { BusyStrip(text: busy) }
                    if let busy = host.busy { BusyStrip(text: busy) }

                    let items = Launcher.items(store: store, host: host)
                        .filter { matches(query, $0.title) }
                        .sorted { $0.title.localizedCaseInsensitiveCompare($1.title) == .orderedAscending }
                    let games = items.filter(\.isGame)
                    // Android, as an app of its own: pinned first, and found by search like anything else.
                    let apps = (matches(query, "Android") ? [LibraryItem.android] : []) + items.filter { !$0.isGame }

                    if router.libraryFilter != .apps {
                        section("Games", games, empty: query.isEmpty ? "Add a game with the + button." : nil)
                    }
                    if router.libraryFilter != .games {
                        section("Apps", apps, empty: nil)
                    }
                    if !query.isEmpty, games.isEmpty, apps.isEmpty { NoResults(query: query) }
                }
                .padding(.horizontal, 20)
                .padding(.top, 8)
                .padding(.bottom, 32)
            }
            .scrollIndicators(.hidden)
            .scrollDismissesKeyboard(.immediately)
            .background(Color(uiColor: .systemBackground).ignoresSafeArea())
            .toolbar(.hidden, for: .navigationBar)
            .libraryDestinations()
        }
    }

    @ViewBuilder
    private func section(_ title: String, _ items: [LibraryItem], empty: String?) -> some View {
        if !items.isEmpty || empty != nil {
            HStack(alignment: .firstTextBaseline) {
                Text(title).font(.system(.title3, design: .rounded).weight(.bold))
                Text("\(items.count)").font(.subheadline).foregroundStyle(.secondary)
            }
            .padding(.top, 6)
            if items.isEmpty, let empty {
                Text(empty).font(.subheadline).foregroundStyle(.secondary)
            } else {
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 76, maximum: 110), spacing: 12, alignment: .top)], spacing: 20) {
                    ForEach(items) { item in
                        NavigationLink(value: item.route) {
                            LauncherTile(title: item.title, iconPath: item.iconPath, item: item, caption: caption(item))
                        }
                        .buttonStyle(CardButtonStyle())
                    }
                }
            }
        }
    }

    private func caption(_ item: LibraryItem) -> String? {
        switch item {
        case .game(let g):
            if g.packageName == GeodeSupport.launcherPackage { return "Geode" }
            return g.report?.canRun == true ? nil : "May not run"
        case .android:
            if AndroidHost.shared.isReady { return "Running" }
            return guest.state == .ready ? nil : "Not installed"
        case .app: return nil
        }
    }
}

private struct FilterChips: View {
    @Binding var filter: LibraryFilter

    var body: some View {
        HStack(spacing: 8) {
            ForEach(LibraryFilter.allCases) { f in
                Button {
                    withAnimation(.easeOut(duration: 0.15)) { filter = f }
                } label: {
                    Text(f.title)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(filter == f ? Color(uiColor: .systemBackground) : Color.primary)
                        .padding(.horizontal, 16)
                        .frame(height: 32)
                        .background(filter == f ? Color.primary : Color(uiColor: .tertiarySystemFill), in: Capsule())
                }
                .buttonStyle(.plain)
            }
        }
    }
}

// MARK: - shared pieces

/// One app in a grid, as a home screen draws it: the icon, and its name under it.
struct LauncherTile: View {
    let title: String
    let iconPath: String?
    var item: LibraryItem? = nil
    var caption: String? = nil
    var size: CGFloat = 64
    @ObservedObject private var statuses = GameStatusStore.shared

    /// How the game did last time it was played, when Husk has seen it played.
    private var result: GameStatusStore.Result? {
        guard case .game(let g)? = item else { return nil }
        return statuses.status(g.id)?.result
    }

    var body: some View {
        VStack(spacing: 7) {
            Group {
                if let item { ItemIcon(item: item, size: size) } else { AppIcon(path: iconPath, size: size) }
            }
            .shadow(color: .black.opacity(0.14), radius: 6, y: 3)
            .overlay(alignment: .bottom) {
                if item?.runsInAndroid == true { AndroidBadge().offset(y: 6) }
            }
            .overlay(alignment: .topTrailing) {
                if let result { GameStatusBadge(result: result).offset(x: 5, y: -5) }
            }
            VStack(spacing: 1) {
                Text(title)
                    .font(.caption.weight(.medium))
                    .foregroundStyle(.primary)
                    .lineLimit(2)
                    .multilineTextAlignment(.center)
                if let caption {
                    Text(caption).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                }
            }
            .padding(.top, item?.runsInAndroid == true ? 3 : 0)
        }
        .frame(maxWidth: .infinity)
        .contentShape(Rectangle())
    }
}

/// A round glyph button in a screen's header.
struct HeaderButton: View {
    let systemImage: String
    let action: () -> Void
    @Environment(\.isEnabled) private var enabled

    var body: some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .font(.system(size: 16, weight: .bold))
                .foregroundStyle(enabled ? Color.accentColor : Color.secondary)
                .frame(width: 38, height: 38)
                .background(Color(uiColor: .tertiarySystemFill), in: Circle())
        }
        .buttonStyle(.plain)
    }
}

/// Work under way, said in one line.
struct BusyStrip: View {
    let text: String

    var body: some View {
        HStack(spacing: 12) {
            ProgressView()
            Text(text).font(.subheadline).lineLimit(2)
            Spacer(minLength: 0)
        }
        .padding(14)
        .background(Color(uiColor: .secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
    }
}

/// A search field as the system draws one.
struct SearchField: View {
    @Binding var text: String
    let prompt: String
    @FocusState private var focused: Bool

    var body: some View {
        HStack(spacing: 8) {
            HStack(spacing: 6) {
                Image(systemName: "magnifyingglass")
                    .font(.system(size: 15, weight: .medium))
                    .foregroundStyle(.secondary)
                TextField(prompt, text: $text)
                    .focused($focused)
                    .submitLabel(.search)
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                if !text.isEmpty {
                    Button { text = "" } label: {
                        Image(systemName: "xmark.circle.fill").foregroundStyle(.tertiary)
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel("Clear")
                }
            }
            .padding(.horizontal, 10)
            .frame(height: 38)
            .background(Color(uiColor: .tertiarySystemFill), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
            if focused {
                Button("Cancel") { text = ""; focused = false }
                    .transition(.move(edge: .trailing).combined(with: .opacity))
            }
        }
        .animation(.easeOut(duration: 0.2), value: focused)
    }
}

/// Nothing matches the search.
struct NoResults: View {
    let query: String
    var body: some View {
        VStack(spacing: 8) {
            Image(systemName: "magnifyingglass").font(.system(size: 28, weight: .light)).foregroundStyle(.secondary)
            Text("No Results").font(.headline)
            Text("Nothing here is called “\(query)”.").font(.subheadline).foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 48)
    }
}

/// Whether a name matches what was searched for.
func matches(_ query: String, _ names: String...) -> Bool {
    let q = query.trimmingCharacters(in: .whitespaces)
    return q.isEmpty || names.contains { $0.localizedCaseInsensitiveContains(q) }
}

extension View {
    /// The pages an item opens, for whichever stack shows it.
    func libraryDestinations() -> some View {
        navigationDestination(for: LibraryRoute.self) { route in
            LibraryRouteView(route: route).toolbar(.visible, for: .navigationBar)
        }
    }
}

private struct LibraryRouteView: View {
    let route: LibraryRoute
    @ObservedObject private var store = TranslationLayerStore.shared

    var body: some View {
        switch route {
        case .game(let id):
            if let app = store.apps.first(where: { $0.id == id }) { GamePage(app: app) } else { GoneView() }
        case .gameSettings(let id):
            if let app = store.apps.first(where: { $0.id == id }) { TLAppSettingsView(app: app) } else { GoneView() }
        case .gameReport(let id):
            if let app = store.apps.first(where: { $0.id == id }) { TLTechnicalView(app: app) } else { GoneView() }
        case .android(let pkg):
            AndroidAppPage(app: pkg)
        case .androidSystem:
            AndroidSystemPage()
        }
    }
}

private struct GoneView: View {
    var body: some View {
        Text("This was removed.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
