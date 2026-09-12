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

    /// Guest resolution, for mapping touches back. Matches the mode requested
    /// of virtio-gpu in QemuRunner.
    private let guestWidth: CGFloat = 360
    private let guestHeight: CGFloat = 640

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
    func makeUIView(context: Context) -> HuskGLView { HuskGLView(frame: .zero) }
    func updateUIView(_ view: HuskGLView, context: Context) {}
}
