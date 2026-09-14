// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// The four places Husk can be.
enum HuskTab: String, CaseIterable, Identifiable {
    case android
    case library
    case console
    case settings

    var id: String { rawValue }

    var title: String {
        switch self {
        case .android:  return "Android"
        case .library:  return "Library"
        case .console:  return "Console"
        case .settings: return "Settings"
        }
    }

    var icon: String {
        switch self {
        case .android:  return "rectangle.inset.filled"
        case .library:  return "square.grid.2x2.fill"
        case .console:  return "terminal"
        case .settings: return "gearshape"
        }
    }

    /// Whether this tab is worth anything with the guest stopped.
    ///
    /// The library and console both read from the guest, so they are dimmed
    /// rather than hidden: a tab that vanishes and reappears is harder to learn
    /// than one that is visibly unavailable.
    var needsGuest: Bool { self == .library }
}

/// A hand-rolled bar rather than a TabView.
///
/// TabView unloads the views of unselected tabs, and the Android tab owns the
/// CAMetalLayer that QEMU renders into — losing it turns the guest black while
/// every frame counter keeps climbing. Keeping the guest mounted underneath and
/// drawing the other tabs over it is the only arrangement that survives that.
struct HuskTabBar: View {
    @Binding var tab: HuskTab
    let guestRunning: Bool

    var body: some View {
        HStack(spacing: 0) {
            ForEach(HuskTab.allCases) { item in
                let available = guestRunning || !item.needsGuest
                Button {
                    guard tab != item else { return }
                    tab = item
                    HuskLog.log("ui", "tab: \(item.rawValue)")
                } label: {
                    VStack(spacing: 3) {
                        Image(systemName: item.icon)
                            .font(.system(size: 17, weight: .medium))
                        Text(item.title).font(.system(size: 10, weight: .medium))
                    }
                    .foregroundStyle(tab == item ? AnyShapeStyle(.tint)
                                                 : AnyShapeStyle(.secondary))
                    .opacity(available ? 1 : 0.35)
                    .frame(maxWidth: .infinity)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!available)
            }
        }
        .padding(.top, 8)
        // The bar sits on the home indicator, so the bottom padding is the safe
        // area's rather than a guessed constant.
        .padding(.bottom, 2)
        .background(.ultraThinMaterial)
        .overlay(alignment: .top) {
            Rectangle().fill(Color.primary.opacity(0.12)).frame(height: 0.5)
        }
    }
}
