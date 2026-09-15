// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Husk's visual vocabulary, in one place so every screen agrees.
///
/// The app is dark, full stop. Not "dark when the phone is", which is a
/// different app that happens to share a binary: the depth here comes from a
/// near-black page with surfaces lifted a few points out of it, and that
/// arrangement has no light-mode translation that is the same design. The one
/// thing that still follows the system is the home screen icon, which is the
/// system's to draw.
enum Theme {
    /// The page. Near-black with a trace of blue in it, so surfaces above it
    /// read as lifted rather than as grey boxes on black.
    static let bg = Color(red: 0.043, green: 0.051, blue: 0.071)
    /// Cards, rows, anything holding content.
    static let surface = Color(red: 0.082, green: 0.094, blue: 0.129)
    /// One step further up: chips, icon wells, the things that sit on a card.
    static let surfaceHigh = Color(red: 0.118, green: 0.133, blue: 0.180)
    /// The edge that separates a surface from the page.
    static let hairline = Color.white.opacity(0.07)

    static let text = Color(red: 0.949, green: 0.957, blue: 0.976)
    static let textDim = Color(red: 0.545, green: 0.573, blue: 0.651)

    /// One accent, taken from the app icon, spent only on what you press.
    static let accent = Color(red: 0.353, green: 0.322, blue: 0.945)
    static let accentSoft = Color(red: 0.353, green: 0.322, blue: 0.945).opacity(0.16)
    static let good = Color(red: 0.204, green: 0.820, blue: 0.478)

    static let cardCorner: CGFloat = 18
    static let rowCorner: CGFloat = 14

    static var backdrop: some View { bg.ignoresSafeArea() }

    /// Dark bars to match the page, applied once at launch. SwiftUI has no
    /// vocabulary for the tab bar's own material, so this is UIKit's.
    static func applyBarAppearance() {
        let tab = UITabBarAppearance()
        tab.configureWithOpaqueBackground()
        tab.backgroundColor = UIColor(bg)
        tab.shadowColor = UIColor.white.withAlphaComponent(0.08)
        UITabBar.appearance().standardAppearance = tab
        UITabBar.appearance().scrollEdgeAppearance = tab

        let nav = UINavigationBarAppearance()
        nav.configureWithOpaqueBackground()
        nav.backgroundColor = UIColor(bg)
        nav.shadowColor = .clear
        nav.titleTextAttributes = [.foregroundColor: UIColor(text)]
        nav.largeTitleTextAttributes = [.foregroundColor: UIColor(text)]
        UINavigationBar.appearance().standardAppearance = nav
        UINavigationBar.appearance().scrollEdgeAppearance = nav
        UINavigationBar.appearance().compactAppearance = nav
    }
}

extension View {
    /// The app's one container: a lifted surface with a hairline edge.
    @ViewBuilder
    func huskCard<S: Shape>(_ shape: S, high: Bool = false) -> some View {
        self.background(high ? Theme.surfaceHigh : Theme.surface, in: shape)
            .overlay(shape.stroke(Theme.hairline, lineWidth: 0.5))
    }

    func huskCard(high: Bool = false) -> some View {
        huskCard(RoundedRectangle(cornerRadius: Theme.cardCorner, style: .continuous),
                 high: high)
    }

    /// Chrome that sits over the guest's own picture.
    ///
    /// Solid, not glass. iOS 26 will happily render this as Liquid Glass and it
    /// looks wrong here: a floating, refracting pill over a game is the phone's
    /// design language arguing with the app's, and over a dark guest screen it
    /// mostly reads as smeared. A flat panel with a hairline is what the design
    /// asks for, and it looks the same on every iOS.
    func huskPanel<S: Shape>(_ shape: S) -> some View {
        self.background(Theme.surface.opacity(0.94), in: shape)
            .overlay(shape.stroke(Color.white.opacity(0.10), lineWidth: 0.5))
    }
}

/// The tab bar, drawn rather than borrowed.
///
/// `TabView` still owns the tabs — their selection, their view lifetime, their
/// navigation stacks. Only the bar is ours: on iOS 26 the system draws it as a
/// floating glass capsule sitting proud of the screen, which is not the flat
/// bar pinned to the bottom edge that the design has. So the system's bar is
/// hidden and this one is inset in its place.
struct HuskTabBar: View {
    @Binding var selection: HuskTab

    var body: some View {
        HStack(spacing: 0) {
            ForEach(HuskTab.allCases) { tab in
                Button {
                    if selection != tab { UISelectionFeedbackGenerator().selectionChanged() }
                    selection = tab
                } label: {
                    VStack(spacing: 4) {
                        Image(systemName: tab.icon)
                            .font(.system(size: 18, weight: .medium))
                        Text(tab.title)
                            .font(.system(size: 10, weight: .medium))
                    }
                    .foregroundStyle(selection == tab ? Theme.accent : Theme.textDim)
                    .frame(maxWidth: .infinity)
                    .padding(.top, 10).padding(.bottom, 4)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .background(alignment: .top) {
            Rectangle().fill(Theme.hairline).frame(height: 0.5)
        }
        .background(Theme.bg.ignoresSafeArea(edges: .bottom))
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

    var body: some View {
        Group {
            if let art = HuskAppIcon.current.preview(dark: true) {
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
struct Tag: View {
    let text: String
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
struct DetailRow: View {
    let label: String
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
    let title: String
    var subtitle: String? = nil
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
    let title: String
    var trailing: String? = nil

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
    let text: String
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
    let title: String
    let message: String
    let systemImage: String
    var actionTitle: String? = nil
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
        .shadow(color: .black.opacity(0.4), radius: 18, y: 8)
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

/// A screen's own header: the mark or a back arrow, whatever buttons belong in
/// the corner, and the big title under them.
///
/// Drawn rather than left to the navigation bar. On iOS 26 the system bar and
/// every button in it is Liquid Glass — a floating, refracting capsule that
/// belongs to a different design than this one, and cannot be told not to be.
/// Pushed screens keep the real bar, where the back gesture and the title
/// behaviour matter more than the finish; the roots draw their own.
struct HuskHeader<Trailing: View>: View {
    var mark = false
    var back: (() -> Void)? = nil
    var title: String? = nil
    @ViewBuilder var trailing: Trailing

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 10) {
                if let back {
                    Button(action: back) {
                        Image(systemName: "chevron.left")
                            .font(.system(size: 16, weight: .semibold))
                            .foregroundStyle(Theme.text)
                            .frame(width: 36, height: 36)
                            .background(Theme.surfaceHigh, in: Circle())
                    }
                    .buttonStyle(.plain)
                } else if mark {
                    HuskMark(size: 32)
                }
                Spacer(minLength: 8)
                trailing
            }
            if let title {
                Text(title)
                    .font(.system(size: 32, weight: .bold))
                    .foregroundStyle(Theme.text)
            }
        }
        .padding(.top, 4)
    }
}

extension HuskHeader where Trailing == EmptyView {
    init(mark: Bool = false, back: (() -> Void)? = nil, title: String? = nil) {
        self.init(mark: mark, back: back, title: title) { EmptyView() }
    }
}
