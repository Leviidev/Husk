# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
from datetime import datetime, timedelta, timezone
from pathlib import Path
import plistlib
import struct
import sys
import tempfile
import unittest

try:
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.serialization import Encoding, pkcs7
    from cryptography.x509.oid import NameOID
    HAS_CRYPTOGRAPHY = True
except ModuleNotFoundError:
    HAS_CRYPTOGRAPHY = False

SPEC = importlib.util.spec_from_file_location('deploy_ios_usb', Path(__file__).resolve().parents[1] / 'scripts/deploy_ios_usb.py')
deploy = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = deploy
SPEC.loader.exec_module(deploy)
NOW = datetime(2026, 11, 16, tzinfo=timezone.utc)
TEAM, BUNDLE, DEVICE = 'TESTTEAM', 'com.husk.app.TESTTEAM', 'TEST-DEVICE'


def signed_cms(data, key, cert, detached=False):
    options = [pkcs7.PKCS7Options.Binary]
    if detached:
        options.append(pkcs7.PKCS7Options.DetachedSignature)
    return pkcs7.PKCS7SignatureBuilder().set_data(data).add_signer(cert, key, hashes.SHA256()).sign(Encoding.DER, options)


def macho(key, cert, flags=0, entitlements=None, with_cms=True):
    directory = struct.pack('>11I', 0xFADE0C02, 44, 0x20100, flags, 0, 0, 0, 0, 0, 0, 0)
    blobs = [(0, directory)]
    if with_cms:
        cms = signed_cms(directory, key, cert, detached=True)
        blobs.append((0x10000, struct.pack('>II', 0xFADE0B01, len(cms) + 8) + cms))
    if entitlements is not None:
        value = plistlib.dumps(entitlements)
        blobs.append((5, struct.pack('>II', 0xFADE7171, len(value) + 8) + value))
    offset = 12 + len(blobs) * 8
    index, content = b'', b''
    for slot, blob in blobs:
        index += struct.pack('>II', slot, offset)
        content += blob
        offset += len(blob)
    signature = struct.pack('>III', 0xFADE0CC0, offset, len(blobs)) + index + content
    header = struct.pack('<8I', 0xFEEDFACF, 0x100000C, 0, 2, 1, 16, 0, 0)
    return header + struct.pack('<4I', 0x1D, 16, 48, len(signature)) + signature


@unittest.skipUnless(HAS_CRYPTOGRAPHY, 'optional USB dependency cryptography is not installed')
class DeploymentTests(unittest.IsolatedAsyncioTestCase):
    @classmethod
    def setUpClass(cls):
        cls.key = ec.generate_private_key(ec.SECP256R1())
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'Synthetic deployment fixture')])
        cls.cert = (x509.CertificateBuilder().subject_name(subject).issuer_name(subject).public_key(cls.key.public_key())
                    .serial_number(12345).not_valid_before(NOW - timedelta(days=1)).not_valid_after(NOW + timedelta(days=30))
                    .sign(cls.key, hashes.SHA256()))

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.app = Path(self.tmp.name) / 'Husk.app'
        self.app.mkdir()
        self.entitlements = {'application-identifier': TEAM + '.' + BUNDLE,
                             'com.apple.developer.team-identifier': TEAM, 'get-task-allow': True}
        self.profile = {'TeamIdentifier': [TEAM], 'ProvisionedDevices': [DEVICE],
                        'ExpirationDate': (NOW + timedelta(days=1)).replace(tzinfo=None),
                        'Entitlements': dict(self.entitlements), 'DeveloperCertificates': [self.cert.public_bytes(Encoding.DER)]}
        (self.app / 'Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier': BUNDLE, 'CFBundleExecutable': 'Husk', 'HuskBuildCommit': 'fixture'}))
        (self.app / '_CodeSignature').mkdir()
        (self.app / '_CodeSignature/CodeResources').write_bytes(plistlib.dumps({'files2': {}}))
        self.write_profile()
        self.write_binary()

    def write_profile(self):
        (self.app / 'embedded.mobileprovision').write_bytes(signed_cms(plistlib.dumps(self.profile), self.key, self.cert))

    def write_binary(self, **kwargs):
        (self.app / 'Husk').write_bytes(macho(self.key, self.cert, entitlements=self.entitlements, **kwargs))

    def check(self):
        return deploy.inspect_app(self.app, now=NOW)

    def test_profile_and_signed_binary_metadata(self):
        checked = self.check()
        self.assertEqual((checked.bundle, checked.team, checked.build, checked.binaries), (BUNDLE, TEAM, 'fixture', 1))
        self.assertEqual(checked.devices, frozenset([DEVICE.casefold()]))
        self.assertEqual(deploy.profile_payload((self.app / 'embedded.mobileprovision').read_bytes()), self.profile)

    def test_rejects_adhoc_or_missing_signing_cms(self):
        self.write_binary(flags=2)
        with self.assertRaisesRegex(ValueError, 'ad-hoc'):
            self.check()
        self.write_binary(with_cms=False)
        with self.assertRaisesRegex(ValueError, 'CMS'):
            self.check()

    def test_rejects_expired_wrong_bundle_and_wrong_team(self):
        self.profile['ExpirationDate'] = (NOW - timedelta(seconds=1)).replace(tzinfo=None)
        self.write_profile()
        with self.assertRaisesRegex(ValueError, 'expired'):
            self.check()
        self.profile['ExpirationDate'] = (NOW + timedelta(days=1)).replace(tzinfo=None)
        self.profile['Entitlements']['application-identifier'] = TEAM + '.another.app'
        self.write_profile()
        with self.assertRaisesRegex(ValueError, 'bundle identifier'):
            self.check()
        self.profile['Entitlements']['application-identifier'] = TEAM + '.*'
        self.write_profile()
        self.entitlements['com.apple.developer.team-identifier'] = 'OTHERTEAM'
        self.write_binary()
        with self.assertRaisesRegex(ValueError, 'team entitlement'):
            self.check()

    def test_rejects_missing_profile_resources_and_bad_command_ranges(self):
        (self.app / 'embedded.mobileprovision').unlink()
        with self.assertRaises(FileNotFoundError):
            self.check()
        self.write_profile()
        (self.app / '_CodeSignature/CodeResources').unlink()
        with self.assertRaisesRegex(ValueError, 'resource envelope'):
            self.check()
        broken = bytearray(macho(self.key, self.cert, entitlements=self.entitlements))
        struct.pack_into('<I', broken, 40, len(broken) + 8)
        with self.assertRaisesRegex(ValueError, 'range'):
            deploy.signed_binary(bytes(broken))
        for data in [b'', b'\x30\x80', b'\x30\x82\xff\xff']:
            with self.assertRaises(ValueError):
                deploy.profile_payload(data)

    def test_rejects_adhoc_nested_framework(self):
        folder = self.app / 'Frameworks/Test.framework'
        folder.mkdir(parents=True)
        (folder / 'Test').write_bytes(macho(self.key, self.cert, flags=2))
        with self.assertRaisesRegex(ValueError, 'ad-hoc'):
            self.check()

    def test_java_resource_is_not_misidentified_as_universal_macho(self):
        (self.app / 'Resource.class').write_bytes(bytes.fromhex('cafebabe0000003400010000'))
        self.assertEqual(self.check().binaries, 1)

    async def test_upgrade_only_after_registered_device_and_identity_match(self):
        checked = self.check()
        class Proxy:
            def __init__(self):
                self.installed = {BUNDLE: {'ApplicationIdentifier': TEAM + '.' + BUNDLE}}
                self.calls = []
            async def lookup(self, **kwargs):
                return self.installed
            async def install_from_local(self, path, **kwargs):
                self.calls.append((path, kwargs))
        proxy = Proxy()
        for identity, installed in [('OTHER-DEVICE', proxy.installed), (DEVICE, {}),
                                    (DEVICE, {BUNDLE: {'TeamIdentifier': 'OTHERTEAM'}}),
                                    (DEVICE, {BUNDLE: {}})]:
            proxy.installed = installed
            with self.assertRaises(ValueError):
                await deploy.update_using_proxy(checked, identity, proxy)
            self.assertFalse(proxy.calls)
        proxy.installed = {BUNDLE: {'ApplicationIdentifier': TEAM + '.' + BUNDLE}}
        await deploy.update_using_proxy(checked, DEVICE, proxy)
        self.assertEqual(proxy.calls, [(str(self.app), {'cmd': 'Upgrade', 'developer': True})])
        self.assertEqual(list(self.app.parent.glob('*.ipa')), [])


if __name__ == '__main__':
    unittest.main()
