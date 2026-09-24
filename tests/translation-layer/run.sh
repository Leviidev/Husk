#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Tests for src/translation-layer, runnable on a Linux host.
#
#   1. the page planner, on layouts worked out by hand
#   2. the APK scanner, on real arm64 Android libraries built here by clang
#   3. the device checks, as arm64 code under qemu-user -- which proves the
#      generated code and the check logic, not what iOS will answer. Only a
#      phone can answer that; Settings > Experimental runs the same code there.
#
# Needs clang and lld (with the Android targets, which upstream clang has),
# python3 and zlib's headers. Step 3 also needs aarch64-linux-gnu-gcc and
# qemu-aarch64, and is skipped without them.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../../src/translation-layer" && pwd)
OUT=${OUT:-$(mktemp -d)}
mkdir -p "$OUT"
CFLAGS="-std=gnu11 -O2 -g -Wall -Wextra -Werror -I$SRC"

echo "== page planner"
cc $CFLAGS -o "$OUT/plan_test" "$HERE/plan_test.c" "$SRC/husk-tl-elf.c"
"$OUT/plan_test"

echo "== scanner"
cc $CFLAGS -fsanitize=address,undefined -o "$OUT/scan_cli" "$HERE/scan_cli.c" \
    "$SRC"/husk-tl-json.c "$SRC"/husk-tl-zip.c "$SRC"/husk-tl-elf.c \
    "$SRC"/husk-tl-scan.c "$SRC"/husk-tl-probe.c -lz -lpthread
python3 "$HERE/test_scan.py" "$OUT" "$OUT/scan_cli"

echo "== device checks, arm64 under qemu-user"
if ! command -v aarch64-linux-gnu-gcc >/dev/null || ! command -v qemu-aarch64 >/dev/null; then
    echo "skipped: needs aarch64-linux-gnu-gcc and qemu-aarch64"
    exit 0
fi
# The checks do not touch ZIP files, so the scanner's zlib is stubbed out
# rather than cross-built.
cat > "$OUT/noscan.c" <<'EOF'
#include <stddef.h>
char *husk_tl_scan(const char *const *p, int n) { (void)p; (void)n; return NULL; }
void *husk_tl_read_entry(const char *a, const char *b, size_t l, size_t *o)
{ (void)a; (void)b; (void)l; *o = 0; return NULL; }
EOF
aarch64-linux-gnu-gcc $CFLAGS -static -o "$OUT/checks_arm64" "$HERE/scan_cli.c" \
    "$OUT/noscan.c" "$SRC"/husk-tl-json.c "$SRC"/husk-tl-probe.c \
    -Wl,--defsym=husk_tl_free=free -lpthread
python3 - "$OUT/checks_arm64" <<'EOF'
import json, subprocess, sys
cli = sys.argv[1]
failures = 0
def run(*args):
    out = subprocess.run(["qemu-aarch64", cli, "checks", *args], check=True,
                         capture_output=True, text=True).stdout
    return {c["id"]: c for c in json.loads(out)}
def check(what, cond, detail=""):
    global failures
    print(("ok   " if cond else "FAIL ") + what + ("" if cond else f" {detail}"))
    failures += 0 if cond else 1

c = run()
check("page size reported", c["page"]["status"] == "info", c["page"])
# glibc keeps its own thread pointer in TPIDR_EL0 -- the check must see that
# and refuse to touch it, which is also the right answer on any system that
# owns the register.
check("tpidr: owned by glibc, left alone", c["tpidr"]["status"] == "fail"
      and "Already in use" in c["tpidr"]["detail"], c["tpidr"])
check("x18: Apple-only", c["x18"]["status"] == "skip", c["x18"])
check("carve: generated code ran beside carved data", c["carve"]["status"] == "pass", c["carve"])
check("place: code placed before data ran", c["place"]["status"] == "pass", c["place"])
check("dualmap: honestly not measured", c["dualmap"]["status"] == "skip", c["dualmap"])

c = run("noexec")
check("noexec: nothing executed", c["carve"]["status"] == "skip"
      and c["place"]["status"] == "skip", c)

print("all passed" if failures == 0 else f"{failures} FAILED")
sys.exit(1 if failures else 0)
EOF
