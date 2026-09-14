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

    var title: String {
        switch self {
        case .automatic:   return "Automatic"
        case .clearLight:  return "Clear Light"
        case .clearDark:   return "Clear Dark"
        case .tintedLight: return "Tinted Light"
        case .tintedDark:  return "Tinted Dark"
        }
    }

    var detail: String {
        self == .automatic
            ? "Follows the system: light, dark and tinted."
            : "Always this look."
    }

    /// What UIKit wants: nil for the primary, the asset name otherwise.
    var alternateName: String? { self == .automatic ? nil : rawValue }

    /// The artwork, for drawing a preview in Settings.
    ///
    /// Loaded by asset name, which works because the catalog is compiled with
    /// INCLUDE_ALL_APPICON_ASSETS — without that only the icon actually in use
    /// is addressable and every preview but one would be blank.
    var preview: UIImage? { UIImage(named: rawValue) }

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


/// The four places Husk can be.
///
/// Kept as a type rather than inlined into the TabView so the titles and icons
/// have one definition, and so `tag`/`selection` share a value the log can name.
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
}
