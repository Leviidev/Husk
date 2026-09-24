# Husk

[![Husk Downloads](https://img.shields.io/github/downloads/leviidev/husk/total?style=for-the-badge&color=5865F2&labelColor=111111)](https://github.com/leviidev/husk/releases)

Android app launcher for iOS.

Drop in an APK, tap it, and the Android app opens full-screen.

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
