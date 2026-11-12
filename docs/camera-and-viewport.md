# Camera2 and photo capture intents

`husk.Cameras` implements basic Camera2 devices, sessions, capture requests and
results. `husk-tl-dvm-camera.m` supplies real frames through AVFoundation on iOS.
Opening a device requests video permission; enumerating cameras does not.
The bridge has no microphone input. Camera access stops when the app goes into
the background, its session closes, or the guest Android application terminates.

The Android camera exposes a backward-compatible software stream configuration:
JPEG, planar YUV 420 and preview surfaces at up to 1920 × 1080, with a 15 fps
source. These are the bridge's output sizes, rather than raw sensor dimensions.
Exposure, ISO and capture timestamps come from the native source. Frame scaling,
JPEG encoding and preview conversion happen in native code. Fixed/infinity focus
is available through the capture request. RAW, flash, manual sensor controls,
video recording and background camera capture are not implemented.
Preview buffers release their Canvas before reuse, restoring the bitmap's
coordinate transform instead of accumulating flips and native drawing contexts.

`ImageReader` keeps a bounded queue and tracks acquired images until they close.
Callbacks retain the subscribing guest process. Camera metadata keys have their
canonical Android names and independent key instances address the same value.
In particular, `CONTROL_AF_MODE` and `LENS_FOCUS_DISTANCE` must be non-null before
CameraX builds its configuration; the former stubs crashed Yandex's route screen
while applying infinity focus.

`husk.PhotoCapture` handles `android.media.action.IMAGE_CAPTURE` through iOS's
camera UI. It copies the resulting JPEG through the caller's `EXTRA_OUTPUT`
content URI and returns the activity result in the caller's process. Cancellation
returns `RESULT_CANCELED` and removes the temporary capture. FileProvider
authorities supplied through manifest string resources are resolved consistently
for provider installation, lookup and transport. File and asset descriptors pass
through the provider transport, with the output stream retaining its descriptor.

## Validation

`python3 tests/translation-layer/test_camera.py` tests production Java classes
with a synthetic native boundary: key identity, immutable requests, bounded image
queues, YUV planes, device/session callbacks, capture results, permission denial
and reset cleanup.

`tests/translation-layer/test_camera_apk.py` is an optional integration test of
Yandex Map Editor 0.44's original CameraX adapter and FileProvider. Supply
`HUSK_CAMERA_APK`, `HUSK_JAVAAPP_TEST`, `HUSK_FRAMEWORK` and the native harness's
runtime, ANGLE and resource environment. It exercises preview pixels, repeated
JPEG captures, pause/reopen, the output URI contract and cancellation. Preview
checks also verify stable row orientation across successive frames. The extra
DEX contains only test entry points; the original APK stays unchanged. The
fixture does not authenticate, start MRC routes or upload photographs.

The desktop implementation never opens a physical camera. Only an explicit
`TL_CAMERA_TEST_PATTERN=1` enables synthetic frames; the iOS implementation does
not contain that test path. Actual camera quality, orientation and authorization
UI require a physical iPhone check.
