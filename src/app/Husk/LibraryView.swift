// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UniformTypeIdentifiers

/// The product screen: a grid of installed Android apps, each with its real icon
/// and name, that launch full-screen with no Android chrome.
///
/// The guest is running the whole time this is on screen -- it is simply not being
/// displayed. Tapping an app sends a launch command over the 9p bridge and swaps
/// to the guest surface.
struct LibraryView: View {
    @ObservedObject var bridge = HuskBridgeFS.shared
    @Binding var running: HuskBridgeFS.AndroidApp?
    @State private var importing = false
    @State private var showLogs = false

    private let columns = [GridItem(.adaptive(minimum: 92, maximum: 120), spacing: 24)]

    var body: some View {
        NavigationStack {
            Group {
                if bridge.apps.isEmpty {
                    empty
                } else {
                    grid
                }
            }
            .navigationTitle("Husk")
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    Button { importing = true } label: { Image(systemName: "plus") }
                        .accessibilityLabel("Add APK")
                }
                ToolbarItem(placement: .topBarLeading) {
                    Button { showLogs = true } label: {
                        Image(systemName: "doc.text.magnifyingglass")
                    }
                    .accessibilityLabel("View logs")
                }
            }
        }
        .fileImporter(isPresented: $importing,
                      allowedContentTypes: [Self.apkType, .item],
                      allowsMultipleSelection: true) { result in
            switch result {
            case .success(let urls):
                for url in urls { bridge.install(apkAt: url) }
            case .failure(let error):
                HuskLog.log("ui", "APK import cancelled/failed: \(error.localizedDescription)")
            }
        }
        .sheet(isPresented: $showLogs) { LogView() }
    }

    /// APKs are ZIPs, and iOS has no built-in type for them, so declare one.
    /// `.item` is allowed alongside it because some providers hand back a generic
    /// type for files they do not recognise, and refusing those would make perfectly
    /// good APKs unpickable.
    static let apkType = UTType(filenameExtension: "apk") ?? .data

    private var grid: some View {
        ScrollView {
            LazyVGrid(columns: columns, spacing: 26) {
                ForEach(bridge.apps) { app in
                    Button {
                        HuskLog.log("ui", "launching \(app.package)")
                        bridge.launch(package: app.package)
                        running = app
                    } label: {
                        VStack(spacing: 8) {
                            icon(for: app)
                            Text(app.name)
                                .font(.caption)
                                .lineLimit(2)
                                .multilineTextAlignment(.center)
                                .foregroundStyle(.primary)
                        }
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(20)

            if !bridge.pendingInstalls.isEmpty {
                VStack(spacing: 6) {
                    ForEach(Array(bridge.pendingInstalls), id: \.self) { name in
                        HStack(spacing: 8) {
                            ProgressView().controlSize(.small)
                            Text("Installing \(name)…").font(.caption)
                        }
                    }
                }
                .padding(.bottom, 20)
            }
        }
    }

    @ViewBuilder
    private func icon(for app: HuskBridgeFS.AndroidApp) -> some View {
        ZStack {
            if let path = app.iconPath, let ui = UIImage(contentsOfFile: path) {
                Image(uiImage: ui).resizable().scaledToFit()
            } else {
                // The catalogue can list an app before its icon has been copied
                // across, so fall back to its initial rather than a blank tile.
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .fill(.tertiary)
                    .overlay(
                        Text(String(app.name.prefix(1)).uppercased())
                            .font(.title.weight(.medium))
                            .foregroundStyle(.secondary))
            }
        }
        .frame(width: 66, height: 66)
        .clipShape(RoundedRectangle(cornerRadius: 16, style: .continuous))
        .shadow(color: .black.opacity(0.18), radius: 5, y: 2)
    }

    private var empty: some View {
        VStack(spacing: 16) {
            Image(systemName: "square.grid.2x2")
                .font(.system(size: 46)).foregroundStyle(.tertiary)
            Text("No apps yet").font(.headline)
            Text("Add an APK and it will be installed into the Android runtime, then appear here with its own icon.")
                .font(.callout).foregroundStyle(.secondary)
                .multilineTextAlignment(.center).padding(.horizontal, 44)
            Button { importing = true } label: {
                Label("Add APK", systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)
            if let msg = bridge.lastAgentMessage {
                Text(msg)
                    .font(.caption2).foregroundStyle(.orange)
                    .multilineTextAlignment(.center).padding(.horizontal, 30)
            }
        }
    }
}

/// An app running full-screen. Nothing on top of it but a way back.
struct RunningAppView: View {
    let app: HuskBridgeFS.AndroidApp
    let onExit: () -> Void
    @State private var showChrome = false
    @State private var keyboard = false

    var body: some View {
        ZStack(alignment: .topLeading) {
            Color.black.ignoresSafeArea()
            HuskDisplay().ignoresSafeArea()
            KeyCapture(active: $keyboard).frame(width: 0, height: 0)

            if keyboard {
                VStack {
                    Spacer()
                    SpecialKeysBar()
                        .padding(.horizontal, 10).padding(.vertical, 8)
                        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
                        .padding(.bottom, 6)
                }
            }

            // Chrome stays hidden: the point is that this looks like a native app.
            // A tap near the top-left corner reveals a way out, the way a
            // full-screen video player does.
            if showChrome {
                HStack(spacing: 10) {
                    Button(action: onExit) {
                        Label(app.name, systemImage: "chevron.left")
                            .font(.footnote.weight(.medium))
                    }
                    Button { keyboard.toggle() } label: {
                        Image(systemName: keyboard ? "keyboard.chevron.compact.down" : "keyboard")
                            .font(.footnote)
                    }
                }
                .padding(.horizontal, 14).padding(.vertical, 8)
                .background(.ultraThinMaterial, in: Capsule())
                .padding(.leading, 16).padding(.top, 8)
                .transition(.opacity)
            }
        }
        .onTapGesture(count: 2) {
            withAnimation(.easeInOut(duration: 0.15)) { showChrome.toggle() }
        }
        .statusBarHidden(true)
    }
}
