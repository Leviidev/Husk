#!/bin/bash
# Copy Husk's own sources into the QEMU tree and wire them into its meson build.
# Idempotent: safe to re-run after editing anything in src/ios-jit/.
set -euo pipefail
HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q="$HUSK_ROOT/third_party/build/qemu-10.0.12-utm"

[ -d "$Q" ] || { echo "QEMU source tree missing; run fetch_sources.sh first" >&2; exit 1; }

echo "[cp  ] JIT substrate -> tcg/"
cp "$HUSK_ROOT/src/ios-jit/husk-ios-jit.c" \
   "$HUSK_ROOT/src/ios-jit/husk-ios-jit.h" \
   "$HUSK_ROOT/src/ios-jit/husk-brk.S" "$Q/tcg/"

echo "[cp  ] display bridge -> ui/"
cp "$HUSK_ROOT/src/ios-jit/husk-display.c" \
   "$HUSK_ROOT/src/ios-jit/husk-display.h" "$Q/ui/"

python3 - "$Q" <<'PY'
import pathlib, sys
q = pathlib.Path(sys.argv[1])

# tcg/meson.build: husk-ios-jit.c + husk-brk.S alongside region.c
p = q / "tcg/meson.build"
s = p.read_text()
if "husk-brk.S" not in s:
    old = "tcg_ss.add(files(\n"
    assert old in s
    # husk-ios-jit.c may already be present from an earlier run
    s = s.replace(old, old + "  'husk-brk.S',\n", 1)
    if "husk-ios-jit.c" not in s:
        s = s.replace(old, old + "  'husk-ios-jit.c',\n", 1)
    p.write_text(s)
    print("  tcg/meson.build: added husk sources")
else:
    print("  tcg/meson.build: already wired")

# ui/meson.build: the display bridge
p = q / "ui/meson.build"
s = p.read_text()
if "husk-display.c" not in s:
    old = "system_ss.add(files(\n"
    assert old in s, "ui/meson.build shape changed"
    s = s.replace(old, old + "  'husk-display.c',\n", 1)
    p.write_text(s)
    print("  ui/meson.build: added husk-display.c")
else:
    print("  ui/meson.build: already wired")
PY

python3 - "$Q" <<'PY2'
import pathlib, sys
q = pathlib.Path(sys.argv[1])

# system/qemu.symbols is the authoritative export list for the shared-lib build.
# On darwin meson seds it into an ld64 -exported_symbols_list, so a symbol absent
# from here stays local no matter what visibility attribute it carries.
p = q / "system/qemu.symbols"
s = p.read_text()
wanted = [
    "husk_display_init",
    "husk_display_lock_frame",
    "husk_display_unlock_frame",
    "husk_display_sequence",
    "husk_display_send_pointer",
    "husk_display_request_update",
    "husk_ios_jit_install_trap_handler",
    "husk_ios_jit_is_available",
    "husk_ios_jit_detach",
    "husk_ios_jit_log_footprint",
]
missing = [w for w in wanted if f"  {w};" not in s]
if missing:
    assert s.lstrip().startswith("{"), "qemu.symbols shape changed"
    idx = s.index("{") + 1
    s = s[:idx] + "\n" + "".join(f"  {w};\n" for w in missing) + s[idx:].lstrip("\n")
    p.write_text(s)
    print(f"  system/qemu.symbols: exported {len(missing)} husk symbols")
else:
    print("  system/qemu.symbols: already exported")
PY2

echo "[ok  ] integrated"
