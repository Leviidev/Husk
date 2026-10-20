// SPDX-License-Identifier: GPL-2.0-or-later
import AVFoundation
import SwiftUI
import UIKit

/// A PS4 game's screen: one CAMetalLayer that the PS4 layer (libhuskps4.dylib) renders into with Vulkan on MoltenVK.
///
/// The game is started once, the first time this view has a landscape size, and cannot be unloaded: leaving the screen pauses
/// it and coming back resumes it in the same layer, so the view itself is kept for the life of the process.
final class PS4SurfaceView: UIView {
    override class var layerClass: AnyClass { CAMetalLayer.self }

    static let shared = PS4SurfaceView()

    private(set) var game: PS4Game?
    private var launched = false
    private var lastSize = CGSize.zero

    private init() {
        super.init(frame: .zero)
        backgroundColor = .black
        isMultipleTouchEnabled = true
        // About 720p-and-up on a phone: the game draws at its own resolution and this is only what it is scaled to.
        contentScaleFactor = min(UIScreen.main.scale, 2)
    }
    required init?(coder: NSCoder) { fatalError() }

    func play(_ game: PS4Game) {
        self.game = game
        setNeedsLayout()
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        let size = bounds.size
        guard size.width > size.height, size.width > 0 else { return }   // landscape only: the game is told its size once
        let k = contentScaleFactor
        let w = Int(size.width * k), h = Int(size.height * k)
        if !launched {
            (layer as? CAMetalLayer)?.drawableSize = CGSize(width: w, height: h)
            if let game { launch(game, width: w, height: h) }
        } else if size != lastSize {
            // MoltenVK sizes the layer to the swapchain it makes; the game is told and remakes it.
            husk_console_resize(Int32(w), Int32(h))
        }
        lastSize = size
    }

    private func launch(_ game: PS4Game, width: Int, height: Int) {
        launched = true
        guard let module = PS4Store.moduleURL else { HuskLog.log("ps4", "libhuskps4.dylib is not in this build"); return }
        let fm = FileManager.default
        try? fm.createDirectory(at: game.dataDir, withIntermediateDirectories: true)
        try? fm.createDirectory(at: PS4Store.userDir, withIntermediateDirectories: true)
        do {
            let session = AVAudioSession.sharedInstance()
            try? session.setCategory(.playback, mode: .default, options: [.mixWithOthers])
            try? session.setActive(true)
        }
        let mvk = (Bundle.main.privateFrameworksPath ?? "") + "/MoltenVK.framework/MoltenVK"
        // Kept for the life of the process: the module copies what it needs, but the game runs on.
        let keep = { (s: String) in UnsafePointer(strdup(s)!) }
        var args = husk_console_launch_args()
        args.module_path = keep(module.path)
        args.game_id = keep(game.libraryID)
        args.game = keep(game.folder.path)
        args.game_args = nil
        args.user_dir = keep(PS4Store.userDir.path)
        args.log_path = keep(game.dataDir.appendingPathComponent("native-run.log").path)
        args.vulkan_path = keep(mvk)
        args.metal_layer = Unmanaged.passUnretained(layer).toOpaque()
        args.width_px = Int32(width)
        args.height_px = Int32(height)
        args.scale = Float(contentScaleFactor)
        args.region_bytes = 0
        HuskLog.log("ps4", "launching \(game.title) (\(game.titleID)) at \(width)x\(height)")
        var err = [CChar](repeating: 0, count: 512)
        husk_console_set_text_requester(PS4Keyboard.request)
        if !husk_console_launch(&args, &err, err.count) {
            let reason = String(cString: err)
            HuskLog.log("ps4", "launch refused: \(reason)")
            PS4Model.lastError = reason
        }
    }
}

private struct PS4Surface: UIViewRepresentable {
    let game: PS4Game
    func makeUIView(context: Context) -> PS4SurfaceView {
        let v = PS4SurfaceView.shared
        v.removeFromSuperview()
        v.play(game)
        return v
    }
    func updateUIView(_ view: PS4SurfaceView, context: Context) {}
}

/// The game's state, polled for the screen.
@MainActor final class PS4Model: ObservableObject {
    static var lastError: String?

    @Published var state = Int32(HUSK_CONSOLE_IDLE.rawValue)
    @Published var frames: UInt64 = 0
    @Published var fps = 0
    @Published var error: String?
    private var timer: Timer?
    private var lastFrames: UInt64 = 0

    func start() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
        poll()
    }
    func stop() { timer?.invalidate(); timer = nil }

    private func poll() {
        state = husk_console_state()
        let f = husk_console_frames()
        fps = Int(f &- lastFrames)
        lastFrames = f
        frames = f
        error = Self.lastError
    }

    var running: Bool { state == Int32(HUSK_CONSOLE_RUNNING.rawValue) || state == Int32(HUSK_CONSOLE_PAUSED.rawValue) }
    var statusText: String {
        if let error { return error }
        switch UInt32(state) {
        case HUSK_CONSOLE_IDLE.rawValue, HUSK_CONSOLE_BOOTING.rawValue: return L10n.tr("Starting…")
        case HUSK_CONSOLE_STOPPED.rawValue: return L10n.tr("The game has ended.")
        case HUSK_CONSOLE_FAILED.rawValue: return L10n.tr("The game could not start. Its log is in the game's Technical Details.")
        default: return ""
        }
    }
}

/// Full screen, landscape: the game, the on-screen pad when no controller is paired, and a way out.
struct PS4PlayView: View {
    let game: PS4Game
    @Environment(\.dismiss) private var dismiss
    @StateObject private var model = PS4Model()
    @StateObject private var virtualPad = VirtualPad()
    @ObservedObject private var pads = HuskGamepads.shared
    @State private var showBar = true

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            PS4Surface(game: game).ignoresSafeArea()
            if !model.running {
                VStack(spacing: 12) {
                    if model.error == nil, model.state <= Int32(HUSK_CONSOLE_BOOTING.rawValue) { ProgressView().tint(.white) }
                    Text(model.statusText)
                        .font(.system(size: 15, weight: .medium)).foregroundStyle(.white.opacity(0.85))
                        .multilineTextAlignment(.center).frame(maxWidth: 460)
                }
            }
            if model.running, pads.names.isEmpty {
                VirtualPadView(pad: virtualPad, opacity: 0.55).ignoresSafeArea()
            }
            if showBar {
                VStack {
                    HStack(spacing: 10) {
                        Button { leave() } label: {
                            Image(systemName: "xmark").font(.system(size: 15, weight: .bold)).foregroundStyle(.white)
                                .frame(width: 36, height: 36).background(.black.opacity(0.55), in: Circle())
                        }
                        if model.running {
                            Text("\(model.fps) fps").font(.system(size: 12, weight: .semibold).monospacedDigit())
                                .foregroundStyle(.white.opacity(0.8))
                                .padding(.horizontal, 8).padding(.vertical, 4).background(.black.opacity(0.45), in: Capsule())
                        }
                        Spacer()
                    }
                    Spacer()
                }
                .padding(14)
            }
        }
        .statusBarHidden(true)
        .huskHideSystemOverlays()
        .huskDefersSystemGestures(on: .all)
        .onTapGesture(count: 3) { withAnimation { showBar.toggle() } }
        .onAppear {
            HuskOrientation.set(.landscape)
            UIApplication.shared.isIdleTimerDisabled = true
            game.markPlayed()
            if husk_console_state() != Int32(HUSK_CONSOLE_IDLE.rawValue) { husk_console_set_paused(false) }
            model.start()
        }
        // iOS refuses GPU work from the background and the game's device would be lost: pause it
        // while Husk is not in front (app switcher, Control Center, a call).
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.willResignActiveNotification)) { _ in
            husk_console_set_paused(true)
        }
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.didBecomeActiveNotification)) { _ in
            husk_console_set_paused(false)
        }
        .onDisappear {
            husk_console_set_paused(true)
            virtualPad.disconnect()
            UIApplication.shared.isIdleTimerDisabled = false
            HuskOrientation.set(HuskOrientation.standard)
            model.stop()
        }
    }

    private func leave() {
        husk_console_set_paused(true)
        dismiss()
    }
}

/// A PS4 game's keyboard dialog (a world or character name), answered with an iOS text prompt. The game keeps polling its
/// dialog until the answer arrives; Cancel gives it the game's own cancel.
enum PS4Keyboard {
    static let request: husk_console_text_requester = { title, initial, maxChars, done, ctx in
        guard let done else { return -1 }
        let title = title.map { String(cString: $0) } ?? ""
        let initial = initial.map { String(cString: $0) } ?? ""
        let context = UInt(bitPattern: ctx)
        DispatchQueue.main.async {
            present(title: title, initial: initial, maxChars: Int(maxChars), done: done, ctx: UnsafeMutableRawPointer(bitPattern: context))
        }
        return 0
    }

    @MainActor private static func present(title: String, initial: String, maxChars: Int,
                                           done: @escaping @convention(c) (UnsafeMutableRawPointer?, UnsafePointer<CChar>?) -> Void,
                                           ctx: UnsafeMutableRawPointer?) {
        let root = UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.keyWindow?.rootViewController }.first
        guard var top = root else {
            HuskLog.log("ps4", "keyboard asked for, but there is no window to show it in; cancelled")
            done(ctx, nil)
            return
        }
        while let p = top.presentedViewController { top = p }
        HuskLog.log("ps4", "the game asked for text (\(title.isEmpty ? "no title" : title), up to \(maxChars) characters)")
        let alert = UIAlertController(title: title.isEmpty ? L10n.tr("Enter Text") : title, message: nil, preferredStyle: .alert)
        alert.addTextField {
            $0.text = initial
            $0.autocorrectionType = .no
            $0.clearButtonMode = .whileEditing
        }
        alert.addAction(UIAlertAction(title: L10n.tr("Cancel"), style: .cancel) { _ in done(ctx, nil) })
        alert.addAction(UIAlertAction(title: L10n.tr("OK"), style: .default) { [weak alert] _ in
            var text = alert?.textFields?.first?.text ?? ""
            if maxChars > 0, text.count > maxChars { text = String(text.prefix(maxChars)) }
            text.withCString { done(ctx, $0) }
        })
        top.present(alert, animated: true)
    }
}
