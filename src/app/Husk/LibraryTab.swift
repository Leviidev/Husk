// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// The apps installed in the guest, as something you look at rather than read.
///
/// The old library was a plain `List` of one-line rows, which is what you build
/// when the list is a debugging aid. It is the app's front door, so it is a grid
/// of tiles: an icon large enough to recognise at a glance, the label under it,
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

    private let columns = [GridItem(.adaptive(minimum: 104), spacing: 14)]

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
            VStack(spacing: 14) {
                if let busy = host.busy {
                    HStack(spacing: 11) {
                        ProgressView().tint(Theme.accent)
                        Text(busy).font(.footnote).lineLimit(2)
                        Spacer(minLength: 0)
                    }
                    .padding(.horizontal, 14).padding(.vertical, 12)
                    .huskGlass()
                }

                // The grid is the page. The guest's state is a strip above it
                // rather than a screen instead of it: a cold boot runs to a
                // couple of minutes, and a launcher that shows nothing at all
                // for that long is a launcher you close.
                if !host.packages.isEmpty {
                    if !host.isReady { statusBanner }
                    LazyVGrid(columns: columns, spacing: 14) {
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
                               actionTitle: JITBootstrap.isDebuggerAttached
                                            ? "Start Android" : "Enable JIT",
                               action: onStartAndroid)
                } else if !host.isReady {
                    booting
                } else {
                    EmptyState(title: "No apps yet",
                               message: "Install an APK and it appears here. Split APK "
                                      + "sets work too — pick every piece at once.",
                               systemImage: "square.grid.2x2",
                               actionTitle: "Install APK(s)",
                               action: { importing = true })
                }
            }
            .padding(.horizontal, 18)
            .padding(.top, 4)
            .padding(.bottom, 24)
        }
    }

    /// The guest's state, in one line over the grid.
    ///
    /// Shown only while the apps on screen cannot actually be opened, so it
    /// takes itself away rather than becoming furniture. Glass here and nowhere
    /// else on the page: it is chrome floating over the grid, which is the one
    /// thing the effect is actually for.
    private var statusBanner: some View {
        HStack(spacing: 12) {
            ZStack {
                Circle().fill(Theme.accentSoft).frame(width: 32, height: 32)
                if started {
                    ProgressView().scaleEffect(0.65).tint(Theme.accent)
                } else {
                    Image(systemName: "power")
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(Theme.accent)
                }
            }

            VStack(alignment: .leading, spacing: 4) {
                Text(started ? host.status : "Android is not running")
                    .font(.footnote.weight(.semibold))
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

            Button {
                if started { onOpenGuest() } else { onStartAndroid() }
            } label: {
                Text(buttonTitle)
                    .font(.footnote.weight(.semibold))
                    .foregroundStyle(Theme.accent)
                    .padding(.horizontal, 12).padding(.vertical, 7)
                    .background(Theme.accentSoft, in: Capsule())
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 13).padding(.vertical, 11)
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
        VStack(spacing: 14) {
            ZStack {
                Circle().fill(Theme.accentSoft).frame(width: 66, height: 66)
                ProgressView().tint(Theme.accent)
            }
            Text(host.status).font(.title3.weight(.semibold))
            Text(runner.setupMessage ?? "Android is starting in the background.")
                .font(.subheadline).foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 26)
            if runner.bootProgress > 0 {
                ProgressView(value: Double(runner.bootProgress), total: 100)
                    .progressViewStyle(.linear).tint(Theme.accent)
                    .frame(maxWidth: 220)
            }
            Button("Show Android") { onOpenGuest() }
                .font(.subheadline.weight(.medium))
                .foregroundStyle(Theme.accent)
                .padding(.top, 2)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 38)
    }
}

/// One app, as a tile.
struct AppCard: View {
    let app: AndroidHost.Package
    /// Set while the guest cannot open anything, so a tile that is still worth
    /// tapping for its details does not claim to be ready to run.
    var dimmed = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 10) {
                AppIcon(path: app.iconPath, size: 60)
                    .shadow(color: .black.opacity(0.14), radius: 5, y: 2)
                    .opacity(dimmed ? 0.5 : 1)
                Text(app.label)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.primary)
                    .lineLimit(2).multilineTextAlignment(.center)
                    .frame(height: 30, alignment: .top)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 15).padding(.horizontal, 6)
            // Fill and hairline without a card shadow: twenty shadows in a grid
            // is twenty offscreen passes, and the guest needs the GPU more than
            // this does.
            .background(Theme.surface,
                        in: RoundedRectangle(cornerRadius: Theme.cardCorner,
                                             style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: Theme.cardCorner, style: .continuous)
                        .stroke(Theme.hairline, lineWidth: 0.5))
        }
        .buttonStyle(CardButtonStyle())
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
                    VStack(spacing: 16) {
                        AppIcon(path: app.iconPath, size: 96)
                            .shadow(color: .black.opacity(0.22), radius: 16, y: 7)
                            .padding(.top, 8)

                        VStack(spacing: 6) {
                            Text(app.label)
                                .font(.title2.weight(.bold))
                                .multilineTextAlignment(.center)
                            Text(app.name)
                                .font(.caption).foregroundStyle(.secondary)
                                .lineLimit(1).truncationMode(.middle)
                        }

                        if host.isReady {
                            StatusPill(text: "Ready", systemImage: "checkmark.circle.fill")
                        } else {
                            StatusPill(text: "Android is starting",
                                       systemImage: "hourglass", tint: .orange)
                        }

                        Button(action: onLaunch) {
                            Label(host.isReady ? "Open" : "Starting Android…",
                                  systemImage: host.isReady ? "play.fill" : "hourglass")
                        }
                        .buttonStyle(PrimaryButtonStyle(enabled: canOpen))
                        .disabled(!canOpen)
                        .padding(.top, 2)

                        if !host.isReady {
                            Text("Everything else about this app is here in the "
                               + "meantime; it opens as soon as Android answers.")
                                .font(.caption).foregroundStyle(.secondary)
                                .multilineTextAlignment(.center)
                                .padding(.horizontal, 20)
                        }

                        Button(role: .destructive) { confirmUninstall = true } label: {
                            Label("Uninstall", systemImage: "trash")
                                .font(.subheadline.weight(.medium))
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 12)
                        }
                        .buttonStyle(.plain)
                        .foregroundStyle(canOpen ? Color.red : Color.secondary)
                        .huskCard()
                        .disabled(!canOpen)
                        .padding(.top, 6)
                    }
                    .padding(.horizontal, 22).padding(.bottom, 30)
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
        .presentationDragIndicator(.visible)
    }
}
