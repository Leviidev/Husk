// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Husk's translations.
///
/// The strings themselves live in `Localizable.xcstrings`, with `en` as the
/// source language and every other language beside it; `InfoPlist.xcstrings`
/// holds the one line Info.plist shows the user. Adding a language means adding
/// it to those two catalogs and nothing else -- there is no per-language code.
///
/// SwiftUI localizes a `Text("literal")` on its own: the literal becomes a
/// `LocalizedStringKey` and is looked up in the catalog. Text the compiler sees
/// as a plain `String` is *not* a key and skips the catalog entirely, which is
/// what the two helpers below are for. That covers a ternary, a concatenation,
/// a value out of a computed property, and a parameter typed `String`.
///
/// A key with no catalog entry resolves to itself, so data -- an app's own name,
/// an ABI, a path -- passes through these unchanged and needs no entry.

/// A `LocalizedStringKey` for text the compiler sees as `String`.
///
/// `Text(working ? "Refreshing…" : "Refresh")` is a `String`, so it is drawn
/// verbatim; `Text(localizedKey(working ? "Refreshing…" : "Refresh"))` is looked
/// up like any other literal.
func localizedKey(_ key: String) -> LocalizedStringKey { LocalizedStringKey(key) }

/// A localized `String`, for values that have to stay `String`.
///
/// A row's value is assembled at runtime -- `installed` or `not found`, a count,
/// a size -- and `DetailRow` takes it as a `String`. `String(localized:)` wants a
/// `LocalizationValue`, which only a literal can become, so a runtime `String`
/// comes through here instead.
func localizedString(_ key: String) -> String {
    String(localized: String.LocalizationValue(key))
}

/// The language Husk is drawn in.
///
/// The phone's own choice unless the user pins one here. A pinned language is
/// written to `AppleLanguages` in Husk's own defaults, which is the key iOS reads
/// when it picks a `.lproj` for the process. That happens once, at launch, so a
/// change shows up the next time Husk opens rather than immediately -- which is
/// what the note in `LanguageSettings` is for.
enum AppLanguage: String, CaseIterable, Identifiable {
    /// Whatever the phone is set to.
    case system
    case english = "en"
    case chineseSimplified = "zh-Hans"

    static let key = "husk.language"

    var id: String { rawValue }

    /// What the picker shows. The two languages are named in themselves, as a
    /// language picker should be; only "System" is a translated word.
    var title: LocalizedStringKey {
        switch self {
        case .system:            return "System"
        case .english:           return "English"
        case .chineseSimplified: return "简体中文"
        }
    }

    /// The identifier to pin, or nil to leave the choice to the system.
    var code: String? { self == .system ? nil : rawValue }

    static var current: AppLanguage {
        AppLanguage(rawValue: UserDefaults.standard.string(forKey: key) ?? "") ?? .system
    }

    /// Writes the choice where iOS reads it, and remembers it so the picker can
    /// show it again. An absent `AppleLanguages` is what "follow the system"
    /// means: the bundle's own localizations decide, in the phone's order.
    static func apply(_ language: AppLanguage) {
        let defaults = UserDefaults.standard
        if let code = language.code {
            defaults.set([code], forKey: "AppleLanguages")
            defaults.set(language.rawValue, forKey: key)
        } else {
            defaults.removeObject(forKey: "AppleLanguages")
            defaults.removeObject(forKey: key)
        }
        defaults.set(!isShowing(language), forKey: pendingKey)
    }

    /// The language this process is drawing. `AppleLanguages` is read once, at
    /// launch, so this is what is on screen right now.
    static let launchedWith: String = Bundle.main.preferredLocalizations.first ?? "en"

    /// Whether what is on screen is already the language asked for.
    private static func isShowing(_ language: AppLanguage) -> Bool {
        guard let code = language.code else { return false }
        // A pinned language and the phone's own can be the same language under
        // different spellings -- `zh-Hans` against `zh-Hans-CN` -- so compare
        // the language part rather than the whole identifier.
        return launchedWith.hasPrefix(code)
    }

    private static let pendingKey = "husk.languagePendingRestart"

    /// Whether the choice now on disk needs a relaunch to be seen. Recorded when
    /// the choice is written rather than worked out later: going back to "follow
    /// the system" removes the pinned identifier there is nothing left to
    /// compare against, and the answer has to survive the screen being reopened.
    static var isPending: Bool { UserDefaults.standard.bool(forKey: pendingKey) }
}
