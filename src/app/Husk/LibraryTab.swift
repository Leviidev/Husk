// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// The apps installed in the guest, as something you look at rather than read.
///
/// Two things carry the screen. At the top, one card that says what the machine
/// is doing — running, starting, or stopped — because that is the fact every
/// other decision on this screen depends on, and it deserves to be the first
/// thing the eye lands on rather than a line of grey text somewhere. Under it,
/// the apps as icons on the page: no tile, no border, no card. An app's own
/// artwork is the most designed thing Husk will ever show, and every container
/// put around it is something competing with it.
///
/// It is a launcher rather than a view onto the bridge. The catalogue is written
/// to disk the first time the guest reports its apps, so the grid is on screen
/// the instant Husk opens — a minute before Android can answer for itself.
/// Everything you can do without the guest (look, read, decide) works straight
/// away; the one thing that needs it, opening an app, is greyed out, with the
/// boot's progress in the card above rather than in place of the grid.
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
    /// Ticks once a second, only so the uptime on the hero card counts up.
    @State private var now = Date()

    private let columns = [GridItem(.adaptive(minimum: 84), spacing: 14)]
    private let clock = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

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
        .onReceive(clock) { now = $0 }
    }

    @ViewBuilder private var content: some View {
        ScrollView {
            VStack(spacing: 20) {
                MachineCard(started: started, now: now,
                            onOpenGuest: onOpenGuest, onStart: onStartAndroid)

                if let busy = host.busy {
                    HStack(spacing: 11) {
                        ProgressView().tint(Theme.accent)
                        Text(busy).font(.footnote).lineLimit(2)
                        Spacer(minLength: 0)
                    }
                    .padding(.horizontal, 14).padding(.vertical, 12)
                    .huskGlass()
                }

                if !host.packages.isEmpty {
                    VStack(spacing: 14) {
                        SectionHeader(title: "Your apps",
                                      trailing: host.isReady
                                                ? "\(host.packages.count)"
                                                : "opens when ready")
                        LazyVGrid(columns: columns, spacing: 20) {
                            ForEach(host.packages) { pkg in
                                AppCard(app: pkg, dimmed: !host.isReady) { detail = pkg }
                            }
                        }
                    }
                } else if started && host.isReady {
                    EmptyState(title: "No apps yet",
                               message: "Install an APK and it appears here. Split APK "
                                      + "sets work too — pick every piece at once.",
                               systemImage: "square.grid.2x2",
                               actionTitle: "Install APK(s)",
                               action: { importing = true })
                }
            }
            .padding(.horizontal, 20)
            .padding(.top, 4)
            .padding(.bottom, 28)
        }
    }
}

/// A small label over a group, with one fact on the right.
struct SectionHeader: View {
    let title: String
    var trailing: String? = nil

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title)
                .font(.subheadline.weight(.semibold))
            Spacer()
            if let trailing {
                Text(trailing)
                    .font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}

/// What the machine is doing, as the first thing on the screen.
///
/// Three states, one shape. Running is the only one that gets the accent fill:
/// it is the state where there is something to do, and the fill is what makes
/// "Open Android" the obvious thing on the page. Starting and stopped are
/// quieter cards, because in both of them the honest answer is "wait" or
/// "press start" and neither wants to shout.
struct MachineCard: View {
    let started: Bool
    let now: Date
    let onOpenGuest: () -> Void
    let onStart: () -> Void

    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var runner = QemuRunner.shared
    @State private var pulse = false

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            if host.isReady {
                running
            } else if started {
                booting
            } else {
                stopped
            }
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(host.isReady ? AnyShapeStyle(Theme.accentSoft)
                                 : AnyShapeStyle(Theme.surface),
                    in: RoundedRectangle(cornerRadius: 20, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .stroke(host.isReady ? Theme.accent.opacity(0.22) : Theme.hairline,
                            lineWidth: 0.5))
        .shadow(color: .black.opacity(0.05), radius: 10, y: 3)
        .animation(.snappy(duration: 0.3), value: host.isReady)
    }

    // MARK: states

    private var running: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 8) {
                // A live dot rather than the word "connected". It breathes, so
                // the card reads as something happening now.
                Circle().fill(Color.green)
                    .frame(width: 7, height: 7)
                    .opacity(pulse ? 0.35 : 1)
                    .animation(.easeInOut(duration: 1.1).repeatForever(autoreverses: true),
                               value: pulse)
                    .onAppear { pulse = true }
                Text("Android is running")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(Theme.accent)
                Spacer()
            }

            HStack(spacing: 22) {
                stat(value: runner.fps > 0 ? String(format: "%.0f", runner.fps) : "—",
                     unit: "fps")
                stat(value: "\(host.packages.count)", unit: host.packages.count == 1
                                                            ? "app" : "apps")
                stat(value: uptime, unit: "up")
            }

            Button(action: onOpenGuest) {
                Label("Open Android", systemImage: "rectangle.inset.filled")
            }
            .buttonStyle(PrimaryButtonStyle())
        }
    }

    private var booting: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 12) {
                BootRing(progress: runner.bootProgress)
                VStack(alignment: .leading, spacing: 2) {
                    Text("Starting Android")
                        .font(.subheadline.weight(.semibold))
                    Text(runner.setupMessage ?? host.status)
                        .font(.caption).foregroundStyle(.secondary)
                        .lineLimit(1)
                }
                Spacer(minLength: 4)
                if runner.bootProgress > 0 {
                    Text("\(runner.bootProgress)%")
                        .font(.technical(20, weight: .medium))
                        .foregroundStyle(Theme.accent)
                }
            }

            ProgressView(value: Double(max(runner.bootProgress, 2)), total: 100)
                .progressViewStyle(.linear).tint(Theme.accent)

            Button("Show Android while it starts", action: onOpenGuest)
                .font(.footnote.weight(.medium))
                .foregroundStyle(Theme.accent)
        }
    }

    private var stopped: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                Image(systemName: "power")
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(Theme.accent)
                    .frame(width: 34, height: 34)
                    .background(Theme.accentSoft, in: Circle())
                VStack(alignment: .leading, spacing: 2) {
                    Text("Android is not running")
                        .font(.subheadline.weight(.semibold))
                    Text(JITBootstrap.isDebuggerAttached
                         ? "Your apps are here; start the guest to open them."
                         : "Husk needs JIT, which only a debugger can grant.")
                        .font(.caption).foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 0)
            }

            Button(action: onStart) {
                Label(JITBootstrap.isDebuggerAttached ? "Start Android" : "Enable JIT",
                      systemImage: JITBootstrap.isDebuggerAttached ? "play.fill" : "bolt.fill")
            }
            .buttonStyle(PrimaryButtonStyle())
        }
    }

    // MARK: pieces

    private func stat(value: String, unit: String) -> some View {
        VStack(alignment: .leading, spacing: 1) {
            Text(value)
                .font(.technical(21, weight: .medium))
                .foregroundStyle(Theme.accent)
            Text(unit)
                .font(.caption2)
                .foregroundStyle(Theme.accent.opacity(0.75))
        }
    }

    /// Minutes until an hour, then hours and minutes. Seconds on a VM uptime
    /// are noise, but the first minute is when someone is watching hardest.
    private var uptime: String {
        guard let from = runner.startedAt else { return "—" }
        let s = Int(now.timeIntervalSince(from))
        if s < 60 { return "\(max(s, 0))s" }
        if s < 3600 { return "\(s / 60)m" }
        return "\(s / 3600)h\((s % 3600) / 60)"
    }
}

/// The boot, as a ring being filled.
///
/// A ring rather than a spinner: a spinner says only that something is looping,
/// and a cold boot is long enough that the difference between 8% and 80% is the
/// only thing anyone actually wants to know.
struct BootRing: View {
    let progress: Int

    var body: some View {
        ZStack {
            Circle().stroke(Theme.accentSoft, lineWidth: 3)
            Circle()
                .trim(from: 0, to: max(CGFloat(progress) / 100, 0.04))
                .stroke(Theme.accent, style: StrokeStyle(lineWidth: 3, lineCap: .round))
                .rotationEffect(.degrees(-90))
                .animation(.snappy, value: progress)
        }
        .frame(width: 34, height: 34)
    }
}

/// One app: its own icon, its name, and nothing else.
struct AppCard: View {
    let app: AndroidHost.Package
    /// Set while the guest cannot open anything, so a tile that is still worth
    /// tapping for its details does not claim to be ready to run.
    var dimmed = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 8) {
                AppIcon(path: app.iconPath, size: 66)
                    .shadow(color: .black.opacity(0.16), radius: 6, y: 3)
                Text(app.label)
                    .font(.caption2.weight(.medium))
                    .foregroundStyle(.primary)
                    .lineLimit(2).multilineTextAlignment(.center)
                    .frame(height: 28, alignment: .top)
            }
            .frame(maxWidth: .infinity)
            .opacity(dimmed ? 0.45 : 1)
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
                        AppIcon(path: app.iconPath, size: 104)
                            .shadow(color: .black.opacity(0.22), radius: 18, y: 8)
                            .padding(.top, 10)

                        VStack(spacing: 7) {
                            Text(app.label)
                                .font(.title2.weight(.bold))
                                .multilineTextAlignment(.center)
                            Text(app.name)
                                .font(.technical(11))
                                .foregroundStyle(.secondary)
                                .lineLimit(1).truncationMode(.middle)
                                .padding(.horizontal, 10).padding(.vertical, 4)
                                .background(Color.primary.opacity(0.05), in: Capsule())
                        }

                        Button(action: onLaunch) {
                            Label(host.isReady ? "Open" : "Starting Android…",
                                  systemImage: host.isReady ? "play.fill" : "hourglass")
                        }
                        .buttonStyle(PrimaryButtonStyle(enabled: canOpen))
                        .disabled(!canOpen)
                        .padding(.top, 4)

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
                                .padding(.vertical, 13)
                        }
                        .buttonStyle(.plain)
                        .foregroundStyle(canOpen ? Color.red : Color.secondary)
                        .huskCard()
                        .disabled(!canOpen)
                        .padding(.top, 8)
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
