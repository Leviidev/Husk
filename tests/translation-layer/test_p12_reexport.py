#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic-only tests for the local P12 compatibility conversion tool."""
import importlib.util
import os
import pathlib
import stat
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("reexport_p12", ROOT / "tools/reexport-p12.py")
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)


class ReexportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.openssl = converter.openssl_path()
        cls.temp = tempfile.TemporaryDirectory(prefix="husk-p12-convert-test-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = pathlib.Path(cls.temp.name)
        cls.password = " пароль +%25 "
        cls.key, cls.cert = cls.work / "key.pem", cls.work / "cert.pem"
        cls.root_key, cls.root_cert = cls.work / "root-key.pem", cls.work / "root.pem"
        cls.command(["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
                     "-subj", "/CN=Husk Synthetic Root", "-keyout", str(cls.root_key), "-out", str(cls.root_cert)])
        request = cls.work / "request.pem"
        cls.command(["req", "-new", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes",
                     "-subj", "/CN=Husk Synthetic Signing Test/OU=TESTTEAM01", "-keyout", str(cls.key), "-out", str(request)])
        for key in (cls.key, cls.root_key):
            key.chmod(0o600)
        cls.command(["x509", "-req", "-in", str(request), "-CA", str(cls.root_cert), "-CAkey", str(cls.root_key),
                     "-CAcreateserial", "-days", "2", "-out", str(cls.cert)])
        cls.original = cls.work / "original.p12"
        cls.command(["pkcs12", "-export", "-in", str(cls.cert), "-inkey", str(cls.key), "-certfile", str(cls.root_cert),
                     "-out", str(cls.original), "-passout", "stdin"], cls.password + "\n")
        cls.original.chmod(0o600)

    @classmethod
    def command(cls, args, password=None):
        return subprocess.run([cls.openssl, *args], input=password, text=True, check=True, capture_output=True)

    def test_reexport_preserves_identity_and_chain(self):
        destination = self.work / "compatible.p12"
        old_bytes = self.original.read_bytes()
        before = set(self.work.iterdir())
        converter.reexport(self.original, destination, self.password, self.openssl)
        self.assertEqual(self.original.read_bytes(), old_bytes)
        self.assertEqual(set(self.work.iterdir()) - before, {destination})
        self.assertEqual(stat.S_IMODE(destination.stat().st_mode), 0o600)
        info = self.command(["pkcs12", "-info", "-noout", "-in", str(destination), "-passin", "stdin"], self.password + "\n")
        self.assertIn("MAC: sha1", info.stderr)
        self.assertIn("pbeWithSHA1And3-KeyTripleDES-CBC", info.stderr)
        self.assertNotIn("PBES2", info.stderr)
        for path in (self.original, destination):
            decoded = converter.run_openssl(self.openssl, ["pkcs12", "-in", str(path), "-nodes"],
                                             password=self.password, pass_option="-passin")
            self.assertEqual(len(converter.certificates(decoded)), 2)

    def test_wrong_password_does_not_create_output(self):
        destination = self.work / "bad-password.p12"
        with self.assertRaises(converter.ReexportError):
            converter.reexport(self.original, destination, "incorrect", self.openssl)
        self.assertFalse(destination.exists())

    def test_rsa_identity(self):
        source, destination = self.work / "rsa-original.p12", self.work / "rsa-compatible.p12"
        self.command(["pkcs12", "-export", "-in", str(self.root_cert), "-inkey", str(self.root_key),
                     "-out", str(source), "-passout", "stdin"], self.password + "\n")
        converter.reexport(source, destination, self.password, self.openssl)
        self.assertTrue(destination.is_file())

    def test_existing_files_are_preserved(self):
        destination = self.work / "existing.p12"
        destination.write_bytes(b"unchanged")
        with self.assertRaises(converter.ReexportError):
            converter.reexport(self.original, destination, self.password, self.openssl)
        self.assertEqual(destination.read_bytes(), b"unchanged")
        old_bytes = self.original.read_bytes()
        with self.assertRaises(converter.ReexportError):
            converter.reexport(self.original, self.original, self.password, self.openssl)
        self.assertEqual(self.original.read_bytes(), old_bytes)

    def test_missing_key_does_not_create_output(self):
        source, destination = self.work / "cert-only.p12", self.work / "no-key.p12"
        self.command(["pkcs12", "-export", "-nokeys", "-in", str(self.cert), "-out", str(source), "-passout", "stdin"],
                     self.password + "\n")
        with self.assertRaises(converter.ReexportError):
            converter.reexport(source, destination, self.password, self.openssl)
        self.assertFalse(destination.exists())

    def test_password_and_private_key_are_not_written_to_arguments_or_temp_file(self):
        real_run = subprocess.run
        public_files = []

        def checked_run(command, **kwargs):
            self.assertNotIn(self.password, " ".join(command))
            self.assertNotIn("pass:", " ".join(command))
            if "-export" in command:
                public_file = pathlib.Path(command[command.index("-in") + 1])
                self.assertNotIn(b"PRIVATE KEY", public_file.read_bytes())
                public_files.append(public_file)
            return real_run(command, **kwargs)

        with mock.patch.object(converter.subprocess, "run", side_effect=checked_run):
            converter.reexport(self.original, self.work / "privacy-test.p12", self.password, self.openssl)
        self.assertTrue(public_files)
        self.assertTrue(all(not path.exists() for path in public_files))

    @unittest.skipUnless(os.environ.get("P12_TEST_IMPORT"), "Requires Keychain access for native import")
    def test_converted_identity_imports_in_production_code(self):
        probe = self.work / "p12-test"
        subprocess.run(["xcrun", "clang", "-O1", "-Wno-deprecated-declarations", str(ROOT / "tools/p12-password-test.c"),
                        "-framework", "Security", "-framework", "CoreFoundation", "-o", str(probe)], check=True, capture_output=True)
        destination = self.work / "native-test.p12"
        converter.reexport(self.original, destination, self.password, self.openssl)
        result = subprocess.run([str(probe), str(destination), "1", "1"], input=self.password + "\n", text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("IMPORT=1", result.stdout)
        self.assertNotIn(self.password, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
