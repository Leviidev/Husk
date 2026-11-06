#!/usr/bin/env python3
"""Build and run real guest-facing temporary-path adapters; no APK or GUI."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--out", type=Path, required=True, help="Directory for build output, fixtures and logs")
args = parser.parse_args()
OUT = args.out.resolve()
OUT.mkdir(parents=True, exist_ok=True)
TASK_TEMP = Path(os.environ.get("TMPDIR", str(OUT / "tmp"))).resolve()
TASK_TEMP.mkdir(parents=True, exist_ok=True)
CHILD_ENV = dict(os.environ, TMPDIR=str(TASK_TEMP), TMP=str(TASK_TEMP), TEMP=str(TASK_TEMP), TL_TMP=str(TASK_TEMP))
binary = OUT / "temp-paths-test"
with (OUT / "build.log").open("w") as log:
    subprocess.run(["bash", ROOT / "tools/regress/build.sh", "tests/translation-layer/temp_paths.c", binary],
                   cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, env=CHILD_ENV)
with (OUT / "results.log").open("w") as log:
    result = subprocess.run([binary, TASK_TEMP], cwd=ROOT, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30, env=CHILD_ENV)
    log.write(result.stdout)
    print(result.stdout, end="")
    result.check_returncode()
print(OUT / "results.log")
