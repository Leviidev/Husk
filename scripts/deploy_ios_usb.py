#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check an already signed Husk.app, then optionally update it over trusted USB.

This script never builds, signs, invokes CI, uninstalls an app or creates an IPA
on the computer. pymobiledevice3 packages the app into IPA bytes internally for
Apple's installation service. iOS performs the final signature verification;
the cross-platform checks here are a structural/provisioning preflight.
"""
import argparse
import asyncio
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import plistlib
import struct
import sys


@dataclass(frozen=True)
class CheckedApp:
    path: Path
    bundle: str
    team: str
    devices: frozenset
    expires: datetime
    build: str
    binaries: int


def _der(data, at=0):
    if at + 2 > len(data):
        raise ValueError('Truncated signing envelope')
    tag, size = data[at], data[at + 1]
    at += 2
    if size & 128:
        n = size & 127
        if not 1 <= n <= 4 or at + n > len(data):
            raise ValueError('Invalid signing envelope length')
        size = int.from_bytes(data[at:at + n], 'big')
        at += n
    end = at + size
    if end > len(data):
        raise ValueError('Truncated signing envelope')
    return tag, data[at:end], end


def _children(data):
    out, at = [], 0
    while at < len(data):
        tag, value, at = _der(data, at)
        out.append((tag, value))
    return out


def profile_payload(data):
    """Extract CMS SignedData's encapsulated plist, without claiming CMS trust."""
    tag, envelope, end = _der(data)
    if tag != 0x30 or end != len(data):
        raise ValueError('Invalid provisioning profile envelope')
    ci = _children(envelope)
    signed_data_oid = bytes.fromhex('2a864886f70d010702')
    if len(ci) != 2 or ci[0] != (6, signed_data_oid) or ci[1][0] != 0xA0:
        raise ValueError('Provisioning profile must contain CMS SignedData')
    tag, signed, end = _der(ci[1][1])
    if tag != 0x30 or end != len(ci[1][1]):
        raise ValueError('Invalid provisioning profile SignedData')
    fields = _children(signed)
    if len(fields) < 4 or fields[2][0] != 0x30:
        raise ValueError('Invalid provisioning profile contents')
    content = _children(fields[2][1])
    if len(content) != 2 or content[0] != (6, bytes.fromhex('2a864886f70d010701')) or content[1][0] != 0xA0:
        raise ValueError('Provisioning profile has no embedded application data')
    tag, payload, end = _der(content[1][1])
    if tag != 4 or end != len(content[1][1]):
        raise ValueError('Unsupported provisioning profile data encoding')
    value = plistlib.loads(payload)
    if not isinstance(value, dict):
        raise ValueError('Invalid provisioning profile plist')
    return value


def _thin_signatures(data):
    if len(data) < 32 or data[:4] != b'\xcf\xfa\xed\xfe':
        raise ValueError('Expected an arm64 iPhone Mach-O executable')
    _, cpu, _, _, commands, command_bytes, _, _ = struct.unpack_from('<8I', data)
    if cpu != 0x100000C or commands > 65536 or 32 + command_bytes > len(data):
        raise ValueError('Invalid or non-iPhone executable')
    at, found = 32, None
    for _ in range(commands):
        if at + 8 > 32 + command_bytes:
            raise ValueError('Truncated Mach-O command')
        cmd, size = struct.unpack_from('<II', data, at)
        if size < 8 or at + size > 32 + command_bytes:
            raise ValueError('Invalid Mach-O command')
        if cmd == 0x1D:
            if size < 16 or found is not None:
                raise ValueError('Invalid code signature command')
            offset, length = struct.unpack_from('<II', data, at + 8)
            if offset < 32 + command_bytes or offset + length > len(data):
                raise ValueError('Invalid code signature range')
            found = data[offset:offset + length]
        at += size
    if found is None or len(found) < 12:
        raise ValueError('An executable has no embedded code signature')
    magic, length, count = struct.unpack_from('>III', found)
    if magic != 0xFADE0CC0 or length > len(found) or count > 65536 or 12 + count * 8 > length:
        raise ValueError('Invalid embedded code signature')
    blobs = {}
    for index in range(count):
        slot, offset = struct.unpack_from('>II', found, 12 + index * 8)
        if offset < 12 + count * 8 or offset + 8 > length or slot in blobs:
            raise ValueError('Invalid code signature slot')
        blob_magic, blob_size = struct.unpack_from('>II', found, offset)
        if blob_size < 8 or offset + blob_size > length:
            raise ValueError('Truncated code signature slot')
        blobs[slot] = (blob_magic, found[offset:offset + blob_size])
    directory = blobs.get(0)
    cms = blobs.get(0x10000)
    if not directory or directory[0] != 0xFADE0C02 or len(directory[1]) < 44:
        raise ValueError('CodeDirectory is missing or malformed')
    if struct.unpack_from('>I', directory[1], 12)[0] & 2:
        raise ValueError('CI ad-hoc signing is not an Apple device signing identity; deployment refused')
    if not cms or cms[0] != 0xFADE0B01 or len(cms[1]) <= 8:
        raise ValueError('An executable has no Apple signing CMS envelope')
    entitlements = None
    if 5 in blobs:
        if blobs[5][0] != 0xFADE7171:
            raise ValueError('Invalid executable entitlements')
        entitlements = plistlib.loads(blobs[5][1][8:])
        if not isinstance(entitlements, dict):
            raise ValueError('Invalid executable entitlements plist')
    return cms[1][8:], entitlements


def signed_binary(data):
    """Return each arm64 slice's CMS and optional XML entitlements."""
    if data[:4] == b'\xcf\xfa\xed\xfe':
        return [_thin_signatures(data)]
    if data[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
        if len(data) < 8:
            raise ValueError('Truncated universal executable')
        count = struct.unpack_from('>I', data, 4)[0]
        wide = data[:4] == b'\xca\xfe\xba\xbf'
        entry_size = 32 if wide else 20
        if not 1 <= count <= 16 or 8 + entry_size * count > len(data):
            raise ValueError('Invalid universal executable')
        out = []
        for i in range(count):
            at = 8 + entry_size * i
            cpu = struct.unpack_from('>I', data, at)[0]
            offset, length = struct.unpack_from('>QQ' if wide else '>II', data, at + 8)
            if cpu != 0x100000C or offset < 8 + entry_size * count or offset + length > len(data):
                raise ValueError('Invalid or non-iPhone executable slice')
            out.append(_thin_signatures(data[offset:offset + length]))
        return out
    raise ValueError('Expected an arm64 iPhone Mach-O executable')


def _certificates(cms):
    from cryptography.hazmat.primitives.serialization import Encoding, pkcs7
    try:
        return {hashlib.sha256(c.public_bytes(Encoding.DER)).digest() for c in pkcs7.load_der_pkcs7_certificates(cms)}
    except ValueError:
        raise ValueError('Invalid executable signing CMS envelope') from None


def _application_matches(value, team, bundle):
    return value == team + '.' + bundle or value == team + '.*'


def _looks_macho(prefix):
    if prefix[:4] == b'\xcf\xfa\xed\xfe':
        return True
    # CAFEBABE is also a Java .class magic. Version bytes are not a Mach-O
    # architecture count; packaged JVM resources must not become executables.
    return (prefix[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf') and len(prefix) >= 12
            and 1 <= int.from_bytes(prefix[4:8], 'big') <= 16
            and int.from_bytes(prefix[8:12], 'big') in (7, 12, 0x1000007, 0x100000C))


def inspect_app(path, now=None):
    app = Path(path).resolve(strict=True)
    if not app.is_dir() or app.suffix.lower() != '.app':
        raise ValueError('Pass an existing signed .app directory; IPA input is not accepted')
    info = plistlib.loads((app / 'Info.plist').read_bytes())
    bundle, executable = info.get('CFBundleIdentifier'), info.get('CFBundleExecutable')
    if not isinstance(bundle, str) or not (bundle == 'com.husk.app' or bundle.startswith('com.husk.app.')):
        raise ValueError('Only a Husk application can be deployed by this script')
    if not isinstance(executable, str) or not executable or Path(executable).name != executable or '/' in executable or '\\' in executable:
        raise ValueError('Invalid app executable name')
    profile = profile_payload((app / 'embedded.mobileprovision').read_bytes())
    expires = profile.get('ExpirationDate')
    now = now or datetime.now(timezone.utc)
    if not isinstance(expires, datetime):
        raise ValueError('Provisioning profile has no expiration date')
    expires = expires.replace(tzinfo=timezone.utc) if expires.tzinfo is None else expires
    if expires <= now:
        raise ValueError('Provisioning profile has expired')
    teams, devices, allowed = profile.get('TeamIdentifier'), profile.get('ProvisionedDevices'), profile.get('Entitlements', {})
    if not isinstance(teams, list) or len(teams) != 1 or not isinstance(teams[0], str):
        raise ValueError('Provisioning profile has no unambiguous signing team')
    team = teams[0]
    if not isinstance(devices, list) or not devices or not all(isinstance(v, str) for v in devices):
        raise ValueError('A registered-device development or ad-hoc distribution profile is required')
    if not _application_matches(allowed.get('application-identifier'), team, bundle):
        raise ValueError('Provisioning profile does not authorize this bundle identifier')
    developer_certs = {hashlib.sha256(c).digest() for c in profile.get('DeveloperCertificates', []) if isinstance(c, bytes)}
    if not developer_certs:
        raise ValueError('Provisioning profile has no allowed signing certificates')
    main = app / executable
    if not main.is_file():
        raise ValueError('The main app executable is missing')
    if not (app / '_CodeSignature/CodeResources').is_file():
        raise ValueError('The signed resource envelope is missing')
    binaries = 0
    for file in app.rglob('*'):
        if file.is_symlink() and not file.resolve().is_relative_to(app):
            raise ValueError('App contains a link outside its bundle')
        if not file.is_file():
            continue
        with file.open('rb') as stream:
            prefix = stream.read(12)
        if not _looks_macho(prefix):
            if file == main:
                raise ValueError('The main executable is not an arm64 Mach-O')
            continue
        for cms, entitlements in signed_binary(file.read_bytes()):
            if not (_certificates(cms) & developer_certs):
                raise ValueError('An executable signer is absent from the provisioning profile')
            if file == main:
                if not entitlements or entitlements.get('application-identifier') != team + '.' + bundle:
                    raise ValueError('Executable application entitlement does not match its bundle/profile')
                if entitlements.get('com.apple.developer.team-identifier') != team:
                    raise ValueError('Executable team entitlement does not match its provisioning profile')
                if entitlements.get('get-task-allow') and not allowed.get('get-task-allow'):
                    raise ValueError('Executable debugging entitlement is not authorized by its profile')
        binaries += 1
    if not binaries:
        raise ValueError('The application has no signed executables')
    return CheckedApp(app, bundle, team, frozenset(v.casefold() for v in devices), expires,
                      str(info.get('HuskBuildCommit', info.get('CFBundleVersion', 'unknown'))), binaries)


def verify_update_target(app, identifier, installed):
    if identifier.casefold() not in app.devices:
        raise ValueError('The connected iPhone is not registered in this provisioning profile')
    current = installed.get(app.bundle) if isinstance(installed, dict) else None
    if not isinstance(current, dict):
        raise ValueError('No existing app has this exact bundle identifier; refusing to create a second Husk')
    entitlements = current.get('Entitlements') or {}
    identity = current.get('ApplicationIdentifier') or entitlements.get('application-identifier')
    team = current.get('TeamIdentifier') or entitlements.get('com.apple.developer.team-identifier')
    if isinstance(team, list) and len(team) == 1:
        team = team[0]
    if not team and isinstance(identity, str) and identity.endswith('.' + app.bundle):
        team = identity[:-(len(app.bundle) + 1)]
    if team != app.team:
        raise ValueError('Installed signing team could not be matched; refusing a potentially incompatible update')


async def update_using_proxy(app, identifier, proxy):
    installed = await proxy.lookup(options={'ReturnAttributes': ['CFBundleIdentifier', 'ApplicationIdentifier', 'TeamIdentifier', 'Entitlements']})
    verify_update_target(app, identifier, installed)
    await proxy.install_from_local(str(app.path), cmd='Upgrade', developer=True)


async def deploy_usb(app):
    from pymobiledevice3 import usbmux
    from pymobiledevice3.lockdown import create_using_usbmux
    from pymobiledevice3.services.installation_proxy import InstallationProxyService
    devices = [d for d in await asyncio.wait_for(usbmux.list_devices(), timeout=10) if d.connection_type == 'USB']
    if len(devices) != 1:
        raise ValueError('Connect exactly one trusted USB iPhone and unlock it')
    device = devices[0]
    async with await create_using_usbmux(serial=device.serial, connection_type='USB', autopair=False) as lockdown:
        async with InstallationProxyService(lockdown) as proxy:
            await update_using_proxy(app, device.serial, proxy)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True, help='An already built and Apple-signed Husk.app directory')
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--check', action='store_true', help='Offline preflight only (default); no device writes')
    action.add_argument('--install', action='store_true', help='Update the existing matching app over trusted USB')
    args = parser.parse_args()
    try:
        app = inspect_app(args.app)
        if args.install:
            asyncio.run(deploy_usb(app))
        print(json.dumps({'action': 'usb-update-complete' if args.install else 'offline-preflight-complete',
                          'build': app.build, 'signedExecutables': app.binaries,
                          'profileExpiresUTC': app.expires.isoformat(), 'buildOrSigningPerformed': False,
                          'localIPAFileCreated': False, 'signatureTrust': 'iOS verifies cryptographic signatures during installation'}))
    except (ValueError, FileNotFoundError, plistlib.InvalidFileException) as error:
        message = str(error) if isinstance(error, ValueError) else 'Required app/signing input is missing or invalid'
        parser.exit(1, 'USB deployment refused: ' + message + '\n')
    except ModuleNotFoundError:
        parser.exit(1, 'USB dependency unavailable; install pymobiledevice3 before using this script\n')
    except Exception as error:
        parser.exit(1, 'USB deployment did not complete (' + type(error).__name__ + '); no uninstall was performed\n')


if __name__ == '__main__':
    main()
