// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// A controller on the glass, for a game whose menus only answer one.
///
/// Minecraft Dungeons' menus are navigated with a gamepad (its touch controls exist only in a mission), so without a
/// controller paired there is no getting past the language screen. This is the whole controller: a D-pad, two analog
/// sticks, A/B/X/Y, both bumpers and triggers, Start and Back, reported to the runtime like any other controller. It takes
/// the last slot so a real controller, which GameController hands the first free one, is never displaced by it.
@MainActor
final class VirtualPad: ObservableObject {
    static let slot: Int32 = 3

    /// The bits of the button mask (husk-tl-gamepad.h, TL_PAD_*).
    enum Button: UInt32 {
        case a = 0, b, x, y, l1 = 4, r1, start = 8, select, up = 11, down, left, right
        var mask: UInt32 { 1 << rawValue }
    }

    /// Everything a controller reports at one moment. Sticks run -1...1 with y up, as GameController gives them.
    struct State: Equatable {
        var buttons: UInt32 = 0
        var lx: Float = 0, ly: Float = 0, rx: Float = 0, ry: Float = 0
        var lt: Float = 0, rt: Float = 0
    }

    private var connected = false
    private var last = State()

    func connect() {
        guard !connected else { return }
        connected = true
        last = State()
        husk_gamepad_connect(Self.slot, "Xbox Wireless Controller")
    }

    func disconnect() {
        guard connected else { return }
        connected = false
        last = State()
        husk_gamepad_disconnect(Self.slot)
    }

    func send(_ state: State) {
        guard connected, state != last else { return }
        last = state
        husk_gamepad_update(Self.slot, state.buttons, state.lx, state.ly, state.rx, state.ry, state.lt, state.rt)
    }
}

/// The pad as an overlay on the game. It is one UIKit view that does its own multi-touch hit-testing and drawing rather than a
/// SwiftUI view per key: with a gesture on every key, SwiftUI has to settle which of several overlapping recognizers owns a touch
/// before any of them fires (a key only registered once the finger had moved), a key's hit area was just its drawn size so a
/// thumb landing between two keys pressed neither, and a finger could not roll from one key to the next. Here every touch is
/// followed for itself, a key takes any touch within a generous margin of it (the nearest key wins), and the hit areas and the
/// drawing come from the same layout so they cannot drift apart. A touch that lands on no control falls through to the game.
struct VirtualPadView: UIViewRepresentable {
    let pad: VirtualPad
    var opacity: Double = 1
    var haptics = true

    func makeUIView(context: Context) -> PadView {
        pad.connect()
        let view = PadView(pad: pad)
        view.alpha = opacity
        view.hapticsEnabled = haptics
        return view
    }
    func updateUIView(_ view: PadView, context: Context) {
        view.alpha = opacity
        view.hapticsEnabled = haptics
    }
    static func dismantleUIView(_ view: PadView, coordinator: ()) { view.pad.disconnect() }
}

@MainActor
final class PadView: UIView {
    let pad: VirtualPad

    private enum Kind {
        case button(VirtualPad.Button, String)       // a round key
        case bumper(VirtualPad.Button, String)       // a long key
        case trigger(left: Bool, String)             // a long key that reports a trigger value
        case stick(left: Bool)
        case dpad
    }
    private struct Control {
        let kind: Kind
        let center: CGPoint
        let size: CGSize                              // a circle's diameter in width, or the key's width and height
        var radius: CGFloat { min(size.width, size.height) / 2 }
    }
    /// What one finger is doing: holding a stick, or pressing whatever is under it (and following it as it slides).
    private struct Finger {
        var stick: Int?
        var vector = CGPoint.zero                     // a held stick's push, -1...1, y down as the screen has it
        var buttons: UInt32 = 0
        var lt = false, rt = false
    }

    private var controls: [Control] = []
    private var fingers: [UITouch: Finger] = [:]
    private var shown = VirtualPad.State()
    private let haptic = UIImpactFeedbackGenerator(style: .light)
    var hapticsEnabled = true

    init(pad: VirtualPad) {
        self.pad = pad
        super.init(frame: .zero)
        isMultipleTouchEnabled = true
        isOpaque = false
        backgroundColor = .clear
        contentMode = .redraw
    }
    required init?(coder: NSCoder) { fatalError("not used") }

    // MARK: layout

    override func layoutSubviews() {
        super.layoutSubviews()
        controls = layout(in: bounds)
        setNeedsDisplay()
    }

    /// Where every control sits, from the size of the screen and the space the notch and the rounded corners take.
    private func layout(in bounds: CGRect) -> [Control] {
        let w = bounds.width, h = bounds.height
        guard w > 0, h > 0 else { return [] }
        let k = max(0.7, min(1.15, h / 372))
        let insets = window?.safeAreaInsets ?? safeAreaInsets
        let left = max(insets.left, 8), right = max(insets.right, 8), bottom = max(insets.bottom, 8)
        let m = 16 * k
        var list: [Control] = []

        let stickR = 62 * k
        list.append(Control(kind: .stick(left: true), center: CGPoint(x: left + m + stickR, y: h - bottom - m - stickR), size: CGSize(width: stickR * 2, height: stickR * 2)))
        let padR = 46 * k
        list.append(Control(kind: .dpad, center: CGPoint(x: left + m + padR, y: 106 * k), size: CGSize(width: padR * 2, height: padR * 2)))

        let face = 28 * k, spread = 58 * k
        let fc = CGPoint(x: w - right - m - spread - face, y: h - bottom - m - spread - face)
        list.append(Control(kind: .button(.y, "Y"), center: CGPoint(x: fc.x, y: fc.y - spread), size: CGSize(width: face * 2, height: face * 2)))
        list.append(Control(kind: .button(.a, "A"), center: CGPoint(x: fc.x, y: fc.y + spread), size: CGSize(width: face * 2, height: face * 2)))
        list.append(Control(kind: .button(.x, "X"), center: CGPoint(x: fc.x - spread, y: fc.y), size: CGSize(width: face * 2, height: face * 2)))
        list.append(Control(kind: .button(.b, "B"), center: CGPoint(x: fc.x + spread, y: fc.y), size: CGSize(width: face * 2, height: face * 2)))

        let rStickR = 46 * k
        list.append(Control(kind: .stick(left: false), center: CGPoint(x: w - right - m - rStickR - 10 * k, y: 106 * k), size: CGSize(width: rStickR * 2, height: rStickR * 2)))

        let bw = 64 * k, bh = 34 * k
        list.append(Control(kind: .trigger(left: true, "L2"), center: CGPoint(x: left + m + bw / 2, y: 10 * k + bh / 2), size: CGSize(width: bw, height: bh)))
        list.append(Control(kind: .bumper(.l1, "L1"), center: CGPoint(x: left + m + bw * 1.5 + 8 * k, y: 10 * k + bh / 2), size: CGSize(width: bw, height: bh)))
        list.append(Control(kind: .trigger(left: false, "R2"), center: CGPoint(x: w - right - m - bw / 2, y: 10 * k + bh / 2), size: CGSize(width: bw, height: bh)))
        list.append(Control(kind: .bumper(.r1, "R1"), center: CGPoint(x: w - right - m - bw * 1.5 - 8 * k, y: 10 * k + bh / 2), size: CGSize(width: bw, height: bh)))

        let sw = 60 * k, sh = 26 * k
        list.append(Control(kind: .bumper(.select, "Back"), center: CGPoint(x: w / 2 - sw / 2 - 12 * k, y: 10 * k + sh / 2 + 4 * k), size: CGSize(width: sw, height: sh)))
        list.append(Control(kind: .bumper(.start, "Start"), center: CGPoint(x: w / 2 + sw / 2 + 12 * k, y: 10 * k + sh / 2 + 4 * k), size: CGSize(width: sw, height: sh)))
        return list
    }

    // MARK: hit-testing

    /// How far a touch at `p` is from a control, as a fraction of how far it may be and still count: under 1 is a hit.
    private func reach(_ c: Control, _ p: CGPoint) -> CGFloat {
        let dx = p.x - c.center.x, dy = p.y - c.center.y
        switch c.kind {
        case .stick: return hypot(dx, dy) / (c.radius * 1.3)
        case .dpad: return hypot(dx, dy) / (c.radius * 1.25)
        case .button: return hypot(dx, dy) / (c.radius * 1.4)
        case .bumper, .trigger:
            let sx = abs(dx) / (c.size.width / 2 + 8), sy = abs(dy) / (c.size.height / 2 + 8)
            return max(sx, sy)
        }
    }

    private func control(at p: CGPoint) -> Int? {
        var best: (Int, CGFloat)?
        for (i, c) in controls.enumerated() {
            let r = reach(c, p)
            if r <= 1, best == nil || r < best!.1 { best = (i, r) }
        }
        return best?.0
    }

    /// Only the controls take touches; everywhere else the game underneath gets them.
    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        control(at: point) != nil ? self : nil
    }

    /// What a finger at `p` presses, for one that is not holding a stick.
    private func press(at p: CGPoint) -> Finger {
        var f = Finger()
        guard let i = control(at: p) else { return f }
        let c = controls[i]
        switch c.kind {
        case .button(let b, _), .bumper(let b, _): f.buttons = b.mask
        case .trigger(let left, _): if left { f.lt = true } else { f.rt = true }
        case .dpad: f.buttons = dpadBits(p, c)
        case .stick: break
        }
        return f
    }

    /// The D-pad as eight directions: a touch toward a corner is both of its directions, as on a real one, and the middle is dead.
    private func dpadBits(_ p: CGPoint, _ c: Control) -> UInt32 {
        let dx = p.x - c.center.x, dy = p.y - c.center.y
        if hypot(dx, dy) < c.radius * 0.22 { return 0 }
        var deg = atan2(-dy, dx) * 180 / .pi
        if deg < 0 { deg += 360 }
        typealias B = VirtualPad.Button
        switch deg {
        case 22.5..<67.5: return B.up.mask | B.right.mask
        case 67.5..<112.5: return B.up.mask
        case 112.5..<157.5: return B.up.mask | B.left.mask
        case 157.5..<202.5: return B.left.mask
        case 202.5..<247.5: return B.down.mask | B.left.mask
        case 247.5..<292.5: return B.down.mask
        case 292.5..<337.5: return B.down.mask | B.right.mask
        default: return B.right.mask
        }
    }

    // MARK: touches

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches {
            let p = t.location(in: self)
            if let i = control(at: p), case .stick = controls[i].kind {
                var f = Finger(); f.stick = i
                f.vector = stickVector(p, controls[i])
                fingers[t] = f
            } else {
                fingers[t] = press(at: p)
            }
        }
        apply()
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches {
            guard var f = fingers[t] else { continue }
            let p = t.location(in: self)
            if let i = f.stick { f.vector = stickVector(p, controls[i]) } else { f = press(at: p) }
            fingers[t] = f
        }
        apply()
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches { fingers[t] = nil }
        apply()
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches { fingers[t] = nil }
        apply()
    }

    /// A stick's push: how far the finger is from its centre, as a fraction of the stick's radius, capped at one.
    private func stickVector(_ p: CGPoint, _ c: Control) -> CGPoint {
        let dx = p.x - c.center.x, dy = p.y - c.center.y
        let d = hypot(dx, dy)
        guard d > 0 else { return .zero }
        let s = min(d, c.radius) / c.radius
        return CGPoint(x: dx / d * s, y: dy / d * s)
    }

    /// Fold every finger into one controller state and report it.
    private func apply() {
        var s = VirtualPad.State()
        for (_, f) in fingers {
            s.buttons |= f.buttons
            if f.lt { s.lt = 1 }
            if f.rt { s.rt = 1 }
            if let i = f.stick, case .stick(let left) = controls[i].kind {
                if left { s.lx = Float(f.vector.x); s.ly = Float(-f.vector.y) } else { s.rx = Float(f.vector.x); s.ry = Float(-f.vector.y) }
            }
        }
        if hapticsEnabled, s.buttons & ~shown.buttons != 0 || (s.lt > shown.lt) || (s.rt > shown.rt) { haptic.impactOccurred() }
        shown = s
        pad.send(s)
        setNeedsDisplay()
    }

    // MARK: drawing

    override func draw(_ rect: CGRect) {
        for c in controls {
            switch c.kind {
            case .stick(let left):
                drawStick(c, push: stickPush(left: left))
            case .dpad:
                drawDPad(c)
            case .button(let b, let label):
                drawRound(c, label: label, down: shown.buttons & b.mask != 0)
            case .bumper(let b, let label):
                drawKey(c, label: label, down: shown.buttons & b.mask != 0)
            case .trigger(let left, let label):
                drawKey(c, label: label, down: (left ? shown.lt : shown.rt) > 0)
            }
        }
    }

    /// How far the finger holding one of the sticks has pushed it, if one is.
    private func stickPush(left: Bool) -> CGPoint? {
        for f in fingers.values {
            guard let i = f.stick, case .stick(let l) = controls[i].kind, l == left else { continue }
            return f.vector
        }
        return nil
    }

    private func fill(_ a: CGFloat) -> UIColor { UIColor.white.withAlphaComponent(a) }

    private func drawStick(_ c: Control, push: CGPoint?) {
        let r = c.radius
        let base = UIBezierPath(arcCenter: c.center, radius: r, startAngle: 0, endAngle: .pi * 2, clockwise: true)
        fill(0.12).setFill(); base.fill()
        fill(0.35).setStroke(); base.lineWidth = 1.5; base.stroke()
        let reach = r * 0.55
        let at = CGPoint(x: c.center.x + (push?.x ?? 0) * reach, y: c.center.y + (push?.y ?? 0) * reach)
        let thumb = UIBezierPath(arcCenter: at, radius: r * 0.42, startAngle: 0, endAngle: .pi * 2, clockwise: true)
        fill(push == nil ? 0.28 : 0.55).setFill(); thumb.fill()
        fill(0.5).setStroke(); thumb.lineWidth = 1.5; thumb.stroke()
    }

    private func drawDPad(_ c: Control) {
        let r = c.radius
        let base = UIBezierPath(arcCenter: c.center, radius: r, startAngle: 0, endAngle: .pi * 2, clockwise: true)
        fill(0.12).setFill(); base.fill()
        fill(0.35).setStroke(); base.lineWidth = 1.5; base.stroke()
        typealias B = VirtualPad.Button
        let arrows: [(B, CGFloat, CGFloat)] = [(.up, 0, -1), (.down, 0, 1), (.left, -1, 0), (.right, 1, 0)]
        for (b, dx, dy) in arrows {
            let on = shown.buttons & b.mask != 0
            let tip = CGPoint(x: c.center.x + dx * r * 0.78, y: c.center.y + dy * r * 0.78)
            let back = CGPoint(x: c.center.x + dx * r * 0.42, y: c.center.y + dy * r * 0.42)
            let side = CGPoint(x: -dy, y: dx)                                  // across the arrow
            let half = r * 0.24
            let tri = UIBezierPath()
            tri.move(to: tip)
            tri.addLine(to: CGPoint(x: back.x + side.x * half, y: back.y + side.y * half))
            tri.addLine(to: CGPoint(x: back.x - side.x * half, y: back.y - side.y * half))
            tri.close()
            fill(on ? 0.85 : 0.4).setFill(); tri.fill()
        }
    }

    private func label(_ text: String, in rect: CGRect, size: CGFloat, alpha: CGFloat) {
        let attrs: [NSAttributedString.Key: Any] = [.font: UIFont.systemFont(ofSize: size, weight: .bold), .foregroundColor: fill(alpha)]
        let s = (text as NSString).size(withAttributes: attrs)
        (text as NSString).draw(at: CGPoint(x: rect.midX - s.width / 2, y: rect.midY - s.height / 2), withAttributes: attrs)
    }

    private func drawRound(_ c: Control, label text: String, down: Bool) {
        let r = c.radius
        let path = UIBezierPath(arcCenter: c.center, radius: r, startAngle: 0, endAngle: .pi * 2, clockwise: true)
        fill(down ? 0.45 : 0.18).setFill(); path.fill()
        fill(0.4).setStroke(); path.lineWidth = 1.5; path.stroke()
        label(text, in: CGRect(x: c.center.x - r, y: c.center.y - r, width: r * 2, height: r * 2), size: r * 0.8, alpha: down ? 1 : 0.85)
    }

    private func drawKey(_ c: Control, label text: String, down: Bool) {
        let rect = CGRect(x: c.center.x - c.size.width / 2, y: c.center.y - c.size.height / 2, width: c.size.width, height: c.size.height)
        let path = UIBezierPath(roundedRect: rect, cornerRadius: min(c.size.height / 2, 14))
        fill(down ? 0.45 : 0.18).setFill(); path.fill()
        fill(0.4).setStroke(); path.lineWidth = 1.5; path.stroke()
        label(text, in: rect, size: min(15, c.size.height * 0.46), alpha: down ? 1 : 0.85)
    }
}
