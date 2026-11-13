# Tru Tien frame-cap and frame-stage review

Device validation confirms that build `595b9a1` observes the game's requested
ES2 context as actual ES3 and selects GPU fences. Host frame counts can reach
approximately 60 per second when the game produces frames at that rate. The
external SDL path has no fixed host-side 30 FPS limit. Its timed wait wakes
when a new frame is published; the producer's handoff wait is backpressure.
The displayed FPS counts presentations; a login/UI sample must not be reported
as gameplay performance. The device can render UI near 60 while an in-world
session remains below 30.

The user's current game settings use automatic frame limiting. APK 2411 reads
`[Video] FrameCap`, with `[ZxCrowd] FpsLimit` as a legacy fallback. A positive
setting is a ceiling, not a guaranteed frame rate. The native loop still calls
`FrameCapNext`, then paces to the chosen effective cap. Consequently, changing
only the Android surface refresh hint cannot produce 60 new game frames.

The game's existing `PerfStats.csv` provides interval aggregates for tick,
render, present, deliberate wait and maximum frame time. The inspected run
commonly spends roughly 8-10 ms on tick/render, about 1 ms on presentation and
about 24 ms waiting at its selected 30 FPS cap. It also contains occasional
100-500 ms stalls. The private samples remain local. An aggregate's maximum
and the percentile of aggregate means are not a true per-frame percentile.
The CSV field named `host_ms` concerns game logic and must not be labelled
Husk compositor cost without independent evidence.

Static inspection found no clear self-lock caused by counting the intentional
pacer sleep as CPU work: the next frame's starting timestamp is written after
`clock_nanosleep`; the default `FrameCapWork=1` path subtracts waits and combines
frame-thread, render-thread and GPU costs. This does not establish why the
adaptive limiter selected 30 in the user's scene.

## Native log creation

The missing native `EC.log` concealed the limiter's reason messages. The game
cleans old `.log` files before initializing its logger. Precreating an empty
file therefore does not work. It then opens `Logs\EC.log`, while the existing
external-data directory is named `logs`.

Husk's relative path lookup previously found case-insensitive existing files,
but fell back to the unresolved relative path when the final file did not
exist. A new log after cleanup could not be created in the intended directory.
The fix preserves existing-file lookup precedence, resolves the existing
parent directory and retains the new final filename. Its fallback is limited
to the Tru Tien package and a single safe `.log` filename under its log
directory. It creates no directories, rewrites no settings and does not change
other packages' file lookup.

## Measuring the remaining stalls

The compositor can snapshot a sequence before taking the texture handoff lock,
then draw a newer sequence published in between. Acknowledging only the older
snapshot leaves the producer waiting for a frame the compositor already
consumed. A UI-triggered redraw can encounter the same race even when the old
snapshot was not fresh. The acknowledgement must follow the actual sampled
sequence after a successful swap, retain the clear/legacy fallback and leave
later publications pending. Texture fences still govern GPU reuse. Failed
swaps must neither acknowledge an unseen frame nor enter an unbounded retry
loop. This is a conditional extra wait, not proof of a fixed host 30 FPS cap.

Additional bounded telemetry separates unique game publications from host
presentations and records time spent copying/publishing, waiting for handoff,
composing and swapping. Fixed storage avoids allocating each frame. Summaries
are emitted outside frame ownership locks and reset across session teardown.
Histogram percentiles are estimates with documented bucket bounds. Idle and
UI-only redraws must not be counted as newly generated game frames.

The purpose is to attribute the long frames before changing scheduling or
game behavior. These diagnostics do not assert that 60 FPS, a 50 FPS minimum,
or the previous memory-limit termination has been solved. The game's thermal
and memory safeguards, graphics settings and user-visible content remain
enabled.

## Java allocation lifetime

The Java runtime currently has no tracing collector. While it is active,
`tl_jni_unref` defers freeing objects because native reference counts alone
cannot describe Java fields, statics or interpreter registers. Blindly enabling
reference-count frees would introduce dangling Java references.

Allocation accounting reports only known object headers and array payloads,
plus deferred unref calls. It excludes strings, fields, native resources and
allocator overhead, and is not an exact Java-heap or reachability measurement.
The periodic memory report can compare that known retention with overall
process growth without walking the heap or changing object lifetimes. This
identifies a real growth source; it does not attribute every byte of the
observed footprint to Java or claim that crash prevention is complete.
