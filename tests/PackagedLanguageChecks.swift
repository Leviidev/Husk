// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// Run as an app bundle with resources copied from the IPA. Do not inject a
/// language bundle: this covers the same Bundle.main lookup the UI actually uses.
@main
enum PackagedLanguageChecks {
    static func main() {
        let language = CommandLine.arguments[1]
        precondition(Bundle.main.preferredLocalizations.first == language,
                     "Expected \(language), selected \(Bundle.main.preferredLocalizations)")
        for (source, chinese) in [
            ("Settings", "设置"),
            ("Library", "应用库"),
            ("Home", "首页"),
            ("Store", "商店"),
            ("General", "通用"),
        ] {
            let text = LocalizedText(stringLiteral: source)
            let expected = language == "zh-Hans" ? chinese : source
            precondition(L10n.tr(text) == expected, "\(source) did not select \(language) through Bundle.main")
        }
        print("Bundle.main selected \(language): \(L10n.tr("Settings")) / \(L10n.tr("Library"))")
    }
}
