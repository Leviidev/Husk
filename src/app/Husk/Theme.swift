// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Husk's visual vocabulary.
///
/// The app is drawn with the system's own materials: grouped lists on the system's grouped background,
/// the system's tab bar and navigation bars, its label colours, and an accent the user can change
/// (`AppTheme`). The names below remain so every screen can say what it means ("a surface", "dim text")
/// without caring which system colour that is today -- but none of them is a colour of Husk's own, so the
/// app is light or dark as the phone is, and as the user pins it.
///
/// The one thing that is not the system's is the chrome over the guest's picture (`huskPanel`,
/// `GuestControl`), which is drawn flat and always dark: it floats over a picture of another phone.
enum Theme {
    /// The page behind grouped content.
    static let bgUI = UIColor.systemGroupedBackground
    static let bg = Color(uiColor: bgUI)
    /// Cards, rows, anything holding content.
    static let surface = Color(uiColor: .secondarySystemGroupedBackground)
    /// One step further up: wells, chips, the things that sit on a card.
    static let surfaceHigh = Color(uiColor: .tertiarySystemFill)
    /// The edge that separates a surface from the page.
    static let hairlineUI = UIColor.separator
    static let hairline = Color(uiColor: hairlineUI)

    static let textUI = UIColor.label
    static let text = Color(uiColor: textUI)
    static let textDim = Color(uiColor: .secondaryLabel)

    /// The user's accent: what is pressed, selected or switched on.
    static var accent: Color { AppTheme.shared.accentColor }
    static var accentSoft: Color { AppTheme.shared.accentColor.opacity(0.16) }
    static let good = Color(uiColor: .systemGreen)
    /// What a floating thing casts.
    static let shadow = Color.black.opacity(0.25)

    /// The appearance the app is drawn in. System is the default: it follows the phone, light by day and
    /// dark by night. Light and Dark pin one.
    enum Appearance: String, CaseIterable, Identifiable {
        case system, light, dark

        static let key = "husk.appearance"

        var id: String { rawValue }

        var title: LocalizedStringKey {
            switch self {
            case .system: return "System"
            case .light: return "Light"
            case .dark: return "Dark"
            }
        }

        var style: UIUserInterfaceStyle {
            switch self {
            case .system: return .unspecified
            case .light: return .light
            case .dark: return .dark
            }
        }

        static var current: Appearance {
            Appearance(rawValue: UserDefaults.standard.string(forKey: key) ?? "") ?? .system
        }
    }

    /// Applies an appearance to every window the app has.
    ///
    /// Through UIKit rather than `.preferredColorScheme`: going back to "follow the system" means handing
    /// the window `nil`, and SwiftUI does not reliably let go of a scheme it has once forced. The window's
    /// own override does, and sheets and covers presented from it inherit it.
    static func apply(_ appearance: Appearance) {
        for case let scene as UIWindowScene in UIApplication.shared.connectedScenes {
            for window in scene.windows {
                window.overrideUserInterfaceStyle = appearance.style
            }
        }
    }

    static let cardCorner: CGFloat = 14
    static let rowCorner: CGFloat = 12

    static var backdrop: some View { bg.ignoresSafeArea() }
}

extension View {
    /// A container for content that is not a list row: a surface on the grouped page, as the system draws one.
    @ViewBuilder
    func huskCard<S: Shape>(_ shape: S, high: Bool = false) -> some View {
        self.background(high ? Theme.surfaceHigh : Theme.surface, in: shape)
    }

    func huskCard(high: Bool = false) -> some View {
        huskCard(RoundedRectangle(cornerRadius: Theme.cardCorner, style: .continuous), high: high)
    }

    /// Chrome that sits over the guest's own picture.
    ///
    /// Solid, not glass. iOS 26 will happily render this as Liquid Glass and it looks wrong here: a
    /// floating, refracting pill over a game is the phone's design language arguing with the guest's. A
    /// flat panel with a hairline looks the same on every iOS.
    ///
    /// Always dark, whatever the app's appearance: it floats over a guest that is mostly black, and the
    /// controls on it are drawn in white.
    func huskPanel<S: Shape>(_ shape: S) -> some View {
        self.background(Color(red: 0.082, green: 0.094, blue: 0.129).opacity(0.94), in: shape)
            .overlay(shape.stroke(Color.white.opacity(0.10), lineWidth: 0.5))
            .environment(\.colorScheme, .dark)
    }
}

/// Technical values — sizes, counts, frame rates, commit hashes — are set in a
/// monospaced face so digits line up between rows and do not reflow as they
/// change.
extension Font {
    static func technical(_ size: CGFloat = 13, weight: Font.Weight = .regular) -> Font {
        .system(size: size, weight: weight, design: .monospaced)
    }
}

/// The one action a screen is for.
struct PrimaryButtonStyle: ButtonStyle {
    var enabled = true

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 16, weight: .semibold))
            .foregroundStyle(enabled ? .white : Theme.textDim)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 15)
            .background(enabled ? Theme.accent : Theme.surfaceHigh,
                        in: RoundedRectangle(cornerRadius: 15, style: .continuous))
            .opacity(configuration.isPressed ? 0.85 : 1)
            .scaleEffect(configuration.isPressed ? 0.985 : 1)
            .animation(.easeOut(duration: 0.12), value: configuration.isPressed)
    }
}

/// A card that is also a button: it moves a little under the finger.
struct CardButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? 0.96 : 1)
            .opacity(configuration.isPressed ? 0.9 : 1)
            .animation(.easeOut(duration: 0.14), value: configuration.isPressed)
    }
}

/// The round glyph buttons in a screen's top corner.
struct CircleButton: View {
    let systemImage: String
    var active = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: systemImage)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(active ? .white : Theme.text)
                .frame(width: 36, height: 36)
                .background(active ? Theme.accent : Theme.surfaceHigh, in: Circle())
        }
        .buttonStyle(.plain)
    }
}

/// Husk's mark, as drawn by whichever app icon is in use.
struct HuskMark: View {
    var size: CGFloat = 32
    @Environment(\.colorScheme) private var scheme

    var body: some View {
        Group {
            if let art = HuskAppIcon.current.preview(dark: scheme == .dark) {
                Image(uiImage: art).resizable().scaledToFit()
            } else {
                Image(systemName: "cube.fill").font(.system(size: size * 0.6))
                    .foregroundStyle(Theme.accent)
            }
        }
        .frame(width: size, height: size)
        .clipShape(RoundedRectangle(cornerRadius: size * 0.26, style: .continuous))
    }
}

/// A filter pill.
struct Chip: View {
    let title: String
    let selected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 14, weight: .medium))
                .foregroundStyle(selected ? .white : Theme.textDim)
                .padding(.horizontal, 16).padding(.vertical, 8)
                .background(selected ? Theme.accent : Theme.surfaceHigh, in: Capsule())
        }
        .buttonStyle(.plain)
    }
}

/// A small tag under a title — a category, an ABI, a state.
///
/// The text is a key, so a category or an ABI that has no entry is drawn as
/// itself, which is what data should do.
struct Tag: View {
    let text: LocalizedStringKey
    var tint: Color = Theme.textDim

    var body: some View {
        Text(text)
            .font(.system(size: 12, weight: .medium))
            .foregroundStyle(tint)
            .padding(.horizontal, 10).padding(.vertical, 4)
            .background(Theme.surfaceHigh, in: Capsule())
    }
}

/// A label and a value on one line, for anything worth reading off.
///
/// The label is a key; the value is data — a count, a size, a state — and is
/// localized by whoever knows whether it is a sentence or a measurement.
struct DetailRow: View {
    let label: LocalizedStringKey
    let value: String
    var mono: Bool = true

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(label).foregroundStyle(Theme.textDim)
            Spacer(minLength: 16)
            Text(value)
                .font(mono ? .technical() : .system(size: 15))
                .foregroundStyle(Theme.text)
                .multilineTextAlignment(.trailing)
                .textSelection(.enabled)
        }
        .font(.system(size: 15))
    }
}

/// One row of a grouped card: an icon, a title, an optional subtitle, and the
/// chevron that says it goes somewhere.
struct HuskRow: View {
    let systemImage: String
    let title: LocalizedStringKey
    var subtitle: LocalizedStringKey? = nil
    var tint: Color = Theme.text
    var showsChevron = true

    var body: some View {
        HStack(spacing: 14) {
            Image(systemName: systemImage)
                .font(.system(size: 16, weight: .medium))
                .foregroundStyle(tint)
                .frame(width: 34, height: 34)
                .background(Theme.surfaceHigh,
                            in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            VStack(alignment: .leading, spacing: 2) {
                Text(title)
                    .font(.system(size: 15, weight: .medium))
                    .foregroundStyle(tint)
                if let subtitle {
                    Text(subtitle)
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.textDim)
                        .lineLimit(1)
                }
            }
            Spacer(minLength: 8)
            if showsChevron {
                Image(systemName: "chevron.right")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(Theme.textDim.opacity(0.7))
            }
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
        .contentShape(Rectangle())
    }
}

/// Rows stacked into one card, hairlines between them.
struct RowGroup<Content: View>: View {
    @ViewBuilder var content: Content

    var body: some View {
        VStack(spacing: 0) { content }
            .huskCard()
    }
}

/// The hairline between two rows in a group.
struct RowDivider: View {
    var body: some View {
        Rectangle().fill(Theme.hairline)
            .frame(height: 0.5)
            .padding(.leading, 62)
    }
}

/// A section label above a group.
struct SectionHeader: View {
    let title: LocalizedStringKey
    var trailing: LocalizedStringKey? = nil

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title)
                .font(.system(size: 17, weight: .semibold))
                .foregroundStyle(Theme.text)
            Spacer()
            if let trailing {
                Text(trailing).font(.system(size: 13)).foregroundStyle(Theme.textDim)
            }
        }
    }
}

/// A small status pill. The tint carries the meaning, the text the detail.
struct StatusPill: View {
    let text: LocalizedStringKey
    let systemImage: String
    var tint: Color = Theme.accent

    var body: some View {
        Label(text, systemImage: systemImage)
            .font(.system(size: 12, weight: .medium))
            .padding(.horizontal, 10).padding(.vertical, 5)
            .background(tint.opacity(0.16), in: Capsule())
            .foregroundStyle(tint)
    }
}

/// What a screen shows when it has nothing to show.
struct EmptyState: View {
    let title: LocalizedStringKey
    let message: LocalizedStringKey
    let systemImage: String
    var actionTitle: LocalizedStringKey? = nil
    var action: (() -> Void)? = nil

    var body: some View {
        VStack(spacing: 14) {
            Image(systemName: systemImage)
                .font(.system(size: 30, weight: .light))
                .foregroundStyle(Theme.accent)
                .frame(width: 64, height: 64)
                .background(Theme.accentSoft, in: Circle())
            Text(title)
                .font(.system(size: 18, weight: .semibold))
                .foregroundStyle(Theme.text)
            Text(message)
                .font(.system(size: 14))
                .foregroundStyle(Theme.textDim)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 28)
            if let actionTitle, let action {
                Button(actionTitle, action: action)
                    .buttonStyle(PrimaryButtonStyle())
                    .padding(.horizontal, 44)
                    .padding(.top, 6)
            }
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 40)
    }
}

/// What finished, said once and then gone.
///
/// Progress belongs in a strip that stays while the work does; this is for the
/// moment after — an APK installed, a machine saved. It says the thing and
/// leaves, because an outcome that needs dismissing is a dialog.
struct Toast: Equatable, Identifiable {
    let id = UUID()
    let title: String
    var detail: String?
    var good = true
}

struct ToastView: View {
    let toast: Toast
    let onClose: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: toast.good ? "checkmark.circle.fill"
                                         : "exclamationmark.triangle.fill")
                .font(.system(size: 19))
                .foregroundStyle(toast.good ? Theme.good : .orange)
            VStack(alignment: .leading, spacing: 2) {
                Text(toast.title)
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(Theme.text)
                if let detail = toast.detail {
                    Text(detail)
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.textDim)
                        .lineLimit(2)
                }
            }
            Spacer(minLength: 8)
            Button(action: onClose) {
                Image(systemName: "xmark")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(Theme.textDim)
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
        .background(Theme.surfaceHigh,
                    in: RoundedRectangle(cornerRadius: Theme.rowCorner, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: Theme.rowCorner, style: .continuous)
                    .stroke(Theme.hairline, lineWidth: 0.5))
        .shadow(color: Theme.shadow, radius: 18, y: 8)
    }
}

/// One control over the guest's picture.
struct GuestControl: View {
    let systemImage: String
    var active = false
    var busy = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            ZStack {
                if busy {
                    ProgressView().scaleEffect(0.6).tint(.white)
                } else {
                    Image(systemName: systemImage)
                        .font(.system(size: 17, weight: .medium))
                        .foregroundStyle(active ? Theme.accent : .white)
                }
            }
            .frame(width: 46, height: 42)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .disabled(busy)
    }
}
