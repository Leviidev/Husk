#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the production signer with a temporary test identity on macOS.

No developer credentials or phone are used. CoreTrust verifies the CMS digest
and RSA signature separately from certificate trust, which a test CA cannot pass.
"""
import ctypes
import hashlib
from pathlib import Path
import platform
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def blobs(path):
    data = path.read_bytes()
    cursor = 32
    signature = None
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        command, size = struct.unpack_from('<II', data, cursor)
        if command == 0x1D:
            offset, length = struct.unpack_from('<II', data, cursor + 8)
            signature = data[offset:offset + length]
        cursor += size
    assert signature
    result = {}
    for i in range(struct.unpack_from('>I', signature, 8)[0]):
        kind, offset = struct.unpack_from('>II', signature, 12 + 8 * i)
        length = struct.unpack_from('>I', signature, offset + 4)[0]
        result[kind] = signature[offset:offset + length]
    return data, result


@unittest.skipUnless(platform.system() == 'Darwin', 'requires Apple Security and CoreTrust')
class AppleSignerTest(unittest.TestCase):
    def test_production_cms_passes_coretrust_and_detects_tampering(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            def run(*args, **kwargs):
                return subprocess.run(list(map(str, args)), check=True, capture_output=True, **kwargs)
            run('cc', '-Wno-deprecated-declarations', '-I', ROOT / 'src/translation-layer-next',
                ROOT / 'tools/codesign-test.c', ROOT / 'src/translation-layer-next/husk-tl-codesign.c',
                '-framework', 'Security', '-framework', 'CoreFoundation', '-o', work / 'signer')
            run('cc', '-dynamiclib', '-x', 'c', '-', '-o', work / 'test.dylib', input=b'int test_value(void) { return 42; }\n')
            run('openssl', 'req', '-new', '-x509', '-newkey', 'rsa:2048', '-nodes',
                '-keyout', work / 'key.pem', '-out', work / 'cert.pem', '-days', '2',
                '-subj', '/CN=Husk temporary signer test/OU=ABCDEFGHIJ')
            run('openssl', 'pkcs12', '-export', '-inkey', work / 'key.pem', '-in', work / 'cert.pem',
                '-keypbe', 'PBE-SHA1-3DES', '-certpbe', 'PBE-SHA1-3DES', '-macalg', 'sha1',
                '-out', work / 'test.p12', '-passout', 'pass:temporary-test')
            run(work / 'signer', work / 'test.dylib', work / 'test.p12', 'temporary-test')
            data, signature = blobs(work / 'test.dylib')
            cd = signature[0]
            cms = signature[0x10000][8:]
            hash_offset, _, special, pages, limit = struct.unpack_from('>5I', cd, 16)
            page_bytes = 1 << cd[39]
            for page in range(pages):
                digest = hashlib.sha256(data[page * page_bytes:min((page + 1) * page_bytes, limit)]).digest()
                self.assertEqual(cd[hash_offset + page * 32:hash_offset + (page + 1) * 32], digest)
            self.assertEqual(cd[hash_offset - 64:hash_offset - 32], hashlib.sha256(signature[2]).digest())
            self.assertEqual(special, 2)
            (work / 'cms.der').write_bytes(cms)
            (work / 'directory.bin').write_bytes(cd)
            run('openssl', 'cms', '-verify', '-binary', '-noverify', '-inform', 'DER',
                '-in', work / 'cms.der', '-content', work / 'directory.bin', '-out', work / 'verified.bin')
            self.assertEqual((work / 'verified.bin').read_bytes(), cd)
            library = None
            for candidate in ('/System/Library/PrivateFrameworks/CoreTrust.framework/CoreTrust', '/usr/lib/libCoreTrust.dylib'):
                try:
                    library = ctypes.CDLL(candidate)
                    break
                except OSError:
                    pass
            self.assertIsNotNone(library, 'CoreTrust is required on the Apple QA runner')
            verify = library.CTVerifyAmfiCMS
            verify.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t,
                               ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32),
                               ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
            verify.restype = ctypes.c_int
            def check(digest):
                agility, value, length = ctypes.c_uint32(), ctypes.c_void_p(), ctypes.c_size_t()
                return verify(cms, len(cms), digest, len(digest), 4,
                              ctypes.byref(agility), ctypes.byref(value), ctypes.byref(length))
            digest = hashlib.sha256(cd).digest()
            self.assertEqual(check(digest), 0, 'CoreTrust rejected the production CMS')
            self.assertNotEqual(check(bytes(32)), 0, 'CoreTrust must reject a different CodeDirectory digest')
            print('PASS: production page hashes, OpenSSL CMS and Apple CoreTrust agree; tampering rejected')


if __name__ == '__main__':
    unittest.main()
