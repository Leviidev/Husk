// SPDX-License-Identifier: GPL-2.0-or-later
import UIKit

/// The app icon, as something a person can choose.
///
/// The six artworks are appearance variants of one design — default, dark, two
/// clear and two tinted. iOS can apply appearance variants automatically, but it
/// can only *switch* between icons that are separately named, so each variant is
/// its own appiconset and the choice goes through `setAlternateIconName`.
enum HuskAppIcon: String, CaseIterable, Identifiable {
    /// nil name = the primary icon. UIKit uses nil rather than a name for it,
    /// and passing the primary's own name is an error.
    case defaultIcon  = "AppIcon"
    case dark         = "AppIconDark"
    case clearLight   = "AppIconClearLight"
    case clearDark    = "AppIconClearDark"
    case tintedLight  = "AppIconTintedLight"
    case tintedDark   = "AppIconTintedDark"

    var id: String { rawValue }

    var title: String {
        switch self {
        case .defaultIcon: return "Default"
        case .dark:        return "Dark"
        case .clearLight:  return "Clear Light"
        case .clearDark:   return "Clear Dark"
        case .tintedLight: return "Tinted Light"
        case .tintedDark:  return "Tinted Dark"
        }
    }

    /// What UIKit wants: nil for the primary, the asset name otherwise.
    var alternateName: String? { self == .defaultIcon ? nil : rawValue }

    /// The artwork, for drawing a preview in Settings.
    ///
    /// Loaded by asset name, which works because the catalog is compiled with
    /// INCLUDE_ALL_APPICON_ASSETS — without that only the icon actually in use
    /// is addressable and every preview but one would be blank.
    var preview: UIImage? { UIImage(named: rawValue) }

    static var current: HuskAppIcon {
        guard let name = UIApplication.shared.alternateIconName else { return .defaultIcon }
        return HuskAppIcon(rawValue: name) ?? .defaultIcon
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
