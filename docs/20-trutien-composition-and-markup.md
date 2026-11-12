# Tru Tien composition and markup investigation — 12 November 2026

The supplied video shows literal color controls such as `^FFCB4A`. The user
also reports other caret/hexadecimal sequences, selective scene flicker and
stutter. These observations remain device acceptance criteria, not passing
results inferred from compilation.

## Corrected runtime contracts

An Android SurfaceView's composition opacity is independent of its EGL
buffer's alpha channel. The default SurfaceView is OPAQUE, but Husk previously
copied the game's sampled alpha into the host output unchanged. The native
window now reads an explicit, volatile composition-format field from its
owning SurfaceView. OPAQUE and the RGB-only formats force output alpha to one;
RGBA, transparent/translucent, TextureView and unknown windows preserve alpha.
The flag is published under the same lock as the frame, dimensions, viewport
and sequence number. Failed CPU allocation retains the entire previous frame,
including its opacity. The Java overlay shader always preserves its alpha.
No game texture bytes, resolution, rendering settings or player limits change.

Destroying a replaced surface no longer clears a newer surface's published
frame. Ownership changes only after successful publication. A failed CPU
fallback therefore cannot transfer ownership away from a retained GPU frame.
Clearing the current owner remains nonblocking under the handoff lock.

Repeated empty Java UI publications no longer wake the compositor. Visible
animations are still published each time. A visible-to-empty transition clears
once. Pixels, dimensions and pending visibility are borrowed coherently, and
displayed visibility changes after that borrow completes. New sessions clear
the old overlay only after the session gate accepts a start; a closing session
with outstanding guest workers retains its frame and rejects the new start.

Bionic's pthread_once control values are 0=not started, 1=initializing and
2=complete. Husk had reversed the last two values. The narrow correction
preserves the existing atomic synchronization. The examined SDL caller does
not have an inline fast path, so this independent ABI defect is not established
as the cause of this game's flicker.

## Text evidence and remaining work

Read-only inspection found valid Vietnamese strings in the game's resource
packages. The installed APK updated between the older gameplay capture and
the new capture; analysis must use the matching native version. AOT cache
entries are separated by library hashes, and the loader validates those
hashes. Different build dates alone do not establish stale AOT code.

The game's own formatter reports a packed UTF-16 string passed to a 32-bit
wide-string conversion in AWString::Format. This warning originates inside
the game's native library, not in Husk's printf implementation. It is a
separate path from the inline color-control recognizer.

The native HUD has both classic AUI and a newer markup/token path. Some paths
already normalize caret controls; indiscriminately stripping controls or
hooking the common HUD drawing function would risk losing colors, escaped
carets and unrelated text while missing classic dialogs. This revision does
not claim a repair for the visible caret text. A bounded, read-only probe runs
after native constructors when the three matching exports are present. It
checks the actual registered color control with uppercase/lowercase hex and
short/invalid controls, logs only synthetic test results, and does not change
registry objects, game code, fonts or user strings. The next device report
will distinguish missing registration from a later rendering-path problem.

## Validation and limits

Portable tests compile the actual frame publication, UI borrowing/reset,
surface lifecycle, opacity classification/drawing and pthread_once functions.
They cover allocation failures, overlapping surface lifetimes, old API
fallbacks, opaque/translucent GPU and CPU frames, Java UI alpha, malformed
formats and rejected starts with an old worker still reserved. The text probe
is tested against missing/uninitialized exports and synthetic controls.

The device capture from the previous build still shows substantial memory
pressure and thermal escalation. Empty UI wake suppression removes redundant
host work; it does not add a Java collector or establish a bounded total game
footprint. Successful CI and an installable IPA do not prove all scene flicker,
language defects, stutter or crashes have been resolved on the iPhone.

## Primary references

- [Android PixelFormat opacity contracts](https://developer.android.com/reference/android/graphics/PixelFormat)
- [AOSP pthread_once implementation](https://android.googlesource.com/platform/bionic/+/refs/heads/main/libc/bionic/pthread_once.cpp)
