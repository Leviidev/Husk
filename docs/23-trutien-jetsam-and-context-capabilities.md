# Tru Tien memory termination and frame synchronization

The November 12 device capture contains a Jetsam report at 14:19 local time.
Its foreground Husk process matches the PID in the previous run's `86fa674`
log. The reason is `per-process-limit`: approximately 3379 MiB charged at
termination, after the final heartbeat reported only 17 MiB of headroom.
The run reached thermal state 2 after about 161 seconds. This establishes an
iOS memory-budget termination, rather than a new native exception report.

The same capture already shows `be6b123` running for 176 seconds, with all 17
text filters installed. It has a memory warning after about 33 seconds and
thermal state 1 after about 90 seconds. The text fix is not a memory fix.
Native exception reports from November 8 do not diagnose this newer event.
The private captures and crash reports remain local.

## Frame synchronization

The game requests an ES2 context; a guest-visible GL version string in the
capture reports ES3. The previous context registry stored only the requested
version. ANGLE's backwards-compatible context extension can return a later
compatible version, so the requested minimum is not a reliable capability
test. See the [ANGLE specification](https://raw.githubusercontent.com/google/angle/main/extensions/EGL_ANGLE_create_context_backwards_compatible.txt)
and the [packaged revision's Metal display implementation](https://raw.githubusercontent.com/google/angle/e3fdc27d77e3/src/libANGLE/renderer/metal/DisplayMtl.mm).

For a context classified as ES2, frame publication and sampling use the
synchronous `glFinish` fallback. The host now observes the unwrapped
`GL_VERSION` once after a registered context is successfully made current.
GPU fences require an observed version of at least ES3 plus all required core
sync functions on both shared contexts. True ES2 and unknown versions retain
the fallback. Creation attributes, game shaders and resolution are unchanged.
Removal/reuse clears the cached observation. A bounded log identifies the
requested version, observed version and selected sync path for each context.

The old GL version log has no context ID, so it does not by itself prove which
context supplied the string. The new observation is tied to the actual current
context. Its performance effect still needs device measurement. Also, the
existing EGL swap counter includes the game and host compositor surfaces;
it cannot be compared directly with host presentation count as dropped frames.

## Memory follow-up

Extended memory accounting previously appeared only at UIKit memory warnings.
It is now sampled every 30 seconds during a Java-game session, including
footprint, resident memory, headroom, compression and the graphics ledger.
These kernel categories overlap and must not be summed as independent usage.
The heartbeat's session/stop barrier bounds the sampling lifetime.

Inspection of APK 2411's archive cache found independent copied buffers,
eviction under its existing mutex, and a cache cap of approximately 24 MiB.
Purging that cache cannot account for the hundreds of MiB of later growth and
can cause more file reads. No extra purge, synthetic background notification,
quality reduction, crowd limit or resolution change is applied here. Existing
real low-memory notifications still reach the game's own adaptive guard.

Portable tests execute the production context registry and current-binding
wrapper with simulated driver versions, including requested ES2/actual ES3,
true ES2, missing functions, invalid versions, failed bindings, foreign
contexts, removal/reuse and host context registration. Existing frame-fence
and surface-lifetime tests cover ordering and fallback. These checks do not
establish that the memory termination or gameplay stutter is fully resolved.
