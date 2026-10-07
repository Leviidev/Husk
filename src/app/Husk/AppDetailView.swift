// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Where the tabs and their stacks are steered from.
///
/// One app can send you to another tab — an app's page offers to show its files
/// — and a tab that is also a navigation stack cannot be pushed from outside
/// itself without somewhere to keep the path. This is that somewhere.
@MainActor final class Router: ObservableObject {
    static let shared = Router()

    @Published var tab: HuskTab = .library
    /// The app pages pushed on top of the library.
    @Published var library: [AndroidHost.Package] = []
    /// Directories pushed on top of the Files root.
    @Published var files: [String] = []

    /// Show a directory in the Files tab, from anywhere.
    func openFiles(at path: String) {
        files = path == FilesTab.root ? [] : [path]
        tab = .files
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
                        Label(localizedKey(canOpen ? "Launch" : "Starting Android…"),
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
                LabeledContent("Last Used",
                               value: live.lastUsed.map(Self.when)
                                      ?? localizedString("Never from Husk"))
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
            Text(localizedKey("Its data goes with it. Save Android afterwards or the change is "
               + "lost on the next launch."))
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
                    if let c = live.category { Tag(text: localizedKey(c)) }
                    if let b = live.bitness { Tag(text: localizedKey(b)) }
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

    /// "Today, 14:03" for the last two days and a plain date before that.
    ///
    /// The time itself is formatted, not spelled out, so it follows the phone's
    /// own 12- or 24-hour setting; only the word in front of it is a key. A date
    /// format of `'Today,' h:mm a` would have frozen both the wording and the
    /// clock into English.
    static func when(_ date: Date) -> String {
        let time = date.formatted(date: .omitted, time: .shortened)
        if Calendar.current.isDateInToday(date) {
            return String(localized: "Today, \(time)")
        }
        if Calendar.current.isDateInYesterday(date) {
            return String(localized: "Yesterday, \(time)")
        }
        return date.formatted(date: .abbreviated, time: .omitted)
    }
}
