# Update an already signed app over USB

This workflow updates Husk from an existing `.app` directory. It does not build
the application, sign it, open Sideloadly, invoke GitHub Actions or export an IPA
file on Windows. Build and signing happen separately, only when requested.

The transport uses `pymobiledevice3`. Its installation service internally packs
the directory into IPA bytes and uploads a temporary package to the iPhone.
This removes manual IPA selection/export; it does not remove Apple's package
format or code-signing requirements.

## Required input

- A complete arm64 `Husk.app`, already compiled and signed with the same Apple
  team and exact bundle identifier as the installed application.
- An unexpired embedded development or registered-device distribution profile
  which includes this iPhone and authorizes the app's signing certificate.
- A trusted, unlocked USB iPhone; Windows must have working Apple device/USB
  services. The existing optional USB tooling supplies `pymobiledevice3` and
  its `cryptography` dependency.

The ad-hoc signature (`codesign -s -`) in the current CI package is not a device
signing identity. Merely extracting that package to `Husk.app` does not make it
installable. The script deliberately rejects it instead of attempting an
installation or changing the working app.

## Commands

First inspect the signed bundle without writing to a device:

```powershell
.\scripts\deploy_ios_usb.ps1 -AppPath 'C:\path\Husk.app'
```

Update the existing matching app once a valid signed bundle is available:

```powershell
.\scripts\deploy_ios_usb.ps1 -AppPath 'C:\path\Husk.app' -Install
```

Equivalent Python commands are `python scripts/deploy_ios_usb.py --app <path>
--check` and `python scripts/deploy_ios_usb.py --app <path> --install`.

The offline preflight checks the CMS provisioning structure, expiration,
bundle/team entitlements, signer-certificate membership and non-ad-hoc Mach-O
signature structure for embedded executables. These cross-platform checks are
not a replacement for cryptographic code/resource verification: iOS performs
that verification during installation. No private key, Apple account, password,
device identifier or provisioning contents are printed.

Before any upload, the script requires exactly one USB device and verifies that
its identifier belongs to the profile and that the installed app has the same
bundle and signing team. An unknown installed team fails closed. It sends an
`Upgrade` command and never uninstalls the old app, changes its bundle identity
or deletes its Documents. Installation errors are reported without falling back
to uninstall/reinstall.

## Validation

`python -m unittest discover -s tests -p test_usb_deploy.py` exercises generated
synthetic signing/provisioning bundles and a mocked installation boundary. It
covers ad-hoc and missing signatures, embedded framework signatures, malformed
ranges/envelopes, expired or mismatched profiles, unknown/wrong installed teams,
unregistered devices and the upgrade-only path. These tests do not constitute an
actual iPhone installation test.
