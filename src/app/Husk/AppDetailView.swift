// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// A page pushed onto Home or the Library: a game's page, its settings or its technical details, an Android app's page,
/// or Android's own.
enum LibraryRoute: Hashable {
    case android(AndroidHost.Package)
    case game(String)
    case gameSettings(String)
    case gameReport(String)
    case androidSystem
}

/// Where the tabs and their stacks are steered from.
///
/// One page can send you somewhere else -- an app's page offers its files, Home's "See All" opens the Library -- and a stack
/// cannot be pushed from outside itself without somewhere to keep the path. This is that somewhere. It also carries the two
/// things only the root view can do: show Android's screen, and start Android.
@MainActor final class Router: ObservableObject {
    static let shared = Router()

    @Published var tab: HuskTab = .home
    @Published var home: [LibraryRoute] = []
    @Published var library: [LibraryRoute] = []
    @Published var libraryFilter: LibraryFilter = .all
    /// Downloads, as a sheet: Home's Downloads tile opens it.
    @Published var showDownloads = false
    /// Android's storage, as a sheet over whatever is showing.
    @Published var showFiles = false
    /// Directories pushed on top of the Files root.
    @Published var files: [String] = []
    /// Android has been started in this run of Husk.
    @Published var androidStarted = false
    /// An Android app to open as soon as Android is up: asked for from its page while Android was off.
    @Published var pendingAndroidLaunch: String?

    /// A game to start as soon as its page is up: the one Husk was closed to switch to (see `switchTo`).
    @Published var autoPlay: String?

    private static let switchKey = "husk.switchToGame"

    /// Only one game can be loaded per run of Husk. Closing Husk to play another remembers which, and the next launch opens
    /// its page and starts it, so switching is: close, open, playing.
    func switchTo(_ appID: String) { UserDefaults.standard.set(appID, forKey: Self.switchKey) }

    /// At launch: the game Husk was closed to switch to, opened and started.
    func resumeSwitch() {
        guard let id = UserDefaults.standard.string(forKey: Self.switchKey) else { return }
        UserDefaults.standard.removeObject(forKey: Self.switchKey)
        HuskLog.log("ui", "opening \(id), which Husk was closed to switch to")
        tab = .library
        library = [.game(id)]
        autoPlay = id
    }

    /// Show Android's own screen. Set by the root view.
    var openGuest: () -> Void = {}
    /// Start Android, or turn JIT on first when it is off. Set by the root view.
    var startAndroid: () -> Void = {}

    /// Show a directory of Android's storage, from anywhere.
    func openFiles(at path: String) {
        files = path == FilesTab.root ? [] : [path]
        showFiles = true
    }

    func showLibrary(_ filter: LibraryFilter) {
        libraryFilter = filter
        tab = .library
    }

    /// Pick APKs or bundles; Husk then asks where they go.
    func addSomething() {
        HuskFilePicker.present { urls in IncomingFiles.shared.receive(urls) }
    }
}

/// One app: what it is, and the things worth doing with it.
struct AppDetailView: View {
    let app: AndroidHost.Package
    let onOpenGuest: () -> Void

    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var router = Router.shared
    @Environment(\.dismiss) private var dismiss
    @State private var confirmUninstall = false

    private var live: AndroidHost.Package {
        host.packages.first { $0.name == app.name } ?? app
    }
    private var canOpen: Bool { host.isReady && host.busy == nil }

    var body: some View {
        List {
            Section {
                header
                Button {
                    host.launch(app.name) { onOpenGuest() }
                } label: {
                    HStack {
                        Spacer()
                        Label(canOpen ? "Launch" : "Starting Android…",
                              systemImage: canOpen ? "play.fill" : "hourglass")
                            .font(.headline)
                        Spacer()
                    }
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .disabled(!canOpen)
                .listRowInsets(EdgeInsets(top: 8, leading: 16, bottom: 8, trailing: 16))
            } footer: {
                if !host.isReady { Text("It opens as soon as Android answers.") }
            }

            Section {
                LabeledContent("Version", value: live.version ?? "—")
                LabeledContent("Size", value: live.sizeBytes.map(Self.bytes) ?? "—")
                LabeledContent("Last Used", value: live.lastUsed.map(Self.when) ?? "Never from Husk")
            }

            Section {
                Button { router.openFiles(at: "/sdcard/Android/data/\(app.name)") } label: {
                    Label("Open in Files", systemImage: "folder")
                }
                Button { appInfo() } label: {
                    Label("App Info", systemImage: "info.circle")
                }
                .disabled(!canOpen)
                Button(role: .destructive) { confirmUninstall = true } label: {
                    Label("Uninstall", systemImage: "trash")
                }
                .disabled(!canOpen)
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle(live.label)
        .navigationBarTitleDisplayMode(.inline)
        .toolbar { ToolbarItem(placement: .topBarTrailing) { menu } }
        .confirmationDialog("Uninstall \(live.label)?", isPresented: $confirmUninstall,
                            titleVisibility: .visible) {
            Button("Uninstall", role: .destructive) {
                host.uninstall(app.name)
                dismiss()
            }
            Button("Cancel", role: .cancel) { }
        } message: {
            Text("Its data goes with it. Save Android afterwards or the change is "
               + "lost on the next launch.")
        }
    }

    // MARK: pieces

    private var menu: some View {
        Menu {
            Button {
                UIPasteboard.general.string = app.name
            } label: { Label("Copy Package Name", systemImage: "doc.on.doc") }
            Button { appInfo() } label: {
                Label("Show in Android Settings", systemImage: "gearshape")
            }
            .disabled(!canOpen)
            Divider()
            Button(role: .destructive) { confirmUninstall = true } label: {
                Label("Uninstall", systemImage: "trash")
            }
            .disabled(!canOpen)
        } label: {
            Image(systemName: "ellipsis.circle")
        }
    }

    private var header: some View {
        HStack(alignment: .top, spacing: 16) {
            AppIcon(path: live.iconPath, size: 76)
            VStack(alignment: .leading, spacing: 6) {
                Text(live.label)
                    .font(.title2.weight(.bold))
                    .lineLimit(2)
                Text(live.name)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .lineLimit(1).truncationMode(.middle)
                HStack(spacing: 6) {
                    if let c = live.category { Tag(text: c) }
                    if let b = live.bitness { Tag(text: b) }
                }
                .padding(.top, 2)
            }
            Spacer(minLength: 0)
        }
        .padding(.vertical, 4)
    }

    /// Android's own page for the app — permissions, storage, force stop. It
    /// is a screen Android already has and Husk should not be reimplementing.
    private func appInfo() {
        let pkg = app.name
        DispatchQueue.global(qos: .userInitiated).async {
            _ = try? GuestBridge.shared.shell(
                "am start -a android.settings.APPLICATION_DETAILS_SETTINGS "
              + "-d package:\(pkg)", timeout: 30)
        }
        onOpenGuest()
    }

    // MARK: formatting

    static func bytes(_ n: Int64) -> String {
        ByteCountFormatter.string(fromByteCount: n, countStyle: .file)
    }

    static func when(_ date: Date) -> String {
        let f = DateFormatter()
        if Calendar.current.isDateInToday(date) {
            f.dateFormat = "'Today,' h:mm a"
        } else if Calendar.current.isDateInYesterday(date) {
            f.dateFormat = "'Yesterday,' h:mm a"
        } else {
            f.dateStyle = .medium
            f.timeStyle = .none
        }
        return f.string(from: date)
    }
}
