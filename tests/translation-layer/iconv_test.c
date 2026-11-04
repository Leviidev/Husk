/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-bionic.h"
#include <assert.h>
#include <errno.h>
#include <iconv.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

static __thread int guest_errno;
void tl_set_guest_errno(int e) { guest_errno = e; }
int tl_errno_to_guest(int e)
{
    /* Only the host errors this module can produce; EILSEQ is intentionally
     * translated even on hosts whose other numbers already match Linux. */
    if (e == EILSEQ) return 84;
    if (e == E2BIG) return 7;
    if (e == EINVAL) return 22;
    if (e == EBADF) return 9;
    if (e == ENOMEM) return 12;
    return e;
}
void tl_log_line(const char *fmt, ...) { (void)fmt; }

static iconv_t (*guest_open)(const char *, const char *);
static size_t (*guest_convert)(iconv_t, char **, size_t *, char **, size_t *);
static int (*guest_close)(iconv_t);

static void *lookup(const char *name)
{
    for (const tl_bionic_entry *e = tl_tab_iconv; e->name; e++)
        if (!strcmp(name, e->name)) return e->addr;
    return NULL;
}
static iconv_t open_ok(const char *to, const char *from)
{
    guest_errno = 123;
    iconv_t cd = guest_open(to, from);
    assert(cd != (iconv_t)-1 && cd != NULL);
    assert(guest_errno == 123);
    return cd;
}
static void roundtrip(const char *encoding, const char *text, size_t len)
{
    char encoded[256], decoded[256];
    char *in = (char *)text, *out = encoded;
    size_t ni = len, no = sizeof(encoded);
    iconv_t cd = open_ok(encoding, "UTF-8");
    assert(guest_convert(cd, &in, &ni, &out, &no) == 0 && ni == 0);
    assert(in == text + len && guest_errno == 123);
    assert(guest_close(cd) == 0 && guest_errno == 123);
    ni = sizeof(encoded) - no; in = encoded; out = decoded; no = sizeof(decoded);
    cd = open_ok("UTF-8", encoding);
    assert(guest_convert(cd, &in, &ni, &out, &no) == 0 && ni == 0);
    assert((size_t)(out - decoded) == len && !memcmp(decoded, text, len));
    assert(guest_close(cd) == 0);
}
static void streaming_errors(void)
{
    iconv_t cd = open_ok("UTF-16LE", "UTF-8");
    char input[] = "A\xC3\xA9", output[16];
    char *in = input, *out = output; size_t ni = 3, no = 2;
    assert(guest_convert(cd, &in, &ni, &out, &no) == (size_t)-1);
    assert(guest_errno == 7 && in == input + 1 && ni == 2 && out == output + 2 && no == 0);
    assert((unsigned char)output[0] == 65 && output[1] == 0);
    no = 14;
    assert(guest_convert(cd, &in, &ni, &out, &no) == 0 && ni == 0);
    assert((unsigned char)output[2] == 0xE9 && output[3] == 0 && guest_errno == 7);
    char invalid[] = "A\xFF"; in = invalid; ni = 2; out = output; no = sizeof(output);
    assert(guest_convert(cd, &in, &ni, &out, &no) == (size_t)-1);
    assert(guest_errno == 84 && in == invalid + 1 && ni == 1 && out == output + 2);
    char incomplete[] = "\xC3\xA9"; in = incomplete; ni = 1; out = output; no = sizeof(output);
    assert(guest_convert(cd, &in, &ni, &out, &no) == (size_t)-1);
    assert(guest_errno == 22 && in == incomplete && ni == 1 && out == output);
    ni = 2;
    assert(guest_convert(cd, &in, &ni, &out, &no) == 0 && ni == 0);
    assert(guest_convert(cd, NULL, NULL, NULL, NULL) == 0);
    in = NULL;
    assert(guest_convert(cd, &in, NULL, NULL, NULL) == 0);
    assert(guest_close(cd) == 0);
}
static void stateful(void)
{
    /* ISO-2022-JP needs shift sequences and a final reset. Separate calls on
     * the same descriptor must not emit a second shift into JIS mode. */
    iconv_t cd = open_ok("ISO-2022-JP", "UTF-8");
    char jp[] = "\xE3\x81\x82", output[32];
    char *in = jp, *out = output; size_t ni = 3, no = sizeof(output);
    assert(guest_convert(cd, &in, &ni, &out, &no) == 0);
    assert(out == output + 5 && !memcmp(output, "\x1B$B$\x22", 5));
    in = jp; ni = 3;
    assert(guest_convert(cd, &in, &ni, &out, &no) == 0 && out == output + 7);
    no = 2;
    assert(guest_convert(cd, NULL, NULL, &out, &no) == (size_t)-1 && guest_errno == 7);
    no = 25;
    assert(guest_convert(cd, NULL, NULL, &out, &no) == 0);
    assert(out == output + 10 && !memcmp(output + 7, "\x1B(B", 3));
    assert(guest_close(cd) == 0);
}
static void representability_and_surrogates(void)
{
    char emoji[] = "\xF0\x9F\x8E\xAE", output[16];
    char *in = emoji, *out = output; size_t ni = 4, no = sizeof(output);
    iconv_t cd = open_ok("ASCII", "UTF-8");
    assert(guest_convert(cd, &in, &ni, &out, &no) == (size_t)-1);
    assert(guest_errno == 84 && in == emoji && ni == 4 && out == output);
    assert(guest_close(cd) == 0);
    cd = open_ok("UTF-8", "UTF-16LE");
    char low[] = { 0, (char)0xDC }; in = low; ni = 2; out = output; no = sizeof(output);
    assert(guest_convert(cd, &in, &ni, &out, &no) == (size_t)-1 && guest_errno == 84);
    assert(in == low && ni == 2 && out == output);
    char pair[] = { 0, (char)0xD8, 0, (char)0xDC }; in = pair; ni = 2;
    assert(guest_convert(cd, &in, &ni, &out, &no) == (size_t)-1 && guest_errno == 22);
    assert(in == pair && ni == 2 && out == output);
    ni = 4;
    assert(guest_convert(cd, &in, &ni, &out, &no) == 0 && ni == 0 && out == output + 4);
    assert(!memcmp(output, "\xF0\x90\x80\x80", 4));
    assert(guest_close(cd) == 0);
}
#ifdef _WIN32
static DWORD WINAPI thread_check(void *unused)
#else
static void *thread_check(void *unused)
#endif
{
    (void)unused;
    assert(guest_errno == 0);
    assert(guest_open("not-a-charset", "UTF-8") == (iconv_t)-1 && guest_errno == 22);
    roundtrip("UTF-32LE", "thread", 6);
    return 0;
}
int main(void)
{
#ifdef _WIN32
    husk_test_iconv_init();
#endif
    guest_open = (void *)lookup("iconv_open");
    guest_convert = (void *)lookup("iconv");
    guest_close = (void *)lookup("iconv_close");
    assert(guest_open && guest_convert && guest_close && !lookup("iconv_missing"));
    puts("PASS: symbol table resolves all three real wrappers");
    const char text[] = "Tru Ti\xC3\xAA" "n \xE4\xB8\xAD \xF0\x9F\x8E\xAE\0end";
    const char *enc[] = { "UTF-16LE", "UTF-16BE", "UTF-32LE", "UTF-32BE", "WCHAR_T", "wchar-t" };
    for (size_t i = 0; i < sizeof(enc) / sizeof(enc[0]); i++) roundtrip(enc[i], text, sizeof(text) - 1);
    puts("PASS: Vietnamese/CJK/emoji/embedded NUL roundtrips and Android WCHAR_T");
    streaming_errors();
    puts("PASS: E2BIG partial progress, EILSEQ, EINVAL retry, pointer/count updates, reset");
    stateful();
    puts("PASS: stateful descriptor persists and flush can be retried after E2BIG");
    representability_and_surrogates();
    puts("PASS: unrepresentable output and malformed/incomplete UTF-16 surrogate errors");
    assert(guest_open("not-a-charset", "UTF-8") == (iconv_t)-1 && guest_errno == 22);
    assert(guest_open(NULL, "UTF-8") == (iconv_t)-1 && guest_errno == 22);
    assert(guest_close((iconv_t)-1) == -1 && guest_errno == 9);
    assert(guest_convert(NULL, NULL, NULL, NULL, NULL) == (size_t)-1 && guest_errno == 9);
    iconv_t cd = open_ok("UTF-8", "UTF-8");
    char *p = "x"; size_t n = 1;
    assert(guest_convert(cd, &p, &n, NULL, NULL) == (size_t)-1 && guest_errno == 22);
    assert(guest_close(cd) == 0);
    for (int i = 0; i < 100; i++) roundtrip("UTF-16LE", "lifecycle", 9);
    puts("PASS: unsupported encodings, invalid arguments, descriptor lifecycle");
    guest_errno = 456;
#ifdef _WIN32
    HANDLE threads[4];
    for (int i = 0; i < 4; i++) { threads[i] = CreateThread(NULL, 0, thread_check, NULL, 0, NULL); assert(threads[i]); }
    for (int i = 0; i < 4; i++) { assert(WaitForSingleObject(threads[i], INFINITE) == WAIT_OBJECT_0); CloseHandle(threads[i]); }
#else
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) assert(!pthread_create(&threads[i], NULL, thread_check, NULL));
    for (int i = 0; i < 4; i++) assert(!pthread_join(threads[i], NULL));
#endif
    assert(guest_errno == 456);
    puts("PASS: separate descriptors and guest errno on four worker threads");
    return 0;
}
