#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile this checkout's Java framework using explicit JDK and R8 inputs.

Leaves libcore/APEX resources intact. Outputs full/core DEX and the class JAR
for JVM contract tests. R8 is hash-verified by the calling CI workflow.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def build(jdk, r8, output):
    here = ROOT / "src/java-framework"
    work = ROOT / "build/hung-java"
    work.mkdir(parents=True, exist_ok=True)
    compilation = work / ("compile-" + uuid.uuid4().hex)
    compilation.mkdir()
    suffix = ".exe" if (jdk / "bin/javac.exe").is_file() else ""
    javac, java = [str(jdk / ("bin/" + name + suffix)) for name in ("javac", "java")]
    def compile_sources(name, sources, classpath=None):
        dest = compilation / name
        dest.mkdir(exist_ok=True)
        # javac argument files preserve spaces in a checkout path on Windows.
        argfile = work / (name + ".args")
        args = ["--release", "8", "-encoding", "UTF-8", "-nowarn", "-d", dest.as_posix()]
        if classpath: args += ["-cp", classpath.as_posix()]
        args += [p.as_posix() for p in sorted(sources)]
        argfile.write_text("\n".join('"' + a.replace('"', '\\"') + '"' for a in args), encoding="utf-8")
        subprocess.run([javac, "@" + str(argfile)], check=True)
        return dest
    stubs = compile_sources("stubs", (here / "stubs").rglob("*.java"))
    with zipfile.ZipFile(work / "host-stubs.jar", "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(stubs.rglob("*.class")): z.write(p, p.relative_to(stubs).as_posix())
    classes = compile_sources("classes", list((here / "src").rglob("*.java")) + list((here / "gen-src").rglob("*.java")), stubs)
    output.mkdir(parents=True, exist_ok=True)
    evidence = {}
    for core in (False, True):
        jar = work / ("framework-core.jar" if core else "framework.jar")
        with zipfile.ZipFile(jar, "w", zipfile.ZIP_DEFLATED) as z:
            for p in sorted(classes.rglob("*.class")):
                rel = p.relative_to(classes).as_posix()
                outer = rel.removesuffix(".class").split("$", 1)[0]
                if not core or (here / "src" / (outer + ".java")).is_file(): z.write(p, rel)
        dexdir = work / ("dex-core" if core else "dex-full")
        dexdir.mkdir(exist_ok=True)
        subprocess.run([java, "-Xmx2g", "-cp", str(r8), "com.android.tools.r8.D8", "--min-api", "26", "--output", str(dexdir), str(jar)], check=True)
        name = "husk-framework-core.dex" if core else "husk-framework.dex"
        data = (dexdir / "classes.dex").read_bytes()
        (output / name).write_bytes(data)
        evidence[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    provenance = ROOT / "build/support-provenance.json"
    subprocess.run([java, "-version"], check=True)
    (output / "java-framework-build.json").write_text(json.dumps({"files": evidence,
        "r8SHA256": hashlib.file_digest(r8.open("rb"), "sha256").hexdigest()}, indent=2) + "\n", encoding="utf-8")
    if provenance.is_file() and output.resolve() == (ROOT / "build/java-runtime").resolve():
        document = json.loads(provenance.read_text(encoding="utf-8"))
        document["javaFramework"] = {"source": "this checkout", "files": evidence, "r8SHA256": hashlib.file_digest(r8.open("rb"), "sha256").hexdigest()}
        provenance.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--jdk", type=Path, required=True)
    ap.add_argument("--r8", type=Path, required=True)
    ap.add_argument("--output", type=Path, default=ROOT / "build/java-runtime")
    args = ap.parse_args()
    build(args.jdk, args.r8, args.output)
