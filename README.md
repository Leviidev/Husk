# Husk

<p align="center">
  <a href="https://trendshift.io/repositories/233057?utm_source=trendshift-badge&amp;utm_medium=badge&amp;utm_campaign=badge-trendshift-233057" target="_blank" rel="noopener noreferrer">
    <img src="https://trendshift.io/api/badge/trendshift/repositories/233057/daily?language=Swift" alt="Leviidev/Husk | Trendshift" width="250" height="55"/>
  </a>
</p>

[![Husk Downloads](https://img.shields.io/github/downloads/leviidev/husk/total?style=for-the-badge&color=5865F2&labelColor=111111)](https://github.com/leviidev/husk/releases)

Android app launcher for iOS.

Drop in an APK, tap it, and the Android app opens full-screen.

## JIT

Husk needs JIT, which on iOS takes an attached debugger. Use StikDebug, or
Husk's built-in StikJIT helper (iOS 26+), which on iOS 27 can pair with your
iPhone from Settings with no computer. The app walks you through it; see
[docs/06-built-in-jit.md](docs/06-built-in-jit.md).

## Languages

Husk is drawn in English or Simplified Chinese, following the phone unless you
pin one in **Settings › Language**.

The strings live in two String Catalogs, and there is no per-language code:

| file | what is in it |
| --- | --- |
| `src/app/Husk/Localizable.xcstrings` | every string in the app |
| `src/app/Husk/InfoPlist.xcstrings` | the one line Info.plist puts in front of the user |

English is the source language, so its strings are the keys and a key with no
translation falls back to itself. Adding a language is a matter of adding it to
those two catalogs and to `CFBundleLocalizations` in `Info.plist`.

Two things are worth knowing before editing a string:

- `Text("literal")` is localized on its own. Anything the compiler sees as a
  plain `String` — a ternary, a concatenation, a value out of a computed
  property, a parameter typed `String` — is not, and needs `localizedKey(...)`
  (for a view) or `localizedString(...)` (for a value that has to stay a
  `String`) from [`Localization.swift`](src/app/Husk/Localization.swift). A
  sentence with a value in the middle must be one literal with an interpolation
  — `Text("Version \(v)")`, key `Version %@` — and never a `+` between literals,
  which would make it a `String` and put the value in the key.
- Log lines, shell commands, paths and the names of things Android reports (app
  labels, ABIs, engine names) are deliberately not localized: they are evidence,
  not sentences.

## Builds

Every push builds an unsigned `Husk.ipa` in GitHub Actions
([build-ipa.yml](.github/workflows/build-ipa.yml)). It is attached to the run
as an artifact, ready for AltStore, SideStore or TrollStore to sign and
install. The first run builds QEMU and its dependencies from scratch, which
takes a couple of hours; after that they are cached.

## Licence

GPL-2.0-or-later. Husk links QEMU, which is GPLv2, so the shipped binary is a
combined GPLv2 work and the full source is public. It cannot go on the App
Store — both because of that and because it needs `get-task-allow` plus a
debugger attaching at runtime. See [docs/01-licensing.md](docs/01-licensing.md).