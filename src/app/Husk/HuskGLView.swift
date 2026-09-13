// SPDX-License-Identifier: GPL-2.0-or-later
import UIKit
import SwiftUI
import QuartzCore

/// The GL display surface: a CAMetalLayer that ANGLE renders into directly.
///
/// This inverts the software path. There, QEMU handed us a CPU framebuffer and
/// HuskMetalView uploaded it to a texture itself. Here nothing is uploaded at
/// all: virglrenderer executes the guest's GL commands against an ANGLE context
/// bound to this layer, so the phone's GPU draws the frame and QEMU only asks
/// for it to be presented. The layer is the output, not the input.
final class HuskGLView: UIView {
    /// Published as soon as the view exists, because the QEMU thread needs it
    /// before it can bring the GL display up and it has no way to reach UIKit.
    static let surfaceReady = NSCondition()
    nonisolated(unsafe) static var layerForGL: CAMetalLayer?
    nonisolated(unsafe) static var pixelSize: CGSize = .zero

    /// The one view for the process.
    ///
    /// QEMU builds its EGL window surface against whichever CAMetalLayer is
    /// published when GL comes up, and never rebuilds it. A second HuskGLView
    /// therefore does not take over the display -- it orphans it: the old layer
    /// keeps receiving every frame while the new one, the one actually on
    /// screen, receives none. The symptom is a frame counter climbing happily
    /// against a black screen, which is precisely what happened once the guest
    /// screen became a conditional sibling in a ZStack and SwiftUI started
    /// tearing it down whenever the library appeared over it.
    ///
    /// Handing out a single instance makes that impossible. UIKit moves a view
    /// between parents without recreating it, so the layer QEMU holds stays the
    /// layer being composited.
    @MainActor static let shared = HuskGLView(frame: .zero)

    /// Guest resolution, for mapping touches back -- read from what QEMU was
    /// actually told rather than kept as a second copy here, which is how this
    /// came to claim 640 while the command line said 800.
    private var guestWidth: CGFloat { CGFloat(QemuRunner.lastGuestRes.w) }
    private var guestHeight: CGFloat { CGFloat(QemuRunner.lastGuestRes.h) }

    override class var layerClass: AnyClass { CAMetalLayer.self }

    private var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = false
        backgroundColor = .black
        metalLayer.device = MTLCreateSystemDefaultDevice()
        metalLayer.pixelFormat = .bgra8Unorm
        // ANGLE presents through this layer, so it must not be framebufferOnly:
        // the surface is rendered into rather than merely displayed.
        metalLayer.framebufferOnly = false
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func layoutSubviews() {
        super.layoutSubviews()
        let scale = window?.screen.scale ?? UIScreen.main.scale
        metalLayer.contentsScale = scale
        metalLayer.drawableSize = CGSize(width: bounds.width * scale,
                                         height: bounds.height * scale)
        Self.surfaceReady.lock()
        Self.layerForGL = metalLayer
        Self.pixelSize = metalLayer.drawableSize
        Self.surfaceReady.broadcast()
        Self.surfaceReady.unlock()
    }

    // MARK: touches

    /// The guest is letterboxed inside the view, exactly as in the software
    /// path, so the same mapping applies -- the GL path changes who draws, not
    /// where the picture ends up.
    private func guestPoint(from p: CGPoint) -> (Int32, Int32)? {
        guard bounds.width > 0, bounds.height > 0 else { return nil }
        let scale = min(bounds.width / guestWidth, bounds.height / guestHeight)
        let drawW = guestWidth * scale
        let drawH = guestHeight * scale
        let gx = (p.x - (bounds.width - drawW) / 2) / scale
        let gy = (p.y - (bounds.height - drawH) / 2) / scale
        guard gx >= 0, gy >= 0, gx < guestWidth, gy < guestHeight else { return nil }
        return (Int32(gx), Int32(gy))
    }

    private func send(_ touch: UITouch, down: Bool) {
        guard let (x, y) = guestPoint(from: touch.location(in: self)) else { return }
        husk_display_send_pointer(x, y, down)
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first { send(t, down: true) }
    }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first { send(t, down: true) }
    }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first { send(t, down: false) }
    }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = touches.first { send(t, down: false) }
    }
}

struct HuskGLScreen: UIViewRepresentable {
    // The shared instance, never a fresh one -- see HuskGLView.shared.
    func makeUIView(context: Context) -> HuskGLView { HuskGLView.shared }
    func updateUIView(_ view: HuskGLView, context: Context) {}
}
