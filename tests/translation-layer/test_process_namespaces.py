#!/usr/bin/env python3
"""Integration test against the real DVM/libcore. Set HUSK_DVM_TEST to tools/java-test's built executable.

HUSK_FRAMEWORK names a freshly built framework DEX; TL_ART supplies the runtime libraries. TL_NO_XMEM/TL_AOT_DIR
may select the same AOT runtime as the iPhone build. No graphics, personal APK, certificate or account is required.
"""
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class ProcessNamespaceTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("HUSK_DVM_TEST") and os.environ.get("HUSK_FRAMEWORK"),
                         "Set HUSK_DVM_TEST and HUSK_FRAMEWORK for the native integration test")
    def test_namespace_identity_and_parcelable_transport(self):
        jdk = pathlib.Path(os.environ.get("HUSK_JDK", "/opt/homebrew/opt/openjdk@17")) / "bin"
        d8 = os.environ.get("HUSK_D8", "/opt/homebrew/share/android-commandlinetools/cmdline-tools/latest/lib/r8.jar")
        framework = ROOT / "src/java-framework"
        with tempfile.TemporaryDirectory() as directory:
            work = pathlib.Path(directory)
            stubs, classes, fixture, dex = [work / n for n in ("stubs", "classes", "fixture", "dex")]
            for p in (stubs, classes, fixture, dex):
                p.mkdir()
            def run(args, **kwargs):
                return subprocess.run(list(map(str, args)), check=True, capture_output=True, text=True, **kwargs)
            run([jdk / "javac", "--release", "8", "-nowarn", "-d", stubs,
                 *sorted((framework / "stubs").rglob("*.java"))])
            run([jdk / "javac", "--release", "8", "-nowarn", "-cp", stubs, "-d", classes,
                 *sorted((framework / "src").rglob("*.java")), *sorted((framework / "gen-src").rglob("*.java"))])
            run([jdk / "javac", "--release", "8", "-cp", os.pathsep.join(map(str, (classes, stubs))),
                 "-d", fixture, pathlib.Path(__file__).parent / "fixtures/ProcessNamespaces.java"])
            jar = work / "fixture.jar"
            run([jdk / "jar", "cf", jar, "-C", fixture, "."])
            run([jdk / "java", "-cp", d8, "com.android.tools.r8.D8", "--min-api", "26", "--output", dex, jar])
            env = os.environ.copy()
            env["TL_FRAMEWORK"] = env["HUSK_FRAMEWORK"]
            result = run([os.environ["HUSK_DVM_TEST"], dex / "classes.dex", "ProcessNamespaces", "run"],
                         env=env, timeout=60)
            self.assertIn('RESULT "PASS process namespaces and Parcelable transport"', result.stdout,
                          result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
