// SPDX-License-Identifier: GPL-2.0-or-later
import UIKit

/// The app icon, as something a person can choose.
///
/// The six artworks are appearance variants of one design — default, dark, two
/// clear and two tinted. iOS can apply appearance variants automatically, but it
/// can only *switch* between icons that are separately named, so each variant is
/// its own appiconset and the choice goes through `setAlternateIconName`.
enum HuskAppIcon: String, CaseIterable, Identifiable {
    /// The primary icon, which carries its own appearances.
    ///
    /// nil name = primary. UIKit uses nil rather than a name for it, and passing
    /// the primary's own name is an error. Because that appiconset declares
    /// dark and tinted variants, leaving it selected is what lets iOS switch the
    /// artwork with the system setting -- no picker involved, and nothing this
    /// code has to observe.
    case automatic    = "AppIcon"
    case clearLight   = "AppIconClearLight"
    case clearDark    = "AppIconClearDark"
    case tintedLight  = "AppIconTintedLight"
    case tintedDark   = "AppIconTintedDark"

    var id: String { rawValue }

    /// A localized `String` rather than a key: this is drawn as a caption and
    /// also named in the log when the icon is applied.
    var title: String {
        switch self {
        case .automatic:   return localizedString("Automatic")
        case .clearLight:  return localizedString("Clear Light")
        case .clearDark:   return localizedString("Clear Dark")
        case .tintedLight: return localizedString("Tinted Light")
        case .tintedDark:  return localizedString("Tinted Dark")
        }
    }

    var detail: String {
        self == .automatic
            ? localizedString("Follows the system: light, dark and tinted.")
            : localizedString("Always this look.")
    }

    /// What UIKit wants: nil for the primary, the asset name otherwise.
    var alternateName: String? { self == .automatic ? nil : rawValue }

    /// The artwork, for drawing this icon inside the app.
    ///
    /// From a plain bundled PNG, not the asset catalog. Appicon assets are not
    /// addressable through UIImage(named:) even with INCLUDE_ALL_APPICON_ASSETS
    /// — the previews in Settings came out blank, and the start screen showed a
    /// stale copy of the default artwork whichever icon was actually in use.
    /// Shipping the images as ordinary resources is what makes them loadable.
    ///
    /// `dark` matters only for Automatic, which has no fixed look of its own:
    /// it is whatever the system is currently showing.
    func preview(dark: Bool = false) -> UIImage? {
        switch self {
        case .automatic:   return UIImage(named: dark ? "icon-dark" : "icon-default")
        case .clearLight:  return UIImage(named: "icon-clearlight")
        case .clearDark:   return UIImage(named: "icon-cleardark")
        case .tintedLight: return UIImage(named: "icon-tintedlight")
        case .tintedDark:  return UIImage(named: "icon-tinteddark")
        }
    }

    static var current: HuskAppIcon {
        guard let name = UIApplication.shared.alternateIconName else { return .automatic }
        return HuskAppIcon(rawValue: name) ?? .automatic
    }

    /// Apply this icon.
    ///
    /// iOS shows its own "You have changed the icon" alert afterwards and there
    /// is no way to suppress it, so this deliberately does not add one of its
    /// own — two dialogs for one tap would be worse than the system's.
    static func apply(_ icon: HuskAppIcon) {
        guard UIApplication.shared.supportsAlternateIcons else {
            HuskLog.log("ui", "this device does not support alternate app icons")
            return
        }
        guard icon.alternateName != UIApplication.shared.alternateIconName else { return }
        UIApplication.shared.setAlternateIconName(icon.alternateName) { error in
            if let error {
                HuskLog.log("ui", "could not set the \(icon.title) icon: "
                                + error.localizedDescription)
            } else {
                HuskLog.log("ui", "app icon set to \(icon.title)")
            }
        }
    }
}


/// The two tabs.
///
/// Android and the console used to be tabs of their own. Android is not one any
/// more because the guest has to stay mounted whatever is on screen, and the
/// console moved into Diagnostics -- it is something you go looking for, not a
/// quarter of the app's navigation.
enum HuskTab: String, CaseIterable, Identifiable {
    case translation
    case library
    case files
    case settings

    var id: String { rawValue }

    var title: String {
        switch self {
        case .translation: return "Native"
        case .library:  return "Library"
        case .files:    return "Files"
        case .settings: return "Settings"
        }
    }

    var icon: String {
        switch self {
        case .translation: return "gamecontroller.fill"
        case .library:  return "square.grid.2x2.fill"
        case .files:    return "folder.fill"
        case .settings: return "gearshape.fill"
        }
    }
}
