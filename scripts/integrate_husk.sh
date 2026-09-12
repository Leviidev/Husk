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

# region.c is patched in place rather than copied, so a re-extracted QEMU tree
# would silently lose it -- and the symptom (QEMU falling back to an RWX mmap and
# failing with EPERM) does not obviously point back here. Apply it idempotently.
if grep -q "alloc_code_gen_buffer_splitwx_husk_ios" "$Q/tcg/region.c"; then
    echo "[skip] tcg/region.c already patched"
else
    echo "[patch] tcg/region.c <- husk-qemu-ios-jit.patch"
    patch -p0 -d / --silent < "$HUSK_ROOT/patches/husk-qemu-ios-jit.patch" 2>/dev/null \
      || patch -p1 -d "$Q" --silent < "$HUSK_ROOT/patches/husk-qemu-ios-jit.patch" 2>/dev/null \
      || { echo "  FAILED to apply region.c patch" >&2; exit 1; }
fi

echo "[cp  ] display bridge -> ui/"
cp "$HUSK_ROOT/src/ios-jit/husk-display.c" \
   "$HUSK_ROOT/src/ios-jit/husk-display.h" "$Q/ui/"

echo "[cp  ] GL display bridge -> ui/"
cp "$HUSK_ROOT/src/ios-jit/husk-display-gl.c" \
   "$HUSK_ROOT/src/ios-jit/husk-display-gl.h" "$Q/ui/"

echo "[cp  ] balloon control -> system/"
cp "$HUSK_ROOT/src/ios-jit/husk-balloon.c" \
   "$HUSK_ROOT/src/ios-jit/husk-balloon.h" "$Q/system/"

echo "[cp  ] snapshot control -> system/"
cp "$HUSK_ROOT/src/ios-jit/husk-snapshot.c" \
   "$HUSK_ROOT/src/ios-jit/husk-snapshot.h" "$Q/system/"

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

# husk-display-gl.c only builds with CONFIG_OPENGL, and it goes in the same
# block as QEMU's own shader.c/console-gl.c so it inherits that condition.
s = p.read_text()
if "husk-display-gl.c" not in s:
    old_gl = "if_true: files('shader.c', 'console-gl.c'))"
    assert old_gl in s, "ui/meson.build GL block shape changed"
    s = s.replace(old_gl, "if_true: files('shader.c', 'console-gl.c', 'husk-display-gl.c'))", 1)
    p.write_text(s)
    print("  ui/meson.build: added husk-display-gl.c (CONFIG_OPENGL)")
else:
    print("  ui/meson.build: GL bridge already wired")

# system/meson.build: balloon control. It lives here rather than in ui/ because
# it calls qmp_balloon(), which system/balloon.c defines.
p = q / "system/meson.build"
s = p.read_text()
if "husk-balloon.c" not in s:
    old = "system_ss.add(files(\n"
    assert old in s, "system/meson.build shape changed"
    s = s.replace(old, old + "  'husk-balloon.c',\n", 1)
    p.write_text(s)
    print("  system/meson.build: added husk-balloon.c")
else:
    print("  system/meson.build: already wired")

s = p.read_text()
if "husk-snapshot.c" not in s:
    old_s = "system_ss.add(files(\n"
    assert old_s in s
    s = s.replace(old_s, old_s + "  'husk-snapshot.c',\n", 1)
    p.write_text(s)
    print("  system/meson.build: added husk-snapshot.c")
else:
    print("  system/meson.build: snapshot already wired")
PY

python3 - "$Q" <<'PY2'
import pathlib, sys
q = pathlib.Path(sys.argv[1])

# system/qemu.symbols is the authoritative export list for the shared-lib build.
# On darwin meson seds it into an ld64 -exported_symbols_list, so a symbol absent
# from here stays local no matter what visibility attribute it carries.
p = q / "system/qemu.symbols"
s = p.read_text()

# Drop husk symbols that no longer exist. Adding without pruning leaves stale
# names in the export list, and the link then fails with "Undefined symbols"
# for a function that was simply renamed.
import re as _re
_live = None
wanted = [
    "husk_display_init",
    "husk_display_lock_frame",
    "husk_display_unlock_frame",
    "husk_display_sequence",
    "husk_display_send_pointer",
    "husk_display_request_update",
    "husk_display_send_key",
    "husk_display_gl_create",
    "husk_display_gl_bind",
    "husk_display_gl_probe",
    "husk_display_gl_early",
    "husk_display_gl_frames",
    "husk_snapshot_save",
    "husk_snapshot_load_at_startup",
    "husk_balloon_set_bytes",
    "husk_ios_jit_prewarm",
    "husk_ios_jit_install_trap_handler",
    "husk_ios_jit_is_available",
    "husk_ios_jit_detach",
    "husk_ios_jit_log_footprint",
    "husk_ios_available_memory",
]
_present = _re.findall(r"^\s*(husk_\w+);", s, _re.M)
for _stale in [n for n in _present if n not in wanted]:
    s = _re.sub(r"^\s*" + _stale + r";\n", "", s, flags=_re.M)
    print("  system/qemu.symbols: pruned stale " + _stale)

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
