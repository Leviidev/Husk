/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macOS: see run_font_paths.sh. Exercises the production path resolver. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
void tl_set_system_fonts_dir(const char *);
void tl_set_extra_fonts_dir(const char *);
const char *tl_path_resolve(const char *, char *, size_t);
static void check(const char *path, const char *expected) {
    char buf[2048]; const char *actual = tl_path_resolve(path, buf, sizeof(buf));
    if (strcmp(actual, expected)) { fprintf(stderr, "%s -> %s, expected %s\n", path, actual, expected); exit(1); }
}
int main(void) {
    char root[] = "/tmp/husk-font-paths-XXXXXX", fonts[1024], extra[1024], file[2048];
    assert(mkdtemp(root));
    snprintf(fonts, sizeof(fonts), "%s/fonts", root); assert(!mkdir(fonts, 0700));
    snprintf(extra, sizeof(extra), "%s/extra", root); assert(!mkdir(extra, 0700));
    snprintf(file, sizeof(file), "%s/Latin.ttf", fonts); FILE *f = fopen(file,"wb"); assert(f); fclose(f);
    tl_set_system_fonts_dir(root); tl_set_extra_fonts_dir(extra);
    setenv("ANDROID_ROOT", "/runtime/java-data", 1);
    check("/system/fonts/Latin.ttf", file);
    check("/runtime/java-data/fonts/Latin.ttf", file);
    snprintf(file, sizeof(file), "%s/CJK.ttc", extra); f=fopen(file,"wb"); assert(f); fclose(f);
    check("/system/fonts/CJK.ttc", file);
    check("/runtime/java-data/fonts/CJK.ttc", file);
    check("/runtime/java-data/fonts-backup/CJK.ttc", "/runtime/java-data/fonts-backup/CJK.ttc");
    check("/runtime/java-data-other/fonts/CJK.ttc", "/runtime/java-data-other/fonts/CJK.ttc");
    check("/runtime/java-data/files/save", "/runtime/java-data/files/save");
    unlink(file); snprintf(file,sizeof(file),"%s/fonts/CJK.ttc",root);
    check("/runtime/java-data/fonts/CJK.ttc",file);
    setenv("ANDROID_ROOT","/system",1); check("/system/fonts/CJK.ttc",file);
    unsetenv("ANDROID_ROOT"); check("/system/fonts/CJK.ttc",file);
    tl_set_system_fonts_dir(NULL);
    check("/runtime/java-data/fonts/CJK.ttc","/runtime/java-data/fonts/CJK.ttc");
    snprintf(file,sizeof(file),"%s/Latin.ttf",fonts);unlink(file);rmdir(fonts);rmdir(extra);rmdir(root);
    puts("PASS: bundled/downloaded fonts, absent fallback, path boundaries, unrelated data");
}
