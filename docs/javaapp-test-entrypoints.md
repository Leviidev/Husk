# Testing SDKs in an unchanged APK

The native `tools/javaapp-test.c` harness accepts `TL_ACTIVITY`, `TL_APPLICATION`
and `TL_EXTRA_DEX` to run an explicit fixture entry point against an unchanged
APK. Normal startup leaves all three unset and uses the manifest. The matching
`tl_javaapp_config` fields are optional and default to null.

`tests/translation-layer/apk_test_harness.py` compiles fixture classes against the
production Java framework and supplies a PNG reader and control-file runner.
It is a helper, not a test by itself. Integration tests supply their own APK,
native executable, framework DEX, runtime libraries, resources and ANGLE paths.
No third-party APK, generated binary or personal app data is included.
