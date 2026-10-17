# Vulkan compositor regression

Run on macOS with Xcode command-line tools, Vulkan headers and a macOS build of
MoltenVK. The library argument is the binary inside `MoltenVK.framework`, or a
MoltenVK dylib. An iOS framework cannot run this host test.

```sh
tests/translation-layer/run_vulkan_compositor.sh \
  "$VULKAN_INCLUDE_DIR" "$MOLTENVK_MACOS_LIBRARY"
```

The script writes one executable to `build/macos` (override with `OUT`). It uses
real MoltenVK swapchains and Metal readback, with stubs only for the surrounding
Android window and Java pixel consumer. It opens no window, plays no audio and
needs no APK or game data.

The checks cover:

- A 17×12 asymmetric image, BGRA UNORM and sRGB channel conversion, bottom-up
  Java pixels and top-down TextureView pixels.
- Binary present semaphores, with separate render/present queue families when
  MoltenVK exposes them, followed by a run on one queue. The test prints the
  queue count and family indices.
- Swapchain replacement without clearing the new image, final Java image
  clearing, and balanced Android window references.
- Direct presentation when neither Java composition nor a TextureView is active.

This verifies the shared bridge's pixel delivery and ownership. It does not
launch the Java runtime or establish iOS device compatibility, frame pacing,
touch input, or normal game shutdown. The added iOS Java launch wiring also
needs a device run before release.
