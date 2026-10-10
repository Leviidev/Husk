// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Multitasking, after LiveContainer's: an app opens in a window over Husk instead of taking the screen, so the rest of Husk stays
/// usable while it runs. The window has a title bar to drag it by, a corner to resize it from, and close / minimise / maximise
/// buttons; minimised, it waits in a dock at the edge of the screen.
///
/// LiveContainer runs each app in a process of its own and can show several. Husk's engines run in Husk's process and one per run
/// (an engine cannot be unloaded), so there is one window at a time; closing it pauses the app, as leaving full screen does.
@MainActor final class AppWindows: ObservableObject {
    static let shared = AppWindows()

    /// Settings › Multitasking: every app opens in a window unless its own settings say otherwise.
    static let defaultKey = "husk.multitask"
    static var defaultWindowed: Bool { UserDefaults.standard.bool(forKey: defaultKey) }

    static func opensInWindow(_ app: TLApp) -> Bool {
        switch TLAppSettings.load(app.id).window {
        case .window: return true
        case .fullScreen: return false
        case .auto: return defaultWindowed
        }
    }

    /// The app in the window.
    @Published private(set) var app: TLApp?
    @Published var minimized = false
    @Published var maximized = false
    /// Where the window is, in the layer's space; nil until it is first placed.
    @Published var frame: CGRect?
    /// How far down the screen the dock sits, as a fraction of the height, and on which side.
    @Published var dockY: CGFloat = 0.3
    @Published var dockLeft = false

    func open(_ app: TLApp) {
        if self.app?.id != app.id { frame = nil; maximized = false }
        self.app = app
        withAnimation(.snappy(duration: 0.25)) { minimized = false }
        TranslationLayerStore.shared.markPlayed(app)
    }

    /// Close the window; with an app given, only when it is that app's.
    func close(_ only: TLApp? = nil) {
        if let only, app?.id != only.id { return }
        withAnimation(.snappy(duration: 0.2)) { app = nil; minimized = false }
    }

    func minimize() { withAnimation(.snappy(duration: 0.25)) { minimized = true } }
    func restore() { withAnimation(.snappy(duration: 0.25)) { minimized = false } }
    func toggleMaximized() { withAnimation(.snappy(duration: 0.25)) { maximized.toggle() } }
}

/// The layer the window and the dock live in, over the tabs. Empty space in it lets touches through to Husk underneath.
struct AppWindowLayer: View {
    @ObservedObject private var windows = AppWindows.shared

    var body: some View {
        GeometryReader { geo in
            if let app = windows.app {
                if windows.minimized {
                    WindowDock(app: app, container: geo.size)
                } else {
                    AppWindow(app: app, container: geo.size)
                        .transition(.scale(scale: 0.2).combined(with: .opacity))
                }
            }
        }
    }
}

// MARK: - the window

private struct AppWindow: View {
    let app: TLApp
    let container: CGSize
    @ObservedObject private var windows = AppWindows.shared
    @StateObject private var model = TLUnityModel()
    /// The frame as a drag or a resize began.
    @State private var startFrame: CGRect?

    private static let bar: CGFloat = 36
    private static let minSize = CGSize(width: 180, height: 140)

    private var settings: TLAppSettings { TLAppSettings.load(app.id) }
    private var portrait: Bool { app.drawsPortrait(settings) }

    /// A first place for the window: the app's own shape, as large as fits comfortably, in the middle of the screen.
    private var initialFrame: CGRect {
        let aspect: CGFloat = portrait ? 19.5 / 9 : 9 / 16
        var w = portrait ? min(container.width * 0.82, 380) : container.width - 24
        var h = w * aspect + Self.bar
        let maxH = container.height * (portrait ? 0.78 : 0.7)
        if h > maxH { h = maxH; w = (h - Self.bar) / aspect }
        return CGRect(x: (container.width - w) / 2, y: (container.height - h) / 2.4, width: w, height: h)
    }

    private var frame: CGRect {
        if windows.maximized { return CGRect(origin: .zero, size: container) }
        return clamp(windows.frame ?? initialFrame)
    }

    /// Keep the title bar on the screen, so the window can always be reached.
    private func clamp(_ f: CGRect) -> CGRect {
        var f = f
        f.size.width = min(max(f.width, Self.minSize.width), container.width)
        f.size.height = min(max(f.height, Self.minSize.height), container.height)
        f.origin.x = min(max(f.minX, -f.width + 90), container.width - 90)
        f.origin.y = min(max(f.minY, 0), container.height - Self.bar)
        return f
    }

    /// Another app's engine is loaded in this run, and an engine cannot be loaded twice.
    private var blockedBy: String? {
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }), loaded != app.apks.first else { return nil }
        return (loaded as NSString).lastPathComponent
    }

    var body: some View {
        let f = frame
        VStack(spacing: 0) {
            titleBar
            content
        }
        .frame(width: f.width, height: f.height)
        .background(Color.black)
        .clipShape(RoundedRectangle(cornerRadius: windows.maximized ? 0 : 12, style: .continuous))
        .overlay {
            if !windows.maximized {
                RoundedRectangle(cornerRadius: 12, style: .continuous).strokeBorder(Color.primary.opacity(0.15), lineWidth: 0.5)
            }
        }
        .overlay(alignment: .bottomTrailing) { if !windows.maximized { resizeHandle } }
        .shadow(color: .black.opacity(windows.maximized ? 0 : 0.35), radius: 18, y: 6)
        .position(x: f.midX, y: f.midY)
        .onAppear {
            CrashReport.gameStarted(app)
            model.start()
            UIApplication.shared.isIdleTimerDisabled = settings.keepAwake
        }
        .onDisappear {
            CrashReport.gameEnded()
            model.stop()
            UIApplication.shared.isIdleTimerDisabled = false
        }
        .onChange(of: model.frames) { n in
            if n > 600, model.state == Int32(HUSK_UNITY_RUNNING) { GameStatusStore.shared.record(app.id, .plays) }
        }
        .onChange(of: model.state) { st in
            if st == Int32(HUSK_UNITY_FAILED) { GameStatusStore.shared.record(app.id, .failed) }
        }
        .onReceive(NotificationCenter.default.publisher(for: TLUnityUIView.appClosedItself)) { _ in windows.close() }
    }

    @ViewBuilder private var content: some View {
        if let other = blockedBy {
            VStack(spacing: 6) {
                Text("Another game is already loaded").font(.subheadline.weight(.semibold))
                Text("\(other) was started in this session. Close Husk completely and open it again to run \(app.label).")
                    .font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.center)
            }
            .foregroundStyle(.white)
            .padding(16)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        } else if let apk = app.apks.first {
            TLWindowScreen(apk: apk, extraApks: Array(app.apks.dropFirst()), dataDir: app.nativeDataDir,
                           engine: app.screenEngine, portrait: portrait, scale: settings.resolution.scale)
                .overlay {
                    if model.frames == 0 {
                        VStack(spacing: 10) {
                            if model.state == Int32(HUSK_UNITY_FAILED) || model.state == Int32(HUSK_UNITY_ENDED) {
                                Text(model.statusText).font(.subheadline.weight(.semibold)).foregroundStyle(.white)
                            } else {
                                ProgressView().tint(.white)
                                Text("Starting \(app.label)…").font(.caption).foregroundStyle(.white.opacity(0.7))
                            }
                        }
                        .allowsHitTesting(false)
                    }
                }
        }
    }

    private var titleBar: some View {
        HStack(spacing: 8) {
            light(.red, "xmark", "Close") { windows.close() }
            light(.yellow, "minus", "Minimise") { windows.minimize() }
            light(.green, windows.maximized ? "arrow.down.right.and.arrow.up.left" : "arrow.up.left.and.arrow.down.right",
                  windows.maximized ? "Restore" : "Maximise") { windows.toggleMaximized() }
            Spacer(minLength: 6)
            if app.screenEngine == .flutter {
                Button { husk_flutter_back() } label: {
                    Image(systemName: "chevron.backward").font(.system(size: 13, weight: .semibold)).frame(width: 28, height: 28)
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Back")
            }
            ItemIcon(item: .game(app), size: 18)
            Text(app.label).font(.footnote.weight(.semibold)).lineLimit(1)
            Spacer(minLength: 6)
            // Balances the lights, so the name sits in the middle.
            Color.clear.frame(width: 3 * 22 + 2 * 8, height: 1)
        }
        .padding(.horizontal, 10)
        .frame(height: Self.bar)
        .background(Color(uiColor: .secondarySystemBackground))
        .contentShape(Rectangle())
        .gesture(windows.maximized ? nil : moveGesture)
        .onTapGesture(count: 2) { windows.toggleMaximized() }
    }

    private func light(_ color: Color, _ symbol: String, _ label: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Image(systemName: symbol)
                .font(.system(size: 9, weight: .heavy))
                .foregroundStyle(.black.opacity(0.55))
                .frame(width: 22, height: 22)
                .background(color, in: Circle())
                .frame(width: 30, height: 30)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .frame(width: 22)
        .accessibilityLabel(label)
    }

    private var moveGesture: some Gesture {
        DragGesture(minimumDistance: 2, coordinateSpace: .global)
            .onChanged { v in
                let start = startFrame ?? frame
                if startFrame == nil { startFrame = start }
                windows.frame = clamp(start.offsetBy(dx: v.translation.width, dy: v.translation.height))
            }
            .onEnded { _ in startFrame = nil }
    }

    private var resizeHandle: some View {
        Image(systemName: "arrow.up.left.and.arrow.down.right")
            .font(.system(size: 10, weight: .bold))
            .foregroundStyle(.white.opacity(0.8))
            .frame(width: 22, height: 22)
            .background(Color.black.opacity(0.45), in: Circle())
            .frame(width: 44, height: 44)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 1, coordinateSpace: .global)
                    .onChanged { v in
                        let start = startFrame ?? frame
                        if startFrame == nil { startFrame = start }
                        var f = start
                        f.size.width = start.width + v.translation.width
                        f.size.height = start.height + v.translation.height
                        windows.frame = clamp(f)
                    }
                    .onEnded { _ in startFrame = nil }
            )
            .accessibilityLabel("Resize")
    }
}

// MARK: - the dock

/// The minimised window: the app's icon at the edge of the screen. Tap to bring the window back, drag to move it up or down or to
/// the other side.
private struct WindowDock: View {
    let app: TLApp
    let container: CGSize
    @ObservedObject private var windows = AppWindows.shared
    @State private var drag: CGSize = .zero

    private let size: CGFloat = 56

    var body: some View {
        let x = windows.dockLeft ? size / 2 + 8 : container.width - size / 2 - 8
        let y = min(max(windows.dockY * container.height, size), container.height - size)
        ItemIcon(item: .game(app), size: size - 12)
            .padding(6)
            .background(Color(uiColor: .secondarySystemBackground), in: RoundedRectangle(cornerRadius: 18, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 18, style: .continuous).strokeBorder(Color.primary.opacity(0.12), lineWidth: 0.5))
            .shadow(color: .black.opacity(0.3), radius: 10, y: 4)
            .position(x: x + drag.width, y: y + drag.height)
            .onTapGesture { windows.restore() }
            .gesture(
                DragGesture(minimumDistance: 4)
                    .onChanged { drag = $0.translation }
                    .onEnded { v in
                        let endX = x + v.translation.width
                        withAnimation(.snappy(duration: 0.25)) {
                            windows.dockLeft = endX < container.width / 2
                            windows.dockY = min(max((y + v.translation.height) / container.height, 0), 1)
                            drag = .zero
                        }
                    }
            )
            .contextMenu {
                Button { windows.restore() } label: { Label("Show", systemImage: "macwindow") }
                Button(role: .destructive) { windows.close() } label: { Label("Close", systemImage: "xmark") }
            }
            .accessibilityLabel("\(app.label), minimised")
            .accessibilityHint("Shows the window again")
    }
}

// MARK: - the screen in the window

/// The app's one view (TLUnityScreen's), held in a window. Flutter lays itself out at any size; every other engine was told its size
/// once as it started, so its picture is scaled to fit the window instead, touches and all.
struct TLWindowScreen: UIViewRepresentable {
    let apk: String
    let extraApks: [String]
    let dataDir: String
    let engine: TLNativeEngine
    let portrait: Bool
    let scale: CGFloat

    func makeUIView(context: Context) -> TLWindowHost {
        TLWindowHost(game: TLUnityScreen.view(apk: apk, extraApks: extraApks, dataDir: dataDir, engine: engine,
                                              portrait: portrait, scale: scale))
    }
    func updateUIView(_ view: TLWindowHost, context: Context) { view.setNeedsLayout() }
}

final class TLWindowHost: UIView {
    let game: TLUnityUIView

    init(game: TLUnityUIView) {
        self.game = game
        super.init(frame: .zero)
        backgroundColor = .black
        clipsToBounds = true
        game.windowed = true
        game.transform = .identity
        addSubview(game)
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func layoutSubviews() {
        super.layoutSubviews()
        guard game.superview === self, bounds.width > 0, bounds.height > 0 else { return }
        if let size = game.launchedSize, game.engine != .flutter, size.width > 0, size.height > 0 {
            let k = min(bounds.width / size.width, bounds.height / size.height)
            game.transform = .identity
            game.bounds = CGRect(origin: .zero, size: size)
            game.center = CGPoint(x: bounds.midX, y: bounds.midY)
            game.transform = CGAffineTransform(scaleX: k, y: k)
        } else {
            game.transform = .identity
            game.frame = bounds
            // The game starts in its own layout pass; once it has, a fixed-size engine needs placing as above.
            DispatchQueue.main.async { [weak self] in
                if self?.game.launchedSize != nil, self?.game.engine != .flutter { self?.setNeedsLayout() }
            }
        }
    }
}
