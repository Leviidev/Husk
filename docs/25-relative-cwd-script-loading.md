# VHTG script directory lookup

The captured VHTG Dev startup loads only seven scripts, reports missing Item
and FightSkill functions, and stops at `world.LifeSkill` with
`LifeSkill.OnInit` absent. Read-only device inspection found the script tree
and its LifeSkill files present. Missing Lua functions are a consequence to
investigate, not permission to stub game logic.

The native script walker first expands its script path with `g_GetFullPath`.
The existing short Android working-directory alias maps that full path back
to the correct host directory. The walker then changes to that directory,
opens `.` and recursively walks the directory entries. Its ARM64 consumer
reads the guest directory type at offset 18 and name at offset 19, matching
the existing directory bridge.

The path resolver previously searched compatibility roots before honoring an
existing relative path. Resolving `.` therefore found `<data_dir>/.` and
enumerated the app's data root instead of the current script directory.
The engine returned from this walk without loading the expected Lua files.

Existing relative paths now retain normal current-directory semantics before
the compatibility searches. Backslashes and repeated slashes still normalize;
short buffers remain untouched. Missing current-directory entries retain the
existing root search order, including the narrow Tru Tien log-creation fix.
The change does not edit game scripts, APKs, native instructions or settings.

The small reproduction compiled the actual resolver and failed before the
change with `opendir('.') resolved outside the game's script working directory`.
It passes after the change. The regression fixture follows the native
Android-alias `chdir` / `opendir(".")` / recursive directory sequence using
the production resolver and directory wrappers. It also checks current-file
precedence over a root shadow, missing-current-file fallback, normalized
paths, parent enumeration and short buffers. Target platforms exercise the
production guest-dirent conversion; Windows uses its host directory entries
because MinGW does not expose POSIX `d_type`.

Windows focused regression and the existing Tru Tien log-creation test pass.
Portable CI and a fresh VHTG startup on iPhone are still required. Completion
of script loading and entry into the world must be verified from the new
device run; successful compilation alone is not a gameplay result.
