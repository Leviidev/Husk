/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Extracted from src/translation-layer-next/husk-tl-bionic-io.c by tests/run_portable.py. */
#include "persistent_runtime_under_test.h"

int main(void)
{
    assert(tl_io_is_persistent_runtime_path(
        "/private/var/containers/Bundle/Application/UUID/Husk.app/java-runtime/com.android.i18n/etc/icu/icudt76l.dat"));
    assert(tl_io_is_persistent_runtime_path(
        "/private/var/containers/Bundle/Application/UUID/Husk.app/java-runtime/com.android.tzdata/etc/tz/versioned/9/icu"));
    assert(tl_io_is_persistent_runtime_path(
        "/private/var/containers/Bundle/Application/UUID/Husk.app/android-system/cacerts/3c860d51.0"));
    assert(!tl_io_is_persistent_runtime_path(
        "/private/var/mobile/Containers/Data/Application/UUID/Documents/TranslationLayer/3B8446CB/TruTien.apk"));
    assert(!tl_io_is_persistent_runtime_path(
        "/private/var/mobile/Containers/Data/Application/UUID/Documents/TranslationLayer/C55AD120/java-data/files/save.dat"));
    assert(!tl_io_is_persistent_runtime_path(""));
    assert(!tl_io_is_persistent_runtime_path(NULL));
    puts("PASS: java-runtime and android-system mappings stay persistent across game switches while app files remain session-scoped");
    return 0;
}
