// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

// MARK: - what's new

/// What this version brought, shown once after updating to it (not on a first install: onboarding is the welcome then).
enum WhatsNew {
    struct Item: Identifiable {
        let symbol: String
        let title: String
        let detail: String
        var id: String { title }
    }

    /// The version whose notes these are. A build of the same version shows nothing again.
    static let version = "3.0.0"
    static let items: [Item] = [
        Item(symbol: "gamecontroller.fill", title: L10n.tr("A new Husk"),
             detail: L10n.tr("A console-style home: your games on a shelf, the one you pick fills the screen with its artwork, and Play is one tap away. Android and PS4 games side by side, a new library, in light or dark as Settings › Appearance says.")),
        Item(symbol: "playstation.logo", title: L10n.tr("PS4 games"),
             detail: L10n.tr("Add a PS4 game with the + button, as a fake package (.pkg), a folder of them, or an extracted game folder, and play it from your library. Updates install beside their game, shaders are kept between launches, and games that ask for a name bring up the keyboard.")),
        Item(symbol: "square.stack.3d.up.fill", title: L10n.tr("Hundreds more Android games"),
             detail: L10n.tr("Hundreds of newly working games on the compatibility list, unofficial ports like Sonic Robo Blast 2, Ring Racers, NetHack and PokeWilds, Flutter games that showed a blank screen draw again, Godot 4 games run, and Python games get online.")),
        Item(symbol: "arrow.down.circle.fill", title: L10n.tr("Complete Store downloads"),
             detail: L10n.tr("Games that keep their data in expansion files get them with the download, and games split into many pieces load.")),
        Item(symbol: "keyboard.fill", title: L10n.tr("Typing everywhere"),
             detail: L10n.tr("Text fields in Unity, Java, Flutter, cocos2d-x and SDL games bring up the keyboard.")),
        Item(symbol: "textformat", title: L10n.tr("Chinese, Japanese and Korean"),
             detail: L10n.tr("Download the fonts in Settings › Fonts, and games stop showing empty boxes.")),
        Item(symbol: "bolt.fill", title: L10n.tr("JIT on its own"),
             detail: L10n.tr("TrollStore installs turn JIT on at launch, and Husk says on screen when something can't start without it.")),
        Item(symbol: "iphone", title: "iOS 15",
             detail: L10n.tr("Husk now runs on iOS 15.5 and later (PS4 games need iOS 17.4).")),
        Item(symbol: "wrench.and.screwdriver.fill", title: L10n.tr("Fixes"),
             detail: L10n.tr("Android no longer crashes after its download, the full Android emulator moved into Settings with a warning, closing Husk from the app switcher isn't reported as a crash, and much more.")),
    ]

    private static let seenKey = "husk.whatsNew.seen"

    /// Whether to show the notes now: an update to this version, not a new install.
    static var due: Bool {
        let seen = UserDefaults.standard.string(forKey: seenKey)
        if seen == version { return false }
        if Onboarding.needed { markSeen(); return false }       // a first install: onboarding welcomes them
        return true
    }

    static func markSeen() { UserDefaults.standard.set(version, forKey: seenKey) }
}

struct WhatsNewSheet: View {
    var done: () -> Void

    var body: some View {
        VStack(spacing: 0) {
            ScrollView {
                VStack(alignment: .leading, spacing: 26) {
                    VStack(alignment: .leading, spacing: 6) {
                        Text(L10n.tr("What's New in Husk \(WhatsNew.version)")).font(.largeTitle.weight(.bold))
                        Text(L10n.tr("A new look, PS4 games, and hundreds more Android games.")).foregroundStyle(.secondary)
                    }
                    .padding(.top, 36)
                    ForEach(WhatsNew.items) { item in
                        HStack(alignment: .top, spacing: 16) {
                            Image(systemName: item.symbol)
                                .font(.system(size: 26))
                                .foregroundStyle(Color.accentColor)
                                .frame(width: 36)
                            VStack(alignment: .leading, spacing: 3) {
                                Text(item.title).font(.headline)
                                Text(item.detail).font(.subheadline).foregroundStyle(.secondary)
                                    .fixedSize(horizontal: false, vertical: true)
                            }
                        }
                    }
                }
                .padding(.horizontal, 28)
                .padding(.bottom, 20)
            }
            Button(action: done) {
                Text(L10n.tr("Continue")).font(.headline).frame(maxWidth: .infinity).padding(.vertical, 8)
            }
            .buttonStyle(.borderedProminent)
            .padding(.horizontal, 28)
            .padding(.vertical, 18)
        }
        .interactiveDismissDisabled()
    }
}

// MARK: - a newer Husk

/// A newer Husk on GitHub: the releases page is asked at launch (at most twice a day) and the person is told once per release.
@MainActor
final class AppUpdates: ObservableObject {
    static let shared = AppUpdates()

    struct Release: Equatable {
        let version: String
        let page: URL
    }

    @Published var available: Release?

    private static let api = URL(string: "https://api.github.com/repos/Leviidev/Husk/releases/latest")!
    private static let checkedKey = "husk.updates.checked", dismissedKey = "husk.updates.dismissed"

    static var current: String { Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "0" }

    func check() async {
        let d = UserDefaults.standard
        if Date().timeIntervalSince1970 - d.double(forKey: Self.checkedKey) < 12 * 3600 { return }
        var req = URLRequest(url: Self.api)
        req.timeoutInterval = 20
        req.setValue("Husk", forHTTPHeaderField: "User-Agent")
        guard let (data, response) = try? await URLSession.shared.data(for: req),
              (response as? HTTPURLResponse)?.statusCode == 200,
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let tag = obj["tag_name"] as? String,
              let page = (obj["html_url"] as? String).flatMap(URL.init(string:)) else { return }
        d.set(Date().timeIntervalSince1970, forKey: Self.checkedKey)
        let version = tag.hasPrefix("v") ? String(tag.dropFirst()) : tag
        guard Self.newer(version, than: Self.current), d.string(forKey: Self.dismissedKey) != version else { return }
        HuskLog.log("update", "Husk \(version) is out (this is \(Self.current))")
        available = Release(version: version, page: page)
    }

    func dismiss() {
        if let v = available?.version { UserDefaults.standard.set(v, forKey: Self.dismissedKey) }
        available = nil
    }

    /// "1.0.10" is newer than "1.0.9": compared number by number.
    static func newer(_ a: String, than b: String) -> Bool {
        let x = a.split(separator: ".").map { Int($0) ?? 0 }, y = b.split(separator: ".").map { Int($0) ?? 0 }
        for i in 0..<max(x.count, y.count) {
            let p = i < x.count ? x[i] : 0, q = i < y.count ? y[i] : 0
            if p != q { return p > q }
        }
        return false
    }
}
