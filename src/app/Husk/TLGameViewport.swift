// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
#if canImport(CoreGraphics)
import CoreGraphics
#endif

/// Geometry shared by full-screen and floating game surfaces. A running engine keeps its launch size;
/// changes to the available area fit that surface without cropping or changing its touch coordinates.
enum TLGameViewport {
    static func area(in bounds: CGRect, safeFrame: CGRect, avoidUnsafeArea: Bool) -> CGRect {
        let area = avoidUnsafeArea ? bounds.intersection(safeFrame) : bounds
        return valid(area) ? area : .zero
    }

    static func fit(_ size: CGSize, in area: CGRect) -> CGRect {
        guard valid(area), size.width.isFinite, size.height.isFinite, size.width > 0, size.height > 0 else { return .zero }
        let scale = min(area.width / size.width, area.height / size.height)
        let fitted = CGSize(width: size.width * scale, height: size.height * scale)
        return CGRect(x: area.midX - fitted.width / 2, y: area.midY - fitted.height / 2,
                      width: fitted.width, height: fitted.height)
    }

    /// Only the parts of this view still outside the window's safe frame. An already inset surface
    /// must report zero to Android/SDL/Flutter instead of applying the notch padding a second time.
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
