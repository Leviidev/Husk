/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Checks the SharedPreferences store: defaults, types, the Editor's puts, and a
 * round trip through the file -- the things the stand-in it replaced got wrong. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "husk-tl-prefs.h"

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

int main(void)
{
    printf("1. the caller's default comes back for anything not stored\n");
    tl_prefs *p = tl_prefs_create();
    CHECK(tl_prefs_get_int(p, "pref_score_multiplier", 1) == 1, "score multiplier default lost");
    CHECK(tl_prefs_get_int(p, "pref_pipe_move_tier_1", 2000) == 2000, "tier default lost");
    CHECK(tl_prefs_get_int(p, "pref_bird_color", -1) == -1, "negative default lost");
    CHECK(tl_prefs_get_bool(p, "pref_moving_pipes_enabled", true) == true, "bool default lost");
    CHECK(fabsf(tl_prefs_get_float(p, "pref_gravity", 1.0f) - 1.0f) < 1e-6, "float default lost");
    CHECK(tl_prefs_get_long(p, "x", 1234567890123LL) == 1234567890123LL, "long default lost");
    CHECK(!strcmp(tl_prefs_get_string(p, "name", "fallback"), "fallback"), "string default lost");

    printf("2. one key does not answer for another\n");
    tl_prefs_put_int(p, "highScore", 42);
    CHECK(tl_prefs_get_int(p, "highScore", 0) == 42, "stored value not returned");
    CHECK(tl_prefs_get_int(p, "pref_score_multiplier", 1) == 1,
          "a different key returned the stored value -- the bug this replaces");

    printf("3. a value of another type counts as absent\n");
    tl_prefs_put_bool(p, "flag", true);
    CHECK(tl_prefs_get_int(p, "flag", 7) == 7, "a bool answered an int read");
    tl_prefs_put_int(p, "flag", 3);
    CHECK(tl_prefs_get_int(p, "flag", 7) == 3 && !tl_prefs_contains(p, "nope"), "overwriting with a new type failed");

    printf("4. remove and clear\n");
    tl_prefs_remove(p, "flag");
    CHECK(!tl_prefs_contains(p, "flag") && tl_prefs_contains(p, "highScore"), "remove touched the wrong key");
    tl_prefs_put_string(p, "gone", NULL);
    CHECK(!tl_prefs_contains(p, "gone"), "putting NULL should remove");

    printf("5. the file round trip, including awkward values\n");
    char path[] = "/Volumes/GTAV/husk2/tmp/husk-prefs-test-XXXXXX";
    int fd = mkstemp(path); close(fd);
    tl_prefs_clear(p);
    tl_prefs_attach(p, path);
    tl_prefs_put_int(p, "i", -2147483647);
    tl_prefs_put_long(p, "j", -9007199254740993LL);
    tl_prefs_put_float(p, "f", 0.1f);
    tl_prefs_put_bool(p, "z", true);
    tl_prefs_put_string(p, "tab\there", "line1\nline2\ttabbed \\ backslash \r");
    tl_prefs_put_string(p, "empty", "");
    CHECK(tl_prefs_save(p), "save failed");
    tl_prefs_destroy(p);

    tl_prefs *q = tl_prefs_create();
    CHECK(tl_prefs_attach(q, path), "reload failed");
    CHECK(tl_prefs_get_int(q, "i", 0) == -2147483647, "int did not survive");
    CHECK(tl_prefs_get_long(q, "j", 0) == -9007199254740993LL, "long did not survive");
    CHECK(tl_prefs_get_float(q, "f", 0) == 0.1f, "float did not survive bit-exact");
    CHECK(tl_prefs_get_bool(q, "z", false), "bool did not survive");
    CHECK(!strcmp(tl_prefs_get_string(q, "tab\there", ""), "line1\nline2\ttabbed \\ backslash \r"),
          "string with tab, newline and backslash did not survive: \"%s\"", tl_prefs_get_string(q, "tab\there", ""));
    CHECK(tl_prefs_contains(q, "empty") && !strcmp(tl_prefs_get_string(q, "empty", "x"), ""), "empty string lost");
    CHECK(tl_prefs_count(q) == 6, "expected 6 entries, got %d", tl_prefs_count(q));
    printf("   %d entries survived a save and reload\n", tl_prefs_count(q));

    printf("6. a missing or damaged file is not fatal\n");
    tl_prefs *r = tl_prefs_create();
    CHECK(tl_prefs_attach(r, "/Volumes/GTAV/husk2/tmp/husk-prefs-does-not-exist"), "a missing file should be fine");
    FILE *bad = fopen(path, "w"); fputs("this is not a prefs file\n", bad); fclose(bad);
    tl_prefs *s = tl_prefs_create();
    CHECK(!tl_prefs_attach(s, path) && tl_prefs_count(s) == 0, "a foreign file should be refused, not half-read");
    tl_prefs_destroy(q); tl_prefs_destroy(r); tl_prefs_destroy(s);
    unlink(path);

    printf(failures ? "=== prefs test FAILED (%d) ===\n" : "=== prefs test passed ===\n", failures);
    return failures ? 1 : 0;
}
