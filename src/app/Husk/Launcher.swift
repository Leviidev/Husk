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
    case ps4(PS4Game)

    var id: String {
        switch self {
        case .game(let g): return "tl:" + g.id
        case .ps4(let g): return g.libraryID
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
        case .ps4(let g): return g.title
        }
    }

    var iconPath: String? {
        switch self {
        case .game(let g): return g.iconPath
        case .app(let p): return p.iconPath
        case .android: return nil
        case .ps4(let g): return g.iconPath
        }
    }

    /// Games on the translation layer (a Flutter app is an app unless it calls itself a game), and what Android itself files as a game.
    var isGame: Bool {
        switch self {
        case .game(let g): return (g.report?.nativeEngine != .flutter && g.report?.engine != "Flutter" && g.report?.webKind == nil) || g.isGameCategory
        case .app(let p): return p.category == "Game"
        case .android: return false
        case .ps4: return true
        }
    }

    var packageName: String? {
        switch self {
        case .game(let g): return g.packageName
        case .app(let p): return p.name
        case .android: return nil
        case .ps4: return nil
        }
    }

    /// A gameplay picture, for apps that have been run on an iPhone (fetched from GitHub; see ShowcaseStore).
    @MainActor var artworkPath: String? {
        if case .ps4(let g) = self { return g.artPath }   // the game's own hero picture (pic1)
        return ShowcaseStore.shared.pictures(for: packageName).first
    }

    var runsInAndroid: Bool {
        if case .app = self { return true }
        return false
    }

    var lastUsed: Date? {
        switch self {
        case .game(let g): return g.lastPlayed
        case .ps4(let g): return g.lastPlayed
        case .app(let p): return p.lastUsed
        case .android: return UserDefaults.standard.object(forKey: "husk.android.lastOpened") as? Date
        }
    }

    var route: LibraryRoute {
        switch self {
        case .game(let g): return .game(g.id)
        case .app(let p): return .android(p)
        case .android: return .androidSystem
        case .ps4(let g): return .ps4(g.titleID)
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

/// How big the Library's icons are: large (three across on a phone), or small, for a long library (five across).
enum LibraryIconSize: String, CaseIterable, Identifiable {
    case list, large, small
    static let key = "husk.library.iconSize"
    var id: String { rawValue }
    var title: String {
        switch self {
        case .list: return "List"
        case .large: return "Large Icons"
        case .small: return "Small Icons"
        }
    }
    var symbol: String {
        switch self {
        case .list: return "list.bullet"
        case .large: return "square.grid.2x2"
        case .small: return "square.grid.3x3"
        }
    }
    var columns: Int {
        let pad = UIDevice.current.userInterfaceIdiom == .pad
        switch self {
        case .large, .list: return pad ? 6 : 3
        case .small: return pad ? 9 : 5
        }
    }
}

/// The Library's pills: everything, Android games, PS4 games, or apps.
enum LibraryPill: String, CaseIterable, Identifiable {
    case all, android, ps4, apps
    var id: String { rawValue }
    var title: String {
        switch self {
        case .all: return "All"
        case .android: return "Android"
        case .ps4: return "PS4"
        case .apps: return "Apps"
        }
    }
    func includes(_ i: LibraryItem) -> Bool {
        switch self {
        case .all: return true
        case .android: return !i.isPS4 && i.isGame
        case .ps4: return i.isPS4
        case .apps: return !i.isPS4 && !i.isGame
        }
    }
}

/// Everything, from both stores, in one list.
@MainActor
enum Launcher {
    static func items(store: TranslationLayerStore, host: AndroidHost) -> [LibraryItem] {
        store.apps.map(LibraryItem.game) + PS4Store.shared.games.map(LibraryItem.ps4) + host.packages.map(LibraryItem.app)
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

    /// Play or open an item: its page, told to start the game as soon as it is up (Android apps just open their page).
    static func play(_ item: LibraryItem, push: (LibraryRoute) -> Void) {
        switch item {
        case .game(let g): Router.shared.autoPlay = g.id
        case .ps4(let g): Router.shared.autoPlay = g.libraryID
        default: break
        }
        push(item.route)
    }
}

// MARK: - Home

/// Home: search, the game played last as a big card, and every game and app in a grid.
struct HomeView: View {
    @ObservedObject private var router = Router.shared
    @ObservedObject private var store = TranslationLayerStore.shared
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var ps4 = PS4Store.shared
    @ObservedObject private var jit = JITCoordinator.shared
    @ObservedObject private var incoming = IncomingFiles.shared
    @ObservedObject private var statuses = GameStatusStore.shared
    @ObservedObject private var compat = CompatibilityStore.shared
    @ObservedObject private var showcase = ShowcaseStore.shared
    @AppStorage(Holiday.key) private var holiday = true
    @State private var query = ""

    /// Everything, most recent first.
    private var items: [LibraryItem] { Launcher.byRecent(Launcher.items(store: store, host: host)) }

    private let columns = Array(repeating: GridItem(.flexible(), spacing: 12, alignment: .top),
                                count: UIDevice.current.userInterfaceIdiom == .pad ? 6 : 3)

    var body: some View {
        NavigationStack(path: $router.home) {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    topBar
                    SearchField(text: $query, prompt: "Search your games…")
                    notices
                    let items = self.items
                    if items.isEmpty {
                        WelcomeCard()
                    } else {
                        let shown = items.filter { matches(query, $0.title, $0.packageName ?? "") }
                        if query.isEmpty, let recent = items.first(where: { $0.lastUsed != nil }) ?? items.first {
                            RecentlyPlayedCard(item: recent) { Launcher.play(recent) { router.home.append($0) } }
                        }
                        HStack(alignment: .firstTextBaseline) {
                            Text(query.isEmpty ? "Your Games" : "Results")
                                .font(.system(size: 20, weight: .bold))
                                .foregroundStyle(Console.text)
                            Spacer()
                            if query.isEmpty, items.count > 6 {
                                Button("See All") { router.tab = .library }
                                    .font(.system(size: 14, weight: .semibold))
                                    .foregroundStyle(Theme.accent)
                            }
                        }
                        .padding(.top, 4)
                        if shown.isEmpty {
                            NoResults(query: query)
                        } else {
                            LazyVGrid(columns: columns, spacing: 16) {
                                ForEach(shown.prefix(query.isEmpty ? 12 : 90)) { item in
                                    HuskLink(value: item.route) { HomeGameTile(item: item) }
                                        .buttonStyle(CardButtonStyle())
                                        .accessibilityLabel(item.title)
                                        .libraryItemMenu(item) { router.home.append($0) }
                                }
                            }
                        }
                    }
                }
                .padding(.horizontal, 20)
                .padding(.top, 6)
                .padding(.bottom, 24)
            }
            .huskHideScrollIndicators()
            .background(Console.page.ignoresSafeArea())
            .huskNavigationBar(hidden: true)
            .libraryDestinations()
        }
    }

    // MARK: top bar

    private var topBar: some View {
        HStack(spacing: 10) {
            Text("Husk")
                .font(.system(size: 30, weight: .bold, design: .rounded))
                .foregroundStyle(Console.text)
                .fixedSize()
            if holiday && Holiday.inSeason { ChristmasTree(size: 22) }
            Spacer(minLength: 4)
            Button { router.addSomething() } label: {
                Image(systemName: "plus")
                    .font(.system(size: 15, weight: .bold))
                    .foregroundStyle(Console.text)
                    .frame(width: 36, height: 36)
                    .background(Console.raised, in: Circle())
            }
            .accessibilityLabel("Add a game or app")
            HomeAvatar()
        }
        .frame(height: 48)
    }

    /// Work under way, and JIT when it is off.
    @ViewBuilder
    private var notices: some View {
        VStack(spacing: 8) {
            if let busy = incoming.preparing { BusyStrip(text: busy) }
            if let busy = store.busy { BusyStrip(text: busy) }
            if let busy = host.busy { BusyStrip(text: busy) }
            DownloadsNotice()
            if !Launcher.jitOn && !JITLess.enabled { JITNotice() }
        }
    }
}

/// The game played last, as a wide card: its picture, its name, "Recently Played" and a round Play button.
private struct RecentlyPlayedCard: View {
    let item: LibraryItem
    let play: () -> Void
    @ObservedObject private var router = Router.shared

    var body: some View {
        HuskLink(value: item.route) {
            ZStack(alignment: .bottomLeading) {
                ItemBackdrop(item: item, pixels: 1200, fallback: Console.card)
                // a scrim under the words, so they read on any picture
                LinearGradient(colors: [.clear, .black.opacity(0.75)], startPoint: .center, endPoint: .bottom)
                HStack(alignment: .bottom) {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(item.title)
                            .font(.system(size: 24, weight: .bold))
                            .foregroundStyle(.white)
                            .lineLimit(2)
                            .multilineTextAlignment(.leading)
                        Text(item.lastUsed == nil ? "Recently Added" : "Recently Played")
                            .font(.system(size: 12, weight: .semibold))
                            .foregroundStyle(.white.opacity(0.9))
                            .padding(.horizontal, 8).frame(height: 22)
                            .background(.white.opacity(0.18), in: Capsule())
                    }
                    Spacer(minLength: 8)
                    Button(action: play) {
                        Image(systemName: item.isGame ? "play.fill" : "arrow.up.forward")
                            .font(.system(size: 17, weight: .bold))
                            .foregroundStyle(.black)
                            .frame(width: 46, height: 46)
                            .background(Color.white, in: Circle())
                    }
                    .accessibilityLabel((item.isGame ? "Play " : "Open ") + item.title)
                }
                .padding(16)
            }
            .frame(height: 180)
            .frame(maxWidth: .infinity)
            .clipShape(RoundedRectangle(cornerRadius: 20, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous).stroke(Console.outline, lineWidth: 0.5))
            .contentShape(RoundedRectangle(cornerRadius: 20, style: .continuous))
        }
        .buttonStyle(CardButtonStyle())
        .libraryItemMenu(item) { router.home.append($0) }
    }
}

/// One game in Home's grid: the icon as a rounded square, the name, and the platform with its status dot.
struct HomeGameTile: View {
    let item: LibraryItem

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Color.clear
                .aspectRatio(1, contentMode: .fit)
                .overlay { GeometryReader { geo in ItemIcon(item: item, size: geo.size.width) } }
                .shadow(color: .black.opacity(0.3), radius: 5, y: 3)
            Text(item.title)
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(Console.text)
                .lineLimit(1)
            HStack(spacing: 4) {
                Circle().fill(item.status?.color ?? Console.textDim).frame(width: 6, height: 6)
                Text(item.platformShort)
                    .font(.system(size: 11))
                    .foregroundStyle(Console.textDim)
                    .lineLimit(1)
            }
        }
    }
}

extension LibraryItem {
    /// "Android", "PS4" or "App": the platform as a word under a tile.
    var platformShort: String {
        switch self {
        case .ps4: return "PS4"
        case .app, .android: return "Android"
        case .game: return "Android"
        }
    }
}

/// What fills the top of Home: the selected item's own artwork, else its icon blown up and blurred, dimmed with plain
/// black and faded into the page at its bottom edge.
private struct HomeHero: View {
    let item: LibraryItem?
    let width: CGFloat
    let height: CGFloat
    @ObservedObject private var showcase = ShowcaseStore.shared

    var body: some View {
        ZStack {
            Color.black
            if let item {
                art(item)
                    .id(item.id)
                    .transition(.opacity)
            }
            Color.black.opacity(0.45)
            VStack(spacing: 0) {
                Spacer(minLength: 0)
                LinearGradient(colors: [.clear, Console.page], startPoint: .top, endPoint: .bottom)
                    .frame(height: height * 0.32)
            }
        }
        .frame(width: width, height: height)
        .clipped()
        .allowsHitTesting(false)
        .animation(.easeInOut(duration: 0.35), value: item?.id)
    }

    @ViewBuilder
    private func art(_ item: LibraryItem) -> some View {
        if let art = item.artworkPath {
            Color.clear.overlay { PictureView(path: art, pixels: 1600) }.clipped()
        } else if item == .android {
            AndroidMark.green.opacity(0.5)
        } else if item.iconPath != nil {
            AppIcon(path: item.iconPath, size: 160)
                .scaleEffect(max(width, height) / 160 * 1.25)
                .blur(radius: 36)
                .frame(width: width, height: height)
        }
    }
}

/// One tile on Home's shelf: the icon, larger with a white ring when selected.
private struct ShelfTile: View {
    let item: LibraryItem
    let selected: Bool

    var body: some View {
        let size: CGFloat = selected ? 92 : 72
        ItemIcon(item: item, size: size)
            .overlay(alignment: .bottomTrailing) {
                if item.runsInAndroid { AndroidBadge().padding(4) }
            }
            .padding(selected ? 4 : 0)
            .overlay {
                if selected {
                    RoundedRectangle(cornerRadius: size * 0.225 + 4, style: .continuous)
                        .strokeBorder(Console.text, lineWidth: 3)
                }
            }
            .shadow(color: .black.opacity(selected ? 0.5 : 0.3), radius: selected ? 12 : 6, y: 4)
            .opacity(selected ? 1 : 0.85)
            .contentShape(Rectangle())
    }
}

/// A wide card in "Recently added": the item's picture, its icon and its name.
private struct WideCard: View {
    let item: LibraryItem

    var body: some View {
        ZStack(alignment: .bottomLeading) {
            ItemBackdrop(item: item, pixels: 800, fallback: Console.card)
            Color.black.opacity(0.35)
            HStack(spacing: 10) {
                ItemIcon(item: item, size: 38)
                VStack(alignment: .leading, spacing: 2) {
                    Text(item.title)
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(.white)
                        .lineLimit(1)
                    Text(item.platformName)
                        .font(.system(size: 12))
                        .foregroundStyle(.white.opacity(0.7))
                        .lineLimit(1)
                }
                Spacer(minLength: 0)
                if let status = item.status { StatusDot(status: status, size: 8) }
            }
            .padding(12)
        }
        .frame(width: 250, height: 140)
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(Console.hairline, lineWidth: 0.5))
        .contentShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
    }
}

/// Home's big white Play button.
struct ConsolePlayButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 17, weight: .bold))
            .foregroundStyle(Theme.accent.isLight ? Color.black : Color.white)
            .frame(maxWidth: .infinity)
            .frame(height: 52)
            .background(Theme.accent, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            .opacity(configuration.isPressed ? 0.85 : 1)
            .scaleEffect(configuration.isPressed ? 0.98 : 1)
            .animation(.easeOut(duration: 0.12), value: configuration.isPressed)
    }
}

/// The square "⋯" beside Play: the same actions as a long press on the tile.
private struct HomeMoreButton: View {
    let item: LibraryItem
    @ObservedObject private var router = Router.shared
    @State private var confirmRemove = false

    var body: some View {
        Menu {
            LibraryItemActions(item: item, showsDetails: true, push: { router.home.append($0) }) { confirmRemove = true }
        } label: {
            Image(systemName: "ellipsis")
                .font(.system(size: 18, weight: .bold))
                .foregroundStyle(Console.text)
                .frame(width: 52, height: 52)
                .background(Console.raised, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
        }
        .accessibilityLabel("More for \(item.title)")
        .removeItemDialog(item, isPresented: $confirmRemove)
    }
}

/// The avatar in Home's corner: the account, adding things, Downloads, Files, JIT and Settings. Its dot is JIT: green on, orange off.
private struct HomeAvatar: View {
    @ObservedObject private var router = Router.shared
    @ObservedObject private var jit = JITCoordinator.shared
    @ObservedObject private var account = Account.shared

    var body: some View {
        Menu {
            Button { router.showProfile = true } label: {
                if let u = account.user, account.signedIn { Label(u.displayName, systemImage: "person.crop.circle") }
                else { Label("Sign In", systemImage: "person.crop.circle.badge.plus") }
            }
            Divider()
            Button { router.showImport = true } label: { Label("Import APK", systemImage: "square.and.arrow.down") }
            Button { router.showDownloads = true } label: { Label("Downloads", systemImage: "arrow.down.circle") }
            Button { router.showFiles = true } label: { Label("Files", systemImage: "folder") }
            Divider()
            Button { JITNotice.toggle(jit) } label: {
                Label(Launcher.jitOn ? "JIT On (\(JITNotice.via(jit)))" : "Turn On JIT", systemImage: "bolt.fill")
            }
            .disabled(jit.busy)
            Button { router.tab = .settings } label: { Label("Settings", systemImage: "gearshape") }
        } label: {
            ZStack(alignment: .topTrailing) {
                AccountAvatar(size: 36)
                Circle()
                    .fill(Launcher.jitOn ? Color.green : Color.orange)
                    .frame(width: 10, height: 10)
                    .overlay(Circle().stroke(Console.page, lineWidth: 2))
                    .offset(x: 1, y: -1)
            }
        }
        .accessibilityLabel("Menu")
        .accessibilityValue(Launcher.jitOn ? "JIT on" : "JIT off")
    }
}

/// JIT is off: say so, and the way to turn it on.
private struct JITNotice: View {
    @ObservedObject private var jit = JITCoordinator.shared
    @ObservedObject private var router = Router.shared

    static func via(_ jit: JITCoordinator) -> String {
        let m = jit.resolvedMethod
        return m == .builtIn ? "StikJIT" : m.title
    }

    static func toggle(_ jit: JITCoordinator) {
        if Launcher.jitOn { jit.showSetup = true; return }
        if HuskBuiltInJIT.isAvailable, jit.resolvedMethod == .builtIn { jit.method = .builtIn }
        jit.enable()
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Button { Self.toggle(jit) } label: {
                HStack(spacing: 10) {
                    Image(systemName: "bolt.fill")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundStyle(.orange)
                    Text("JIT is off").font(.system(size: 14, weight: .semibold)).foregroundStyle(.white)
                    Spacer(minLength: 0)
                    if jit.busy { ProgressView() } else {
                        Text("Turn On")
                            .font(.system(size: 13, weight: .bold))
                            .foregroundStyle(.black)
                            .padding(.horizontal, 12).frame(height: 28)
                            .background(Color.white, in: Capsule())
                    }
                }
                .padding(.horizontal, 12).padding(.vertical, 10)
                .background(Color.black.opacity(0.55), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).stroke(Console.hairline, lineWidth: 0.5))
            }
            .buttonStyle(CardButtonStyle())
            .disabled(jit.busy)
            Button { router.tab = .settings } label: {
                Text("Play without JIT")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(.white)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
                    .background(Color.black.opacity(0.55), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                    .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).stroke(Console.hairline, lineWidth: 0.5))
            }
            .buttonStyle(CardButtonStyle())
            if let error = jit.error {
                Text(error).font(.caption).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}

/// Downloads at a glance, only while there is something to say: what is coming in, a failure, an update.
private struct DownloadsNotice: View {
    @ObservedObject private var downloads = Downloads.shared
    @ObservedObject private var updates = AppUpdates.shared
    @ObservedObject private var guest = GuestImage.shared
    @ObservedObject private var router = Router.shared

    private var status: String? {
        let running = downloads.jobs.filter { !$0.complete && !$0.paused && !$0.failed }
        if !running.isEmpty {
            let total = running.reduce(Int64(0)) { $0 + $1.total }
            let got = running.reduce(Int64(0)) { $0 + $1.received }
            let pct = total > 0 ? " · \(Int(Double(got) / Double(total) * 100))%" : ""
            return "\(running.count) downloading\(pct)"
        }
        if downloads.jobs.contains(where: \.failed) { return "A download failed" }
        if let release = updates.available { return "Husk \(release.version) is out" }
        if guest.update.isSomething { return "Android update ready" }
        return nil
    }

    var body: some View {
        if let status {
            Button { router.showDownloads = true } label: {
                HStack(spacing: 10) {
                    Image(systemName: "arrow.down.circle.fill")
                        .font(.system(size: 16, weight: .semibold))
                        .foregroundStyle(Theme.accent)
                    Text(status).font(.system(size: 14, weight: .semibold)).foregroundStyle(.white).lineLimit(1)
                    Spacer(minLength: 0)
                    Image(systemName: "chevron.right").font(.system(size: 12, weight: .bold)).foregroundStyle(Console.textDim)
                }
                .padding(.horizontal, 12).padding(.vertical, 10)
                .background(Color.black.opacity(0.55), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).stroke(Console.hairline, lineWidth: 0.5))
            }
            .buttonStyle(CardButtonStyle())
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
                .foregroundStyle(Console.text)
                .frame(width: 60, height: 60)
                .background(Console.raised, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            VStack(alignment: .leading, spacing: 6) {
                Text("Add your first game")
                    .font(.system(size: 26, weight: .heavy))
                    .foregroundStyle(Console.text)
                Text("Pick an APK or a bundle (.xapk, .apkm, .apks), or share one to Husk from Files or Safari. Games run "
                   + "straight on your iPhone. Other apps run inside Android. PS4 games are extracted folders put in "
                   + "Husk's PS4 Games folder.")
                    .font(.subheadline)
                    .foregroundStyle(Console.textDim)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Button { router.addSomething() } label: { Text("Add a Game or App") }
                .buttonStyle(ConsolePlayButtonStyle())
        }
        .padding(20)
        .background(Console.card, in: RoundedRectangle(cornerRadius: 22, style: .continuous))
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
            .background(Self.green.huskGradient, in: RoundedRectangle(cornerRadius: size * 0.225, style: .continuous))
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

/// Every game and app: the platform chips, search, and a grid per platform.
struct LibraryScreen: View {
    @ObservedObject private var router = Router.shared
    @ObservedObject private var store = TranslationLayerStore.shared
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var ps4 = PS4Store.shared
    @ObservedObject private var guest = GuestImage.shared
    @ObservedObject private var statuses = GameStatusStore.shared
    @ObservedObject private var compat = CompatibilityStore.shared
    @State private var query = ""
    @AppStorage(LibraryIconSize.key) private var iconSize: LibraryIconSize = .list
    @State private var pill: LibraryPill = .all

    private var columns: [GridItem] {
        Array(repeating: GridItem(.flexible(), spacing: iconSize == .small ? 10 : 14, alignment: .top), count: iconSize.columns)
    }

    var body: some View {
        NavigationStack(path: $router.library) {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    SearchField(text: $query, prompt: "Search your games…")
                    HStack(spacing: 8) {
                        ForEach(LibraryPill.allCases) { p in
                            ConsoleChip(title: p.title, selected: pill == p) { withAnimation(.easeOut(duration: 0.18)) { pill = p } }
                        }
                        Spacer(minLength: 0)
                        Menu {
                            Picker("View", selection: $iconSize) {
                                ForEach(LibraryIconSize.allCases) { Label($0.title, systemImage: $0.symbol).tag($0) }
                            }
                        } label: {
                            Image(systemName: iconSize.symbol)
                                .font(.system(size: 17, weight: .semibold))
                                .foregroundStyle(Console.textDim)
                                .frame(width: 36, height: 36)
                        }
                        .accessibilityLabel("View")
                    }
                    if let busy = store.busy { BusyStrip(text: busy) }
                    if let busy = host.busy { BusyStrip(text: busy) }

                    let items = Launcher.items(store: store, host: host)
                        .filter(pill.includes)
                        .filter { matches(query, $0.title) }
                        .sorted { $0.title.localizedCaseInsensitiveCompare($1.title) == .orderedAscending }
                    let playstation = items.filter(\.isPS4)
                    let android = items.filter { !$0.isPS4 }

                    if pill == .all || pill == .ps4 {
                        section("PlayStation 4", playstation,
                                empty: pill == .ps4 && query.isEmpty
                                    ? (PS4Store.available ? "Put an extracted game folder in Husk's PS4 Games folder in Files."
                                                          : "PS4 games need iOS 17.4 or later.")
                                    : nil)
                    }
                    if pill != .ps4 {
                        section(pill == .apps ? "Apps" : "Android", android, empty: query.isEmpty && pill == .all ? "Add a game with the + button." : nil)
                    }
                    if !query.isEmpty, items.isEmpty { NoResults(query: query) }
                }
                .padding(.horizontal, 20)
                .padding(.top, 8)
                .padding(.bottom, 24)
            }
            .huskHideScrollIndicators()
            .huskDismissKeyboardOnScroll()
            .background(Console.page.ignoresSafeArea())
            .navigationTitle("Library")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button { router.addSomething() } label: { Image(systemName: "plus") }
                        .accessibilityLabel("Add a Game or App")
                }
            }
            .libraryDestinations()
        }
    }

    @ViewBuilder
    private func section(_ title: String, _ items: [LibraryItem], empty: String?) -> some View {
        if !items.isEmpty || empty != nil {
            VStack(alignment: .leading, spacing: 14) {
                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    Text(title).font(.system(size: 21, weight: .bold)).foregroundStyle(Console.text)
                    Text("\(items.count)").font(.system(size: 15, weight: .medium)).foregroundStyle(Console.textDim)
                }
                if items.isEmpty, let empty {
                    Text(empty)
                        .font(.subheadline)
                        .foregroundStyle(Console.textDim)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(16)
                        .background(Console.card, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                } else if iconSize == .list {
                    LazyVStack(spacing: 2) {
                        ForEach(items) { item in
                            HuskLink(value: item.route) { LibraryRow(item: item, caption: caption(item)) }
                                .buttonStyle(CardButtonStyle())
                                .accessibilityLabel(item.title)
                                .libraryItemMenu(item) { router.library.append($0) }
                        }
                    }
                } else {
                    LazyVGrid(columns: columns, spacing: iconSize == .small ? 14 : 20) {
                        ForEach(items) { item in
                            HuskLink(value: item.route) { LibraryCell(item: item, caption: caption(item), small: iconSize == .small) }
                                .buttonStyle(CardButtonStyle())
                                .accessibilityLabel(item.title)
                                .libraryItemMenu(item) { router.library.append($0) }
                        }
                    }
                }
            }
            .padding(.top, 8)
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
        case .app: return "Emulator"
        case .ps4: return PS4Store.available ? nil : "Needs iOS 17.4"
        }
    }
}

/// One item in the Library list: its icon, its name, the platform with its status dot, and a "⋯" for its actions.
private struct LibraryRow: View {
    let item: LibraryItem
    let caption: String?
    @ObservedObject private var router = Router.shared
    @State private var confirmRemove = false

    var body: some View {
        HStack(spacing: 14) {
            ItemIcon(item: item, size: 50)
            VStack(alignment: .leading, spacing: 3) {
                Text(item.title)
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundStyle(Console.text)
                    .lineLimit(1)
                HStack(spacing: 5) {
                    Circle().fill(item.status?.color ?? Console.textDim).frame(width: 6, height: 6)
                    Text([item.platformShort, caption].compactMap { $0 }.joined(separator: " · "))
                        .font(.system(size: 12))
                        .foregroundStyle(Console.textDim)
                        .lineLimit(1)
                }
            }
            Spacer(minLength: 8)
            Menu {
                LibraryItemActions(item: item, showsDetails: true, push: { router.library.append($0) }) { confirmRemove = true }
            } label: {
                Image(systemName: "ellipsis")
                    .font(.system(size: 16, weight: .bold))
                    .foregroundStyle(Console.textDim)
                    .frame(width: 36, height: 36)
                    .contentShape(Rectangle())
            }
            .accessibilityLabel("More for \(item.title)")
        }
        .padding(.vertical, 8)
        .contentShape(Rectangle())
        .removeItemDialog(item, isPresented: $confirmRemove)
    }
}

/// One item in the Library grid: a rounded square icon, its name with the status dot, and a caption when there is one.
private struct LibraryCell: View {
    let item: LibraryItem
    let caption: String?
    var small = false

    var body: some View {
        VStack(alignment: .leading, spacing: small ? 5 : 8) {
            Color.clear
                .aspectRatio(1, contentMode: .fit)
                .overlay { GeometryReader { geo in ItemIcon(item: item, size: geo.size.width) } }
                .overlay(alignment: .topTrailing) { if item.runsInAndroid, !small { AndroidBadge().padding(6) } }
                .snowCap(height: small ? 4 : 7)
                .shadow(color: .black.opacity(0.25), radius: small ? 3 : 6, y: small ? 2 : 3)
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: small ? 3 : 5) {
                    if let status = item.status { StatusDot(status: status) }
                    Text(item.title)
                        .font(.system(size: small ? 11 : 13, weight: .semibold))
                        .foregroundStyle(Console.text)
                        .lineLimit(1)
                }
                if let caption {
                    Text(caption).font(.system(size: small ? 9 : 11)).foregroundStyle(Console.textDim).lineLimit(1)
                }
            }
        }
        .contentShape(Rectangle())
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
                Group { if let item { ItemIcon(item: item, size: size) } else { AppIcon(path: iconPath, size: size) } }.snowCap(height: 7)
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

/// What can be done with a library item: play or open it, its page, its settings, and removing it. Shared by the long press
/// on a tile and Home's "⋯" button.
struct LibraryItemActions: View {
    let item: LibraryItem
    var showsDetails = false
    let push: (LibraryRoute) -> Void
    let askRemove: () -> Void

    var body: some View {
        Button { Launcher.play(item, push: push) } label: {
            Label(item.isGame ? "Play" : "Open", systemImage: item.isGame ? "play.fill" : "arrow.up.forward.app")
        }
        if showsDetails, !item.runsInAndroid {
            Button { push(item.route) } label: { Label("Details", systemImage: "info.circle") }
        }
        if case .game(let g) = item {
            Button { push(.gameSettings(g.id)) } label: { Label("Settings", systemImage: "slider.horizontal.3") }
            Divider()
            Button(role: .destructive) { askRemove() } label: { Label("Remove", systemImage: "trash") }
        } else if case .app(let p) = item, AndroidHost.shared.isReady {
            Divider()
            Button(role: .destructive) { askRemove() } label: { Label("Uninstall", systemImage: "trash") }
                .accessibilityHint("Removes \(p.label) from Android")
        }
    }
}

/// Asking before an item is removed, and removing it.
private struct RemoveItemDialog: ViewModifier {
    let item: LibraryItem
    @Binding var isPresented: Bool

    func body(content: Content) -> some View {
        content
            .confirmationDialog(removeTitle, isPresented: $isPresented, titleVisibility: .visible) {
                Button(removeVerb, role: .destructive) {
                    switch item {
                    case .game(let g): TranslationLayerStore.shared.remove(g)
                    case .app(let p): AndroidHost.shared.uninstall(p.name)
                    default: break
                    }
                }
            } message: {
                Text(removeMessage)
            }
    }

    private var removeVerb: String { if case .app = item { return "Uninstall" }; return "Remove" }
    private var removeTitle: String { "\(removeVerb) \(item.title)?" }
    private var removeMessage: String {
        if case .app = item { return "It is uninstalled from Android, with its data." }
        return "Its files and saved progress are deleted from Husk."
    }
}

/// Long-press on a library tile: play or open it, its settings, and removing it -- what iOS puts on a home screen icon.
private struct LibraryItemMenu: ViewModifier {
    let item: LibraryItem
    let push: (LibraryRoute) -> Void
    @State private var confirmRemove = false

    func body(content: Content) -> some View {
        content
            .contextMenu { LibraryItemActions(item: item, push: push) { confirmRemove = true } }
            .removeItemDialog(item, isPresented: $confirmRemove)
    }
}

extension View {
    func libraryItemMenu(_ item: LibraryItem, push: @escaping (LibraryRoute) -> Void) -> some View {
        modifier(LibraryItemMenu(item: item, push: push))
    }

    func removeItemDialog(_ item: LibraryItem, isPresented: Binding<Bool>) -> some View {
        modifier(RemoveItemDialog(item: item, isPresented: isPresented))
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
        huskDestination(for: LibraryRoute.self) { route in
            LibraryRouteView(route: route).huskNavigationBar(hidden: false)
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
        case .ps4(let id):
            if let game = PS4Store.shared.game(id) { PS4GamePage(game: game) } else { GoneView() }
        }
    }
}

private struct GoneView: View {
    var body: some View {
        Text("This was removed.").foregroundStyle(.secondary).frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
