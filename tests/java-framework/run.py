#!/usr/bin/env python3
"""Run framework layout/input regressions on a host JDK, without Android or an APK."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
framework = root / "src/java-framework"
jdk = os.environ.get("HUSK_JDK") or os.environ.get("JAVA_HOME")
javac = str(Path(jdk) / "bin/javac") if jdk else shutil.which("javac")
java = str(Path(jdk) / "bin/java") if jdk else shutil.which("java")
if not javac or not java:
    raise SystemExit("Set HUSK_JDK to a JDK installation (JDK 21 is supported).")

with tempfile.TemporaryDirectory(prefix="husk-framework-tests-") as tmp:
    out = Path(tmp)
    stubs, classes = out / "stubs", out / "classes"
    stubs.mkdir()
    classes.mkdir()
    # javac argument files avoid the OS argument limit for the generated framework.
    def compile_sources(name, sources, options):
        args = out / (name + ".args")
        args.write_text("\n".join('"' + str(p).replace("\\", "\\\\").replace('"', '\\"') + '"' for p in sources))
        subprocess.run([javac, "-J-Djava.io.tmpdir=" + str(out), "-nowarn", *options, "@" + str(args)], check=True)

    compile_sources("stubs", sorted((framework / "stubs").rglob("*.java")),
                    ["--release", "8", "-d", str(stubs)])
    compile_sources("framework", sorted((framework / "src").rglob("*.java"))
                    + sorted((framework / "gen-src").rglob("*.java")),
                    ["--release", "8", "-cp", str(stubs), "-d", str(classes)])
    tests = sorted(Path(__file__).parent.glob("*Test.java"))
    classpath = os.pathsep.join([str(classes), str(stubs)])
    compile_sources("tests", tests, ["-cp", classpath, "-d", str(classes)])
    for test in tests:
        subprocess.run([java, "-Djava.io.tmpdir=" + str(out), "-cp", classpath, test.stem], check=True)
