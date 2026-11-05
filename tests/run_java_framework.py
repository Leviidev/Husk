#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument("--jdk", type=Path, required=True)
args = ap.parse_args()
suffix = ".exe" if os.name == "nt" else ""
jar = ROOT / "build/hung-java/framework.jar"
out = ROOT / "build/java-contract-tests"
out.mkdir(exist_ok=True)
sources = list((ROOT / "tests/java-framework").rglob("*.java"))
subprocess.run([str(args.jdk / ("bin/javac" + suffix)), "-cp", str(jar), "-d", str(out)] + [str(p) for p in sources], check=True)
classpath = os.pathsep.join(map(str, (out, jar, ROOT / "build/hung-java/host-stubs.jar")))
subprocess.run([str(args.jdk / ("bin/java" + suffix)), "-ea", "-cp", classpath, "android.app.DialogFragmentTest"], check=True)
subprocess.run([str(args.jdk / ("bin/java" + suffix)), "-ea", "-cp", classpath, "android.view.DisplayCutoutTest"], check=True)
