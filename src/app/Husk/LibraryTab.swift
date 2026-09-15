// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// The apps installed in the guest, as something you look at rather than read.
///
/// The old library was a plain `List` of one-line rows, which is what you build
/// when the list is a debugging aid. It is the app's front door, so it is a grid
/// of cards: an icon large enough to recognise at a glance, the label under it,
/// and everything else out of the way until asked for.
///
/// It is also a launcher rather than a view onto the bridge. The catalogue is
/// written to disk the first time the guest reports its apps, so the grid is on
/// screen the instant Husk opens — a minute before Android can answer for
/// itself. Everything you can do without the guest (look, read, decide) works
/// straight away; the one thing that needs it, opening an app, is greyed out,
/// with the boot's progress shown above the grid rather than in place of it.
struct LibraryTab: View {
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var guest = GuestImage.shared

    let onOpenGuest: () -> Void
    let onStartAndroid: () -> Void
    let started: Bool

    @State private var importing = false
    @State private var sendingFiles = false
    @State private var detail: AndroidHost.Package?

    private let columns = [GridItem(.adaptive(minimum: 104), spacing: 16)]

    var body: some View {
        NavigationStack {
            ZStack {
                Theme.backdrop
                content
            }
            .navigationTitle("Library")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Menu {
                        Button { importing = true } label: {
                            Label("Install APK(s)", systemImage: "square.and.arrow.down")
                        }
                        Button { sendingFiles = true } label: {
                            Label("Send files to Android", systemImage: "doc.badge.plus")
                        }
                        Divider()
                        Button { onOpenGuest() } label: {
                            Label("Show Android", systemImage: "rectangle.inset.filled")
                        }
                        Button {
                            QemuRunner.shared.saveState(reason: "asked from the library")
                        } label: {
                            Label("Save Android state",
                                  systemImage: "externaldrive.badge.checkmark")
                        }
                        .disabled(runner.isSavingState)
                    } label: {
                        Image(systemName: "plus")
                    }
                    .disabled(!host.isReady || host.busy != nil)
                }
            }
            .fileImporter(isPresented: $importing, allowedContentTypes: [.item],
                          allowsMultipleSelection: true) { result in
                if case .success(let urls) = result, !urls.isEmpty {
                    HuskLog.log("ui", "importing \(urls.count) file(s): "
                              + urls.map(\.lastPathComponent).joined(separator: ", "))
                    host.install(urls)
                }
            }
            .fileImporter(isPresented: $sendingFiles, allowedContentTypes: [.item],
                          allowsMultipleSelection: true) { result in
                if case .success(let urls) = result {
                    HuskLog.log("ui", "sending \(urls.count) file(s) to Android")
                    host.sendFiles(urls)
                }
            }
            .sheet(item: $detail) { app in
                AppDetailSheet(app: app, onLaunch: {
                    host.launch(app.name) { onOpenGuest() }
                    detail = nil
                })
            }
        }
    }

    @ViewBuilder private var content: some View {
        ScrollView {
            VStack(spacing: 16) {
                if let busy = host.busy {
                    HStack(spacing: 10) {
                        ProgressView()
                        Text(busy).font(.footnote)
                        Spacer()
                    }
                    .padding(14)
                    .huskGlass()
                }

                // The grid is the page. The guest's state is a strip above it
                // rather than a screen instead of it: a cold boot runs to a
                // couple of minutes, and a launcher that shows nothing at all
                // for that long is a launcher you close.
                if !host.packages.isEmpty {
                    if !host.isReady { statusBanner }
                    LazyVGrid(columns: columns, spacing: 18) {
                        ForEach(host.packages) { pkg in
                            AppCard(app: pkg, dimmed: !host.isReady) { detail = pkg }
                        }
                    }
                    .padding(.top, 2)
                } else if !started {
                    EmptyState(title: "Android is not running",
                               message: "Start the guest and the apps you install "
                                      + "appear here, ready the moment Husk opens.",
                               systemImage: "power",
                               actionTitle: "Start Android",
                               action: onStartAndroid)
                        .huskGlass()
                } else if !host.isReady {
                    booting
                } else {
                    EmptyState(title: "No apps yet",
                               message: "Install an APK and it appears here. Split APK "
                                      + "sets work too — pick every piece at once.",
                               systemImage: "square.grid.2x2",
                               actionTitle: "Install APK(s)",
                               action: { importing = true })
                        .huskGlass()
                }
            }
            .padding(.horizontal, 18)
            .padding(.vertical, 12)
        }
    }

    /// The guest's state, in one line over the grid.
    ///
    /// Shown only while the apps on screen cannot actually be opened, so it
    /// takes itself away rather than becoming furniture.
    private var statusBanner: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle().fill(Theme.accentSoft).frame(width: 34, height: 34)
                if started {
                    ProgressView().scaleEffect(0.7).tint(Theme.accent)
                } else {
                    Image(systemName: "power")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(Theme.accent)
                }
            }

            VStack(alignment: .leading, spacing: 3) {
                Text(started ? host.status : "Android is not running")
                    .font(.footnote.weight(.medium))
                    .lineLimit(1)
                if started, runner.bootProgress > 0 {
                    ProgressView(value: Double(runner.bootProgress), total: 100)
                        .progressViewStyle(.linear).tint(Theme.accent)
                        .frame(height: 3)
                } else {
                    Text(subtitle)
                        .font(.caption2).foregroundStyle(.secondary)
                        .lineLimit(1)
                }
            }

            Spacer(minLength: 6)

            Button(buttonTitle) {
                if started { onOpenGuest() } else { onStartAndroid() }
            }
            .font(.footnote.weight(.medium))
            .tint(Theme.accent)
        }
        .padding(.horizontal, 14).padding(.vertical, 11)
        .huskGlass()
    }

    private var subtitle: String {
        if started { return "Apps open once it has finished starting." }
        // Without a debugger attached there is no executable memory and so no
        // guest, and "Start" would do nothing at all. Say which of the two is
        // missing rather than offering a button that quietly fails.
        return JITBootstrap.isDebuggerAttached
            ? "Your apps are here; starting the guest opens them."
            : "Enable JIT to start Android and open these."
    }

    private var buttonTitle: String {
        if started { return "Show" }
        return JITBootstrap.isDebuggerAttached ? "Start" : "Enable JIT"
    }

    /// Boot progress, with the bar rather than a bare spinner — a cold boot is
    /// long enough that "something is happening" is not enough information.
    ///
    /// Only reached on a first ever boot, when there is no catalogue yet and so
    /// nothing else to put on the screen.
    private var booting: some View {
        VStack(spacing: 12) {
            if runner.bootProgress > 0 {
                ProgressView(value: Double(runner.bootProgress), total: 100)
                    .progressViewStyle(.linear).tint(Theme.accent)
                    .frame(maxWidth: 260)
            } else {
                ProgressView().tint(Theme.accent)
            }
            Text(host.status).font(.callout.weight(.medium))
            Text(runner.setupMessage ?? "Android is starting in the background.")
                .font(.caption).foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
            Button("Show Android") { onOpenGuest() }
                .font(.footnote).tint(Theme.accent).padding(.top, 2)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 30)
        .huskGlass()
    }
}

/// One app, as a tile.
struct AppCard: View {
    let app: AndroidHost.Package
    /// Set while the guest cannot open anything, so a card that is still worth
    /// tapping for its details does not claim to be ready to run.
    var dimmed = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 9) {
                AppIcon(path: app.iconPath)
                    .frame(width: 62, height: 62)
                    .clipShape(RoundedRectangle(cornerRadius: 15, style: .continuous))
                    .opacity(dimmed ? 0.55 : 1)
                Text(app.label)
                    .font(.caption.weight(.medium))
                    .lineLimit(2).multilineTextAlignment(.center)
                    .frame(height: 30, alignment: .top)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14).padding(.horizontal, 6)
            .huskGlass()
        }
        .buttonStyle(.plain)
    }
}

/// What an app is, and the two things worth doing with it.
///
/// A sheet rather than a push: this is a detour from the grid, not a place in a
/// hierarchy, and it should close where it opened.
struct AppDetailSheet: View {
    let app: AndroidHost.Package
    let onLaunch: () -> Void

    @ObservedObject private var host = AndroidHost.shared
    @Environment(\.dismiss) private var dismiss
    @State private var confirmUninstall = false

    /// Anything that reaches the guest needs the guest, which on a cold launch
    /// is a minute or two away.
    private var canOpen: Bool { host.isReady && host.busy == nil }

    var body: some View {
        NavigationStack {
            ZStack {
                Theme.backdrop
                ScrollView {
                    VStack(spacing: 18) {
                        AppIcon(path: app.iconPath)
                            .frame(width: 96, height: 96)
                            .clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
                            .shadow(color: .black.opacity(0.18), radius: 14, y: 6)
                            .padding(.top, 14)

                        Text(app.label).font(.title2.weight(.semibold))
                            .multilineTextAlignment(.center)

                        Button(action: onLaunch) {
                            Label(host.isReady ? "Open" : "Starting Android…",
                                  systemImage: host.isReady ? "play.fill" : "hourglass")
                                .font(.headline).frame(maxWidth: .infinity)
                                .padding(.vertical, 13)
                        }
                        .buttonStyle(.plain)
                        .huskGlass(Capsule(), prominent: host.isReady)
                        .opacity(canOpen ? 1 : 0.45)
                        .disabled(!canOpen)

                        if !host.isReady {
                            Text("Android is still starting. Everything else about "
                               + "this app is here in the meantime.")
                                .font(.caption).foregroundStyle(.secondary)
                                .multilineTextAlignment(.center)
                                .padding(.horizontal, 14)
                        }

                        Card {
                            VStack(spacing: 10) {
                                DetailRow(label: "Package", value: app.name)
                            }
                        }

                        Button(role: .destructive) { confirmUninstall = true } label: {
                            Label("Uninstall", systemImage: "trash")
                                .font(.subheadline)
                        }
                        .disabled(!canOpen)
                        .padding(.top, 2)
                    }
                    .padding(.horizontal, 20).padding(.bottom, 28)
                }
            }
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Done") { dismiss() }
                }
            }
            .confirmationDialog("Uninstall \(app.label)?",
                                isPresented: $confirmUninstall, titleVisibility: .visible) {
                Button("Uninstall", role: .destructive) {
                    host.uninstall(app.name)
                    dismiss()
                }
                Button("Cancel", role: .cancel) { }
            } message: {
                Text("Its data goes with it. Save Android afterwards or the change "
                   + "is lost on the next launch.")
            }
        }
    }
}
