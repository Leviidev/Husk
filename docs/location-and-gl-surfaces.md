# Location and embedded GL surfaces

Husk forwards Android `LocationManager` listener subscriptions to iOS Core Location.
`husk.Geo` keeps each listener's executor, guest process, interval and displacement
filter. `husk-tl-dvm-location.m` requests When In Use authorization only when an
active subscription needs location. Passive listeners and cache reads do not
start tracking. Updates stop in the background and after the last active
subscription is removed. Permission denial or unavailable services never produce
invented coordinates. Session reset cancels queued callbacks and clears the fix.

The bridge supplies coordinates, accuracy, altitude, speed, course and both wall
and monotonic timestamps. Android `gps`, `network` and `fused` providers currently
use the same Core Location source. PendingIntent delivery, GNSS measurements and
Google Play Services' fused-location service are not implemented by this bridge.
The desktop implementation reports location unavailable and never queries the
Mac's location.

A `GLSurfaceView` may occupy only part of the host window. Husk asks the view for
its screen rectangle, renders into a pbuffer of that size and composites the
result below Android controls at the matching position. Fullscreen views retain
the direct framebuffer path. Rendering a partial view directly into the host's
full framebuffer placed its viewport at the bottom of the window, leaving a
blank area at the actual view's top. This affected Yandex Map Editor's walk map.

## Validation

`python3 tests/translation-layer/test_location.py` runs the production Java
framework on a JVM with synthetic native samples. It covers authorization,
metadata, filtering, cancellation, cache copies, process ownership and reset.

`tests/translation-layer/fixtures/location-ios-probe.m` links the production
native bridge into a small iOS Simulator app. Its scene must become active and
its Info.plist must include `NSLocationWhenInUseUsageDescription`. Grant/revoke
the app's location permission with `simctl privacy`; inject a synthetic fix with
`simctl location`. The probe writes `Documents/result.json`. Set
`HUSK_GEO_EXPECT_DENIED=1` for the denied-permission case. Clear the injected
location and remove the probe after testing.

`python3 tests/translation-layer/test_glsurfaceview.py` checks actual rendered
pixels for an offset view, a resized view and a fullscreen view. Supply
`HUSK_JAVAAPP_TEST` (the native `tools/javaapp-test.c` harness), `HUSK_FRAMEWORK`
(the current framework DEX), `TL_ART`, `TL_FRAMEWORK_RES`, `TL_ANGLE_EGL` and
`TL_ANGLE_GLES`. A JIT-less run also needs `TL_NO_XMEM=1` and `TL_AOT_DIR`.
The test builds its own APK and does not use an account or third-party map SDK.

The harness also accepts `TL_ACTIVITY`, `TL_APPLICATION` and `TL_EXTRA_DEX` to
exercise an SDK from an unchanged APK through an additional test entry point.
Normal app startup leaves all three unset and uses the APK's manifest.
