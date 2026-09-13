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
    nonisolated(unsafe) static var lastLandscape: Bool?

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
        // Opaque, or Core Animation blends the frame against what is behind it
        // using an alpha channel nobody in this pipeline maintains. Android's
        // scanout is commonly B8G8R8X8 -- the fourth byte is ignored by
        // definition, and zero in practice -- so a frame drawn perfectly is a
        // frame composited to nothing. The symptom is an entirely black screen
        // with the frame counter reporting 60 fps.
        isOpaque = true
        metalLayer.isOpaque = true
        // Black again. The magenta tell-tale that briefly lived here existed to
        // decide whether this layer reached the screen at all, and the boot
        // console settled that: it was visible, so the layer composites and
        // ANGLE presents into it. What was black was the picture itself.
        backgroundColor = .black
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    /// The size published on the run that QEMU actually bound its EGL surface
    /// to. Logged on every change afterwards, because a later resize does NOT
    /// reach that surface -- ANGLE keeps drawing at the size it was created
    /// with, and the mismatch shows up as a picture in the wrong corner or no
    /// picture at all, with a frame counter that looks perfectly healthy.
    private var lastLoggedSize: CGSize = .zero

    override func layoutSubviews() {
        super.layoutSubviews()
        let scale = window?.screen.scale ?? UIScreen.main.scale
        metalLayer.contentsScale = scale
        metalLayer.drawableSize = CGSize(width: bounds.width * scale,
                                         height: bounds.height * scale)
        HuskMetalPresenter.shared.attach(layer: metalLayer)

        // Give the guest a display the shape of the screen.
        //
        // The previous approach rotated Android inside a fixed portrait panel
        // and turned the picture back in the shader. Android honoured the
        // rotation and then letterboxed the app into a sub-rectangle: small,
        // and unresponsive, because the window accepting input was that
        // rectangle rather than where a finger mapped to. Correctly placed
        // touches landed in no window at all.
        //
        // A modeset is the real answer. The guest's virtio-gpu driver changes
        // resolution, the scanout becomes genuinely landscape, and nothing needs
        // rotating -- not the shader, not the touch map, both of which are back
        // to their portrait forms because there is no longer anything to undo.
        let landscape = bounds.width > bounds.height
        if landscape != Self.lastLandscape {
            Self.lastLandscape = landscape
            let base = QemuRunner.lastGuestRes
            let short = min(base.w, base.h), long = max(base.w, base.h)
            let w = landscape ? long : short
            let h = landscape ? short : long
            HuskLog.log("ui", "screen is \(landscape ? "landscape" : "portrait"); "
                            + "asking the guest for \(w)x\(h)")
            husk_display_set_ui_size(Int32(w), Int32(h))
            QemuRunner.lastGuestRes = (w: w, h: h)
        }

        Self.surfaceReady.lock()
        let first = Self.layerForGL == nil
        Self.layerForGL = metalLayer
        Self.pixelSize = metalLayer.drawableSize
        Self.surfaceReady.broadcast()
        Self.surfaceReady.unlock()

        let size = metalLayer.drawableSize
        if size != lastLoggedSize {
            lastLoggedSize = size
            HuskLog.log("gl", "\(first ? "published" : "re-laid out") the GL layer: "
                            + "bounds \(Int(bounds.width))x\(Int(bounds.height)) "
                            + "@\(scale)x -> drawable \(Int(size.width))x\(Int(size.height)), "
                            + "window=\(window != nil)")
        }
    }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        HuskLog.log("gl", window == nil
            ? "the GL view left the window (its layer keeps the EGL surface)"
            : "the GL view is in a window")
        if window != nil {
            // Walk up from this view to the window and say what each ancestor
            // is doing to it. A view that is hidden, transparent, zero-sized or
            // covered looks exactly like a view that is drawing black, and only
            // one of those is a GPU problem.
            //
            // More than once, because the hierarchy changes: the first dump ran
            // four seconds in and reported the GL view COVERED by the software
            // display, which was true at that instant and no longer true eight
            // seconds later when the snapshot finished loading and GL bound. A
            // single early sample described a transient as if it were the
            // steady state.
            for t in [4.0, 30.0, 90.0] {
                DispatchQueue.main.asyncAfter(deadline: .now() + t) {
                    self.describePlacement(why: "\(Int(t))s after reaching a window")
                }
            }
        }
    }

    func describePlacement(why: String) {
        guard window != nil else {
            HuskLog.log("gl", "placement (\(why)): the view is not in a window")
            return
        }
        HuskLog.log("gl", "placement (\(why)):")
        var v: UIView? = self
        var depth = 0
        while let cur = v {
            let f = cur.frame
            HuskLog.log("gl", String(
                format: "placement[%d] %@ frame=%.0f,%.0f %.0fx%.0f alpha=%.2f "
                      + "hidden=%@ opaque=%@ clips=%@ layerOpacity=%.2f",
                depth, String(describing: type(of: cur)),
                f.origin.x, f.origin.y, f.width, f.height, cur.alpha,
                cur.isHidden ? "yes" : "no", cur.isOpaque ? "yes" : "no",
                cur.clipsToBounds ? "yes" : "no", Double(cur.layer.opacity)))
            // Anything drawn after this view, inside the same parent, is
            // painted over it -- which is how an opaque black rectangle hid the
            // guest once already.
            if let parent = cur.superview,
               let idx = parent.subviews.firstIndex(of: cur) {
                let above = parent.subviews[(idx + 1)...]
                    .filter { !$0.isHidden && $0.alpha > 0.01 && $0.frame.contains(cur.frame) }
                if !above.isEmpty {
                    HuskLog.log("gl", "placement[\(depth)] COVERED by "
                              + above.map { String(describing: type(of: $0)) }
                                     .joined(separator: ", "))
                }
            }
            v = cur.superview
            depth += 1
        }
        HuskLog.log("gl", "placement: drawable \(Int(metalLayer.drawableSize.width))"
                        + "x\(Int(metalLayer.drawableSize.height)) "
                        + "opaque=\(metalLayer.isOpaque) "
                        + "presentsWithTransaction=\(metalLayer.presentsWithTransaction) "
                        + "device=\(metalLayer.device?.name ?? "none")")
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
