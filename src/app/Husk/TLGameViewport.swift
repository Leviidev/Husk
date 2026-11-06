// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
#if canImport(CoreGraphics)
import CoreGraphics
#endif

/// Full-screen pixels and safe controls are independent. A running engine keeps its launch size;
/// changes to the available area fit that surface without changing its touch coordinates.
enum TLGameViewport {
    enum CutoutEdge { case none, top, left, bottom, right }

    struct Placement: Equatable {
        let bounds: CGRect
        let center: CGPoint
        let scale: CGFloat
    }

    /// Describe the final geometry without an identity-transform step that changes safe areas.
    static func placement(_ size: CGSize, in area: CGRect) -> Placement? {
        let fitted = fit(size, in: area)
        guard valid(fitted) else { return nil }
        return Placement(bounds: CGRect(origin: .zero, size: size),
                         center: CGPoint(x: fitted.midX, y: fitted.midY), scale: fitted.width / size.width)
    }

    /// UIKit's landscape safe area reserves both short edges and the Home indicator. Only the
    /// physical camera edge is a cutout; do not turn those other margins into phantom cutouts.
    static func cutoutInsets(view: CGRect, safeFrame: CGRect, edge: CutoutEdge) -> (top: CGFloat, left: CGFloat, bottom: CGFloat, right: CGFloat) {
        let remaining = insets(view: view, safeFrame: safeFrame)
        switch edge {
        case .top: return (remaining.top, 0, 0, 0)
        case .left: return (0, remaining.left, 0, 0)
        case .bottom: return (0, 0, remaining.bottom, 0)
        case .right: return (0, 0, 0, remaining.right)
        case .none: return (0, 0, 0, 0)
        }
    }

    static func fit(_ size: CGSize, in area: CGRect) -> CGRect {
        guard valid(area), size.width.isFinite, size.height.isFinite, size.width > 0, size.height > 0 else { return .zero }
        let scale = min(area.width / size.width, area.height / size.height)
        let fitted = CGSize(width: size.width * scale, height: size.height * scale)
        return CGRect(x: area.midX - fitted.width / 2, y: area.midY - fitted.height / 2,
                      width: fitted.width, height: fitted.height)
    }

    /// Insets relative to this surface, including when a cached view has been moved or scaled.
    static func insets(view: CGRect, safeFrame: CGRect) -> (top: CGFloat, left: CGFloat, bottom: CGFloat, right: CGFloat) {
        guard valid(view), valid(safeFrame) else { return (0, 0, 0, 0) }
        return (min(view.height, max(0, safeFrame.minY - view.minY)),
                min(view.width, max(0, safeFrame.minX - view.minX)),
                min(view.height, max(0, view.maxY - safeFrame.maxY)),
                min(view.width, max(0, view.maxX - safeFrame.maxX)))
    }

    private static func valid(_ rect: CGRect) -> Bool {
        !rect.isNull && !rect.isInfinite && rect.width > 0 && rect.height > 0
            && rect.origin.x.isFinite && rect.origin.y.isFinite && rect.width.isFinite && rect.height.isFinite
    }
}
