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
        ZStack {
            Theme.backdrop
            ScrollView {
                VStack(spacing: 18) {
                    HuskHeader(back: { dismiss() }) { menu }
                    header
                    launch
                    facts
                    actions
                }
                .padding(.horizontal, 18)
                .padding(.top, 4)
                .padding(.bottom, 30)
            }
        }
        .navigationBarHidden(true)
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
            } label: { Label("Copy package name", systemImage: "doc.on.doc") }
            Button { appInfo() } label: {
                Label("Show in Android settings", systemImage: "gearshape")
            }
            .disabled(!canOpen)
            Divider()
            Button(role: .destructive) { confirmUninstall = true } label: {
                Label("Uninstall", systemImage: "trash")
            }
            .disabled(!canOpen)
        } label: {
            Image(systemName: "ellipsis")
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(Theme.text)
                .frame(width: 36, height: 36)
                .background(Theme.surfaceHigh, in: Circle())
        }
    }

    private var header: some View {
        HStack(alignment: .top, spacing: 16) {
            AppIcon(path: live.iconPath, size: 72)
            VStack(alignment: .leading, spacing: 6) {
                Text(live.label)
                    .font(.system(size: 26, weight: .bold))
                    .foregroundStyle(Theme.text)
                    .lineLimit(2)
                Text(live.name)
                    .font(.system(size: 13))
                    .foregroundStyle(Theme.textDim)
                    .lineLimit(1).truncationMode(.middle)
                HStack(spacing: 6) {
                    if let c = live.category { Tag(text: c) }
                    if let b = live.bitness { Tag(text: b) }
                }
                .padding(.top, 2)
            }
            Spacer(minLength: 0)
        }
    }

    private var launch: some View {
        VStack(spacing: 8) {
            Button {
                host.launch(app.name) { onOpenGuest() }
            } label: {
                Label(canOpen ? "Launch" : "Starting Android…",
                      systemImage: canOpen ? "play.fill" : "hourglass")
            }
            .buttonStyle(PrimaryButtonStyle(enabled: canOpen))
            .disabled(!canOpen)

            if !host.isReady {
                Text("It opens as soon as Android answers.")
                    .font(.system(size: 12))
                    .foregroundStyle(Theme.textDim)
            }
        }
    }

    private var facts: some View {
        RowGroup {
            fact("Version", live.version ?? "—")
            RowDivider().padding(.leading, 14)
            fact("Size", live.sizeBytes.map(Self.bytes) ?? "—")
            RowDivider().padding(.leading, 14)
            fact("Last used", live.lastUsed.map(Self.when) ?? "Never from Husk")
        }
    }

    private func fact(_ label: String, _ value: String) -> some View {
        HStack {
            Text(label)
                .font(.system(size: 15))
                .foregroundStyle(Theme.textDim)
            Spacer(minLength: 12)
            Text(value)
                .font(.system(size: 15, weight: .medium))
                .foregroundStyle(Theme.text)
        }
        .padding(.horizontal, 14).padding(.vertical, 13)
    }

    private var actions: some View {
        RowGroup {
            Button { router.openFiles(at: "/sdcard/Android/data/\(app.name)") } label: {
                HuskRow(systemImage: "folder", title: "Open in Files")
            }
            .buttonStyle(.plain)
            RowDivider()
            Button { appInfo() } label: {
                HuskRow(systemImage: "info.circle", title: "App info")
            }
            .buttonStyle(.plain)
            .disabled(!canOpen)
            RowDivider()
            Button { confirmUninstall = true } label: {
                HuskRow(systemImage: "trash", title: "Uninstall", tint: .red,
                        showsChevron: false)
            }
            .buttonStyle(.plain)
            .disabled(!canOpen)
        }
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
