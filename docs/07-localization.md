# Localization

Husk supports English (`en`) and Simplified Chinese (`zh-Hans`). English remains the
development language and fallback for unsupported languages. The app, download
Live Activity and built-in JIT helper include both localizations.

iOS selects the app language from the user's preferences. Settings > App Language
opens Husk's system settings, where the preferred language can be selected.
The guest-language choice in onboarding applies to apps inside Husk; it is separate
from the language of Husk's interface.

The translated interface covers navigation, onboarding, settings, import and
download flows, JIT setup, help, game pages, save management and user-facing errors.
Third-party app/game content and technical diagnostics may still use their own
language. F-Droid metadata prefers an available Chinese translation when Husk is
using Chinese, with an English fallback.

## Editing translations

- `src/app/Shared/Localization.swift` provides `L10n.tr` for both SwiftUI and strings
  produced by models, alerts and UIKit.
- `src/app/Shared/{en,zh-Hans}.lproj/Localizable.strings` contains the English source
  keys and translations. `project.yml` includes these in all three targets.
- `src/app/Husk/{en,zh-Hans}.lproj/InfoPlist.strings` localizes the local-network
  permission explanation.

Use the same entry point for literals and interpolated text:

```swift
Text(L10n.tr("Settings"))
status = L10n.tr("Installing \(name)…")
```

Every interpolation is represented by `%@` in the catalog. A translation can use
positional arguments such as `%1$@` and `%3$@` to reorder data. Literal percent signs
are escaped as `%%`. A few Chinese translations omit the argument containing only
an English plural suffix; those exceptions are listed in the catalog check.

Keep protocol keys, enum raw values, file paths and log-parsing markers unchanged.
Localize their presentation rather than values that code stores or compares.

## Validation

On a Mac with Xcode:

```sh
./scripts/check_localization.sh
```

This checks catalog parity, placeholder preservation, English fallback, percentage
escaping and interpolation, including Chinese word order and plural suffixes.

After building an IPA with the normal build scripts:

```sh
python3 scripts/check_ipa_language.py build/Husk.ipa
```

The package check reads the app and extensions' actual language declarations and
resources from the IPA. It runs the production localization helper through
`Bundle.main` in a temporary macOS app wrapper, without injecting a selected
localization bundle. It verifies English and Chinese selection, plus English
fallback for an unsupported language. This checks bundle lookup, not iOS gameplay.
For an explicitly Chinese-only distribution, pass `--chinese-only`.
