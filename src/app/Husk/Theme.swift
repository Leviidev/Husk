// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Husk's visual vocabulary, in one place so every screen agrees.
///
/// The screens grew one at a time and each invented its own spacing, its own
/// idea of a container and its own way of showing a value — which is why the app
/// read as a pile of forms rather than one thing. Everything here exists to be
/// reused rather than restated.
///
/// The one rule worth stating: the page itself is quiet. An earlier version
/// washed every screen in a purple gradient, which is the kind of thing that
/// looks designed in a mockup and cheap on a phone — the colour ends up behind
/// text, nothing sits on a definite surface, and the whole app reads as a theme
/// rather than as software. Colour is spent on the few things that are actually
/// the point: the accent on what you press, the app icons themselves.
enum Theme {
    /// Taken from the app icon's gradient, so the app and its icon agree.
    static let accent = Color(red: 0.36, green: 0.31, blue: 0.93)
    static let accentSoft = Color(red: 0.36, green: 0.31, blue: 0.93).opacity(0.12)
    /// The far end of the icon's gradient, for anything that wants depth.
    static let accentDeep = Color(red: 0.16, green: 0.13, blue: 0.85)

    /// The page. Grey in light, black in dark, exactly as every system app.
    static let background = Color(uiColor: .systemGroupedBackground)
    /// What content sits on: white over the grey, a step up out of the black.
    static let surface = Color(uiColor: .secondarySystemGroupedBackground)
    /// A single hairline is what separates a surface from the page without
    /// resorting to a drop shadow on everything.
    static let hairline = Color.primary.opacity(0.07)

    static let cardCorner: CGFloat = 16
    static let rowSpacing: CGFloat = 12

    /// The backdrop every screen sits on.
    static var backdrop: some View {
        background.ignoresSafeArea()
    }
}

extension View {
    /// A raised surface: the app's one container.
    ///
    /// Flat fill, hairline edge, and a shadow soft enough that you notice the
    /// card is lifted rather than noticing the shadow. `elevated` is for the
    /// few things that float above their own page.
    @ViewBuilder
    func huskCard<S: Shape>(_ shape: S, elevated: Bool = false) -> some View {
        self.background(Theme.surface, in: shape)
            .overlay(shape.stroke(Theme.hairline, lineWidth: 0.5))
            .shadow(color: .black.opacity(elevated ? 0.10 : 0.045),
                    radius: elevated ? 14 : 7, y: elevated ? 6 : 2)
    }

    func huskCard(elevated: Bool = false) -> some View {
        huskCard(RoundedRectangle(cornerRadius: Theme.cardCorner, style: .continuous),
                 elevated: elevated)
    }

    /// Liquid Glass, for chrome that floats over something else.
    ///
    /// Reserved for exactly that. Glass over a flat page has nothing to refract
    /// and just reads as a slightly dirty panel, so content uses `huskCard` and
    /// this is kept for the controls over the guest's picture and the strip that
    /// sits above the app grid.
    @ViewBuilder
    func huskGlass<S: Shape>(_ shape: S) -> some View {
        if #available(iOS 26.0, *) {
            self.glassEffect(.regular, in: shape)
        } else {
            self.background(.ultraThinMaterial, in: shape)
                .overlay(shape.stroke(Theme.hairline, lineWidth: 0.5))
        }
    }

    func huskGlass() -> some View {
        huskGlass(RoundedRectangle(cornerRadius: Theme.cardCorner, style: .continuous))
    }
}

/// Technical values — sizes, counts, frame rates, commit hashes — are set in a
/// monospaced face so digits line up between rows and do not reflow as they
/// change. A frame rate that jitters its own layout is hard to read.
extension Font {
    static func technical(_ size: CGFloat = 13, weight: Font.Weight = .regular) -> Font {
        .system(size: size, weight: weight, design: .monospaced)
    }
}

/// The one action a screen is for.
///
/// Solid accent rather than a tinted panel: the point of a primary button is
/// that there is no question which control it is, and a translucent one asks
/// the question every time.
struct PrimaryButtonStyle: ButtonStyle {
    var enabled = true

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.headline)
            .foregroundStyle(.white)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
            .background(enabled ? Theme.accent : Color.secondary.opacity(0.35),
                        in: Capsule())
            .shadow(color: enabled ? Theme.accent.opacity(0.28) : .clear,
                    radius: 12, y: 5)
            .opacity(configuration.isPressed ? 0.85 : 1)
            .scaleEffect(configuration.isPressed ? 0.985 : 1)
            .animation(.easeOut(duration: 0.12), value: configuration.isPressed)
    }
}

/// A card that is also a button: it moves a little under the finger, which is
/// the difference between a tile that responds and one that just redraws.
struct CardButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? 0.955 : 1)
            .opacity(configuration.isPressed ? 0.9 : 1)
            .animation(.easeOut(duration: 0.14), value: configuration.isPressed)
    }
}

/// The app's one container shape.
struct Card<Content: View>: View {
    @ViewBuilder var content: Content

    var body: some View {
        content
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .huskCard()
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
                .font(.system(size: 34, weight: .light))
                .foregroundStyle(Theme.accent)
                .frame(width: 66, height: 66)
                .background(Theme.accentSoft, in: Circle())
            Text(title).font(.title3.weight(.semibold))
            Text(message)
                .font(.subheadline).foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 30)
            if let actionTitle, let action {
                Button(actionTitle, action: action)
                    .buttonStyle(PrimaryButtonStyle())
                    .padding(.horizontal, 44)
                    .padding(.top, 6)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 38)
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
                    .fill(active ? Theme.accent.opacity(0.9) : Color.black.opacity(0.42))
                    .frame(width: 36, height: 36)
                    .overlay(Circle().stroke(Color.white.opacity(0.14), lineWidth: 0.5))
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
