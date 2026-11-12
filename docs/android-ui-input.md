# Android controls, keyboard and switch geometry

Drawing and hit testing share `ViewGroup`'s stable child order, including custom
drawing order, elevation and translation Z. Touches walk that order from front
to back. A camera preview brought to the front of the child array must not
intercept controls that are rendered above it through elevation.

Husk's Java viewport ignores SwiftUI's keyboard safe area. Android's window and
IME insets already perform `adjustResize`; shrinking the UIKit host as well made
the fixed-size Metal surface scale down and acquire black margins on both sides.

The platform `Switch` measures room for two thumb positions and the track's
padding. A nine-patch track's intrinsic width can be only a narrow stretchable
strip. Thumb placement uses its content width, excluding transparent drawable
padding. Cached `NinePatchDrawable` constant states create independent drawable
instances: bitmap pixels can be shared, but bounds, tint and callbacks cannot.

Java Android apps now occupy a separate viewport below Husk's toolbar, within the
host safe area. An existing fixed-size Android surface fits inside the available
space when the toolbar is hidden or the host changes size. The WKWebView sibling
uses the same bounds/transform as the Metal view; keyboard overlap is measured in
that view's local coordinates. Other native game engines keep their screen layout.

## Checks

`test_ui_apk.py` uses the original Yandex Map Editor APK's riding layout and
preference/widget layouts. It verifies Back and Continue above a touch-consuming
preview, pass-through outside controls, complete switch pixels, and opposite
thumb positions for checked and unchecked states. It uses the environment
documented by `docs/javaapp-test-entrypoints.md`, with no account or route recording.

`build_viewport_keyboard_probe.py <output-directory>` generates a standalone
iOS Simulator app from the current `TLWindowScreen`, `TLWindowHost` and Java
viewport body. Only the guest renderer is replaced by a UIKit view/text field.
Compile the generated Swift with `xcrun --sdk iphonesimulator swiftc`, using
module name `ViewportProbe`, a simulator target and SDK; sign and install the
generated app bundle with `simctl`. It opens a real keyboard and writes
`Documents/result.json`, checking that the host and rendered view keep their
width and height. A control build with the keyboard modifier removed reproduces
the shrinking host. The probe does not use the camera or location.
