#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Test production PKCS#12 parsing against independently generated OpenSSL fixtures.

python3 tests/translation-layer/test_pkcs12.py
P12_TEST_IMPORT=1 also checks Security.framework imports (needs Keychain access).
P12_TEST_SIMULATOR=<booted UDID> checks the same fixtures on iOS Simulator.
Only synthetic identities are created; passwords use stdin, never process arguments.
"""
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest
import base64
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]


class PKCS12Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.openssl = os.environ.get("OPENSSL") or shutil.which("openssl")
        if not cls.openssl or not shutil.which("xcrun"):
            raise unittest.SkipTest("Requires macOS, Xcode and OpenSSL")
        cls.temp = tempfile.TemporaryDirectory(prefix="husk-p12-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = pathlib.Path(cls.temp.name)
        cls.probe = cls.work / "p12-test"
        cls.simulator = os.environ.get("P12_TEST_SIMULATOR")
        command = ["xcrun"]
        if cls.simulator:
            command += ["--sdk", "iphonesimulator"]
        command += ["clang", "-O1", "-g", "-Wno-deprecated-declarations"]
        if os.environ.get("P12_TEST_SANITIZE") and not cls.simulator:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        if cls.simulator:
            command += ["-target", "arm64-apple-ios27.0-simulator"]
        command += [str(ROOT / "tools/p12-password-test.c"), "-framework", "Security",
                    "-framework", "CoreFoundation", "-o", str(cls.probe)]
        subprocess.run(command, check=True, capture_output=True)
        if cls.simulator:
            subprocess.run(["codesign", "--sign", "-", "--timestamp=none", str(cls.probe)], check=True, capture_output=True)
        cls.key, cls.cert = cls.work / "key.pem", cls.work / "cert.pem"
        cls.run_openssl(["req", "-x509", "-newkey", "rsa:2048", "-noenc", "-days", "2",
                         "-subj", "/CN=Husk Synthetic Test/OU=TESTTEAM01", "-keyout", str(cls.key), "-out", str(cls.cert)])
        cls.key.chmod(0o600)
        cls.fixtures = {}
        for name, password, args in [
            ("modern", "husk-test", []),
            ("aes128", "husk-test", ["-keypbe", "AES-128-CBC", "-certpbe", "AES-128-CBC"]),
            ("aes192", "husk-test", ["-keypbe", "AES-192-CBC", "-certpbe", "AES-192-CBC"]),
            ("compat", "husk-test", ["-keypbe", "PBE-SHA1-3DES", "-certpbe", "PBE-SHA1-3DES", "-macalg", "sha1"]),
            ("empty", "", ["-keypbe", "PBE-SHA1-3DES", "-certpbe", "PBE-SHA1-3DES", "-macalg", "sha1"]),
            ("empty-modern", "", []),
            ("whitespace", "  secret with spaces  ", []),
            ("unicode", " пароль+%25 ", []),
            ("no-mac", "husk-test", ["-nomac"]),
            ("cert-only", "husk-test", ["-nokeys"]),
        ]:
            path = cls.work / (name + ".p12")
            cls.run_openssl(["pkcs12", "-export", "-in", str(cls.cert), "-inkey", str(cls.key),
                             "-passout", "stdin", "-out", str(path), *args], password + "\n")
            path.chmod(0o600)
            cls.fixtures[name] = (path, password)
        ec_key, request, ec_cert, ec_p12 = [cls.work / name for name in ("ec-key.pem", "ec.csr", "ec-cert.pem", "ec-chain.p12")]
        cls.run_openssl(["req", "-new", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes",
                         "-subj", "/CN=Husk EC Signing Test/OU=TESTTEAM01", "-keyout", str(ec_key), "-out", str(request)])
        ec_key.chmod(0o600)
        cls.run_openssl(["x509", "-req", "-in", str(request), "-CA", str(cls.cert), "-CAkey", str(cls.key),
                         "-CAcreateserial", "-days", "2", "-out", str(ec_cert)])
        cls.run_openssl(["pkcs12", "-export", "-in", str(ec_cert), "-inkey", str(ec_key), "-certfile", str(cls.cert),
                         "-passout", "stdin", "-out", str(ec_p12)], "ec-password\n")
        cls.fixtures["ec-chain"] = (ec_p12, "ec-password")

    @classmethod
    def run_openssl(cls, args, password=None):
        subprocess.run([cls.openssl, *args], input=password, text=True, check=True, capture_output=True)

    def probe_file(self, path, password, expected_mac, imported=None):
        command = [str(self.probe), str(path), str(expected_mac)]
        if imported is not None:
            command.append(str(int(imported)))
        if self.simulator:
            command = ["xcrun", "simctl", "spawn", self.simulator, *command]
        result = subprocess.run(command, input=password + "\n", text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        # No password, certificate or key bytes may be printed by the probe/importer.
        if password:
            self.assertNotIn(password, result.stdout + result.stderr)
        self.assertNotIn("PRIVATE KEY", result.stdout + result.stderr)
        return result.stdout

    def test_correct_and_wrong_passwords(self):
        for name, (path, password) in self.fixtures.items():
            if name == "no-mac":
                continue
            with self.subTest(name=name):
                self.probe_file(path, password, 1)
                self.probe_file(path, "incorrect", 0)

    def test_whitespace_is_significant(self):
        path, password = self.fixtures["whitespace"]
        self.probe_file(path, password.strip(), 0)

    def test_absent_mac_is_unknown(self):
        path, password = self.fixtures["no-mac"]
        self.probe_file(path, password, -1)
        self.probe_file(path, "incorrect", -1)

    def test_modified_and_truncated(self):
        path, password = self.fixtures["modern"]
        data = path.read_bytes()
        modified = self.work / "modified.p12"
        # OpenSSL's last INTEGER is the MAC iteration count: changing it invalidates the MAC.
        modified.write_bytes(data[:-1] + bytes([data[-1] ^ 1]))
        self.probe_file(modified, password, 0)
        for size in (0, 1, 7, len(data) - 1):
            modified.write_bytes(data[:size])
            self.probe_file(modified, password, -1)

    def test_native_compatibility_export(self):
        for name, (path, password) in self.fixtures.items():
            destination = self.work / (name + "-native-compatible.p12")
            native = "1" if self.simulator or os.environ.get("P12_TEST_IMPORT") else "-"
            if native != "-" and not password and not self.simulator:
                native = "0"  # macOS Security's empty-passphrase limitation also applies to the normalized identity.
            command = [str(self.probe), str(path), "-1" if name == "no-mac" else "1", native, str(destination)]
            if self.simulator:
                command = ["xcrun", "simctl", "spawn", self.simulator, *command]
            result = subprocess.run(command, input=password + "\n", text=True, capture_output=True)
            with self.subTest(name=name):
                if name in ("no-mac", "cert-only"):
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse(destination.exists())
                    continue
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                decoded = []
                for p12 in (path, destination):
                    opened = subprocess.run([self.openssl, "pkcs12", "-nodes", "-in", str(p12), "-passin", "stdin"],
                                            input=(password + "\n").encode(), capture_output=True)
                    self.assertEqual(opened.returncode, 0, opened.stderr)
                    decoded.append(opened.stdout)
                certs = lambda pem: {base64.b64decode(b) for b in re.findall(rb"-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----", pem, re.DOTALL)}
                self.assertEqual(certs(decoded[0]), certs(decoded[1]))
                public = [subprocess.check_output([self.openssl, "pkey", "-pubout", "-outform", "DER"], input=pem, stderr=subprocess.PIPE) for pem in decoded]
                self.assertEqual(public[0], public[1])
        path, _ = self.fixtures["modern"]
        output = self.work / "wrong-password-compatible.p12"
        command = [str(self.probe), str(path), "0", "-", str(output)]
        if self.simulator:
            command = ["xcrun", "simctl", "spawn", self.simulator, *command]
        result = subprocess.run(command, input="incorrect\n", text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(output.exists())

    @unittest.skipUnless(os.environ.get("P12_TEST_IMPORT") or os.environ.get("P12_TEST_SIMULATOR"), "Keychain access not requested")
    def test_native_import(self):
        for name, (path, password) in self.fixtures.items():
            with self.subTest(name=name):
                # macOS Security rejects these empty-password exports; iOS accepts them.
                imported = name not in ("cert-only", "no-mac", "aes128", "aes192") and (bool(password) or bool(self.simulator))
                result = self.probe_file(path, password, -1 if name == "no-mac" else 1, imported=imported)
                if name == "cert-only":
                    self.assertIn("ERROR=", result)
        path, password = self.fixtures["modern"]
        self.probe_file(path, "incorrect", 0, imported=False)

    @unittest.skipUnless(os.environ.get("P12_TEST_SIMULATOR"), "Requires iOS Simulator for UIKit-linked Swift code")
    def test_swift_passwords_and_errors(self):
        sdk = subprocess.check_output(["xcrun", "--sdk", "iphonesimulator", "--show-sdk-path"], text=True).strip()
        target = ["-target", "arm64-apple-ios27.0-simulator", "-isysroot", sdk]
        source = ROOT / "src/translation-layer-next/husk-tl-codesign.c"
        native = self.work / "codesign.o"
        wrapper = self.work / "native-wrapper.c"
        wrapper.write_text('''#include <Security/Security.h>
#include <stdlib.h>
static OSStatus fixture_import(CFDataRef d, CFDictionaryRef o, CFArrayRef *r);
#define SecPKCS12Import fixture_import
#include "''' + str(source) + '''"
#undef SecPKCS12Import
static OSStatus fixture_import(CFDataRef d, CFDictionaryRef o, CFArrayRef *r) {
    static CFDataRef original;
    if (getenv("P12_TEST_REJECT_ORIGINAL")) {
        if (!original) original = CFDataCreateCopy(NULL, d);
        if (CFEqual(d, original)) { *r = NULL; return errSecDecode; }
    }
    return SecPKCS12Import(d, o, r);
}
''')
        subprocess.run(["xcrun", "clang", *target, "-Wno-deprecated-declarations", "-c", str(wrapper), "-o", str(native)], check=True, capture_output=True)
        logger = self.work / "logger.c"
        logger.write_text('void tl_log_line(const char *f, ...) {}\n')
        log_object = self.work / "logger.o"
        subprocess.run(["xcrun", "clang", *target, "-c", str(logger), "-o", str(log_object)], check=True, capture_output=True)
        # Compile the actual Swift source unchanged, appending same-file access to its
        # private input/error helpers. Keychain storage is unchanged and needs an app host.
        harness = self.work / "PasswordProbe.swift"
        production = (ROOT / "src/app/Husk/SigningCertificate.swift").read_text()
        harness.write_text(production + '''
        enum HuskLog { nonisolated static func log(_ category: String, _ message: String) {} }
        extension SigningCertificate {
            static func verifyFixtureInputs(_ p12: Data, _ password: String) {
                for password in ["  secret with spaces  ", " пароль+%25 ", "", "a+b", "%25"] {
                    let values = passwordCandidates(password)
                    precondition(values.first == password, "The exact password must be tried first")
                    precondition(Set(values).count == values.count, "Duplicate password attempts")
                    precondition(values.contains(""), "Missing store compatibility fallback")
                }
                precondition(passwordCandidates("p%2Bq").contains("p+q"))
                precondition(OpenFailure(detail: "status -25293", mac: 1).message.contains("password was verified"))
                precondition(OpenFailure(detail: "status -25293", mac: 0).message.contains("integrity check"))
                precondition(!OpenFailure(detail: "status -25293", mac: -1).message.contains("did not match"))
                let opened = try! open(p12, password).get()
                precondition(opened.p12 != p12, "Native rejection must trigger compatibility import")
                precondition(opened.info.teamID == "TESTTEAM01")
                let reopened = try! open(opened.p12, opened.password).get()
                precondition(reopened.p12 == opened.p12 && reopened.info.tag == opened.info.tag,
                             "Reopening the saved compatible container must preserve its signer tag")
            }
        }
        @main struct PasswordProbe {
            @MainActor static func main() {
                let p12 = try! Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
                SigningCertificate.verifyFixtureInputs(p12, readLine() ?? "")
                print("SWIFT_PASSWORD_CHECKS_OK")
            }
        }
''')
        swift_probe = self.work / "swift-password-test"
        compiled = subprocess.run(["xcrun", "swiftc", "-parse-as-library", "-swift-version", "5", "-sdk", sdk,
                                   "-target", "arm64-apple-ios27.0-simulator", "-import-objc-header", str(source.with_suffix(".h")),
                                   str(harness), str(native), str(log_object), "-framework", "Security", "-framework", "CoreFoundation",
                                   "-o", str(swift_probe)], text=True, capture_output=True)
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        subprocess.run(["codesign", "--sign", "-", "--timestamp=none", str(swift_probe)], check=True, capture_output=True)
        environment = dict(os.environ, SIMCTL_CHILD_P12_TEST_REJECT_ORIGINAL="1")
        path, password = self.fixtures["modern"]
        result = subprocess.run(["xcrun", "simctl", "spawn", self.simulator, str(swift_probe), str(path)],
                                input=password + "\n", env=environment, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("SWIFT_PASSWORD_CHECKS_OK", result.stdout)


if __name__ == "__main__":
    unittest.main()
