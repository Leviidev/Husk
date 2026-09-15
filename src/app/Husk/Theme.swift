// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Husk's visual vocabulary, in one place so every screen agrees.
///
/// The screens grew one at a time and each invented its own spacing, its own
/// idea of a container and its own way of showing a value — which is why the app
/// reads as a pile of forms rather than one thing. Everything here exists to be
/// reused rather than restated.
enum Theme {
    /// Taken from the app icon's gradient, so the app and its icon agree.
    static let accent = Color(red: 0.36, green: 0.31, blue: 0.93)
    static let accentSoft = Color(red: 0.36, green: 0.31, blue: 0.93).opacity(0.14)

    static let cardCorner: CGFloat = 14
    static let rowSpacing: CGFloat = 12
}

/// Technical values — sizes, counts, frame rates, commit hashes — are set in a
/// monospaced face so digits line up between rows and do not reflow as they
/// change. A frame rate that jitters its own layout is hard to read.
extension Font {
    static func technical(_ size: CGFloat = 13, weight: Font.Weight = .regular) -> Font {
        .system(size: size, weight: weight, design: .monospaced)
    }
}

/// The app's one container shape.
struct Card<Content: View>: View {
    @ViewBuilder var content: Content

    var body: some View {
        content
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color(UIColor.secondarySystemGroupedBackground),
                        in: RoundedRectangle(cornerRadius: Theme.cardCorner))
    }
}

/// A label and a value on one line, for anything technical worth reading off.
struct DetailRow: View {
    let label: String
    let value: String
    var mono: Bool = true

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(label).foregroundStyle(.secondary)
            Spacer(minLength: 16)
            Text(value)
                .font(mono ? .technical() : .body)
                .multilineTextAlignment(.trailing)
                .textSelection(.enabled)
        }
        .font(.subheadline)
    }
}

/// A small status pill. The tint carries the meaning, the text the detail.
struct StatusPill: View {
    let text: String
    let systemImage: String
    var tint: Color = Theme.accent

    var body: some View {
        Label(text, systemImage: systemImage)
            .font(.caption.weight(.medium))
            .padding(.horizontal, 10).padding(.vertical, 5)
            .background(tint.opacity(0.16), in: Capsule())
            .foregroundStyle(tint)
    }
}

/// What a screen shows when it has nothing to show, with the one action that
/// would change that.
struct EmptyState: View {
    let title: String
    let message: String
    let systemImage: String
    var actionTitle: String? = nil
    var action: (() -> Void)? = nil

    var body: some View {
        VStack(spacing: 14) {
            Image(systemName: systemImage)
                .font(.system(size: 42, weight: .light))
                .foregroundStyle(.tertiary)
            Text(title).font(.headline)
            Text(message)
                .font(.callout).foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 36)
            if let actionTitle, let action {
                Button(actionTitle, action: action)
                    .buttonStyle(.borderedProminent)
                    .tint(Theme.accent)
                    .padding(.top, 2)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 30)
    }
}

/// A row of controls over the guest's picture.
///
/// Circular glyph buttons rather than a capsule of bare symbols: at this size a
/// tap target needs an edge to aim at, and the old pill gave every control the
/// same weight as its neighbours with nothing to separate them.
struct GuestControl: View {
    let systemImage: String
    var active = false
    var busy = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            ZStack {
                Circle()
                    .fill(active ? Theme.accent.opacity(0.9) : Color.black.opacity(0.35))
                    .frame(width: 34, height: 34)
                if busy {
                    ProgressView().scaleEffect(0.55).tint(.white)
                } else {
                    Image(systemName: systemImage)
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(.white)
                }
            }
        }
        .buttonStyle(.plain)
        .disabled(busy)
    }
}
