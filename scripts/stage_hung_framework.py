#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Stage the Java job's tested DEX files without replacing the other APEX resources."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def stage(source, root=ROOT):
    metadata = json.loads((source / "java-framework-build.json").read_text(encoding="utf-8"))
    names = {"husk-framework.dex", "husk-framework-core.dex"}
    if set(metadata["files"]) != names:
        raise ValueError("Expected exactly the full and core Java framework")
    for name in names:
        data = (source / name).read_bytes()
        item = metadata["files"][name]
        if not data.startswith(b"dex\n") or len(data) != item["bytes"] or hashlib.sha256(data).hexdigest() != item["sha256"]:
            raise ValueError("Java framework artifact mismatch: " + name)
    provenance = root / "build/support-provenance.json"
    record = json.loads(provenance.read_text(encoding="utf-8"))
    for name in names:
        shutil.copyfile(source / name, root / "build/java-runtime" / name)
    record["javaFramework"] = {"source": "this checkout, tested in the java-framework job", **metadata}
    provenance.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print("Staged tested full/core Java framework from this revision")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source", type=Path)
    stage(ap.parse_args().source)
