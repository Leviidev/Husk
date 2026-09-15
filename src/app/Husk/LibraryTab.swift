// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// The apps installed in the guest, as something you look at rather than read.
///
/// The old library was a plain `List` of one-line rows, which is what you build
/// when the list is a debugging aid. It is the app's front door, so it is a grid
/// of cards: an icon large enough to recognise at a glance, the label under it,
/// and everything else out of the way until asked for.
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

                if !started {
                    EmptyState(title: "Android is not running",
                               message: "The library talks to Android over the bridge, "
                                      + "so it needs the guest up first.",
                               systemImage: "power",
                               actionTitle: "Start Android",
                               action: onStartAndroid)
                        .huskGlass()
                } else if !host.isReady {
                    booting
                } else if host.packages.isEmpty {
                    EmptyState(title: "No apps yet",
                               message: "Install an APK and it appears here. Split APK "
                                      + "sets work too — pick every piece at once.",
                               systemImage: "square.grid.2x2",
                               actionTitle: "Install APK(s)",
                               action: { importing = true })
                        .huskGlass()
                } else {
                    LazyVGrid(columns: columns, spacing: 18) {
                        ForEach(host.packages) { pkg in
                            AppCard(app: pkg) { detail = pkg }
                        }
                    }
                    .padding(.top, 2)
                }
            }
            .padding(.horizontal, 18)
            .padding(.vertical, 12)
        }
    }

    /// Boot progress, with the bar rather than a bare spinner — a cold boot is
    /// long enough that "something is happening" is not enough information.
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
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            VStack(spacing: 9) {
                AppIcon(path: app.iconPath)
                    .frame(width: 62, height: 62)
                    .clipShape(RoundedRectangle(cornerRadius: 15, style: .continuous))
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
                            Label("Open", systemImage: "play.fill")
                                .font(.headline).frame(maxWidth: .infinity)
                                .padding(.vertical, 13)
                        }
                        .buttonStyle(.plain)
                        .huskGlass(Capsule(), prominent: true)
                        .disabled(host.busy != nil)

                        Card {
                            VStack(spacing: 10) {
                                DetailRow(label: "Package", value: app.name)
                            }
                        }

                        Button(role: .destructive) { confirmUninstall = true } label: {
                            Label("Uninstall", systemImage: "trash")
                                .font(.subheadline)
                        }
                        .disabled(host.busy != nil)
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
