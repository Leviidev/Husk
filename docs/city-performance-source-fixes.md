# Tru Tien city performance: measured baseline and pending source fixes

## Measurement

The user confirmed walking and rotating the camera in the crowded city on
16 November 2026. USB logs identify installed build `5974f72`. The clean
08:52:47–08:54:13 Vietnam-time subset contains 86 interval aggregates and
2,560 frames, giving approximately **29.25 FPS** weighted by interval duration.
Tick/render medians are 7.11/12.81 ms and game wait is 13.87 ms. The largest
frame recorded in the subset is 217.8 ms. These interval statistics do not
provide an instantaneous minimum or a true whole-session frame percentile.

Two wholly contained 30-second host/producer windows both report
28.66/29.43 FPS. Host composition CPU averages 0.18/0.14 ms and screen swap
0.02 ms. This does not support a host-side swap lock at 30 FPS. It also does
not measure GPU cost. The game attempts 40 FPS, drops back under measured
workload, and applies backoff; its current ceiling is 45 with thermal state 2.

Memory growth is also material: later snapshots exceed 3.1 GiB footprint,
with only about 219 MiB process headroom. More than six million Java objects
have been recorded as unfreed; header/array accounting is only a partial
measurement and excludes strings, fields and native resources. No memory
guard was relaxed or disabled during this investigation.

Detailed captures, the unchanged configuration snapshots and raw evidence are
kept privately in `build/private-city-fps-20261116/`; they are not source inputs
for remote builds. Game counters are retained as reported, without using them
to dispute the user's observed crowded location.

## Source changes prepared

- Preserve the game's requested texture and sampler filtering. The installed
  Crisp preset silently raises AF off/1x to 4x and changes mipmap filtering;
  source now forwards the original parameters, including GL's validation of
  invalid requests. Color and sharpening presets remain separate. Resolution,
  visible-player settings and game content are not reduced by this patch.
- Reuse at most 50 `Message` objects instead of allocating a Message and an
  unused HashMap for every dispatch. Queue ownership prevents duplicate use;
  dispatch, cancellation and shutdown clear payload references before reuse.
- Replace Choreographer's five temporary per-frame lists and boxed callback
  packets with reusable records and primitive timestamps. The free-record
  cache is capped at 64; phase order, due times, cancellation, reentrant posts
  and exception propagation are preserved.

These pools reduce known allocation churn. They do not add a tracing garbage
collector, eliminate all allocations, or establish that the remaining Java
retention and native resource growth are fixed. A workload exceeding the
cached pool capacity can still allocate additional records.

## Verification and deployment state

The original sampler and object-reuse failures were reproduced before the
source changes. Targeted production C sampler tests and 15 JVM contract tests
pass, including 10,000 frame-callback iterations, 40,000 multi-threaded Message
borrow/return iterations, delayed/reentrant callbacks, failed dispatch and
copy/Bundle isolation. Seven USB deployment tests and 20 existing Python
analyzer/regression tests also pass. Scheduler tests run separately from the
general UI fixtures' Handler/Looper doubles, so they exercise the real classes.

Only unit-test harnesses and individual Java classes have been compiled.
**No application build, JAR/DEX packaging, signing, CI dispatch or installation
was performed for these source changes.** The installed application remains
the measured build. A measured improvement or stable 50–60 FPS is not yet
established.

The user's workflow is to request **BUILD** explicitly before building the
application. Future updates should use the signed `.app` USB flow described in
[USB development deployment](usb-development-deployment.md). That flow removes
manual IPA selection, but its transport still uses temporary package bytes and
requires a valid device signing identity. It cannot replace signed native code
by copying source files into Documents.
