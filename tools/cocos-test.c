/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for the cocos2d-x driver: runs an APK's cocos2d-x game on a Mac, off-screen.
 *
 *   cocos-test <apk> [seconds] [width height]
 *
 * Geometry Dash is a landscape game, so the default surface is landscape (the phone's aspect).
 * Environment: TL_JNI_TRACE=1|2, TL_VERBOSE=0..2, TL_CTL=<fifo> (lines "tap X Y", "hold X Y MS",
 * "swipe X1 Y1 X2 Y2 MS", "wait MS", "shot PNG", "text WORD", "bs", "pause", "resume", "quit"), TL_FRAMES=<n> (save every nth frame; default latest only).
 */
#include <mach/mach.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-audio.h"
#include "husk-tl-cocos.h"
#include "husk-tl-geode.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"

void tl_log_line(const char *fmt, ...)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static struct timespec t0;
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    pthread_mutex_lock(&m);
    if (!t0.tv_sec) t0 = t;
    if (getenv("TL_LOG_TIME")) fprintf(stderr, "[%7.3f] ", (t.tv_sec - t0.tv_sec) + (t.tv_nsec - t0.tv_nsec) / 1e9);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap);
    pthread_mutex_unlock(&m);
}

static void describe(const char *label, const void *addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &lib, &sa);
    if (lib) fprintf(stderr, "  %-6s %p  %s  %s+%#lx\n", label, addr, lib, sym ? sym : "?", sa ? (unsigned long)((const char *)addr - (const char *)sa) : 0ul);
    else fprintf(stderr, "  %-6s %p\n", label, addr);
}

static int safe_read(uintptr_t addr, void *out, size_t n)
{
    vm_size_t got = 0;
    return vm_read_overwrite(mach_task_self(), (vm_address_t)addr, n, (vm_address_t)out, &got) == KERN_SUCCESS && got == n;
}

static void on_crash(int sig, siginfo_t *info, void *uctx)
{
    ucontext_t *uc = uctx;
    _STRUCT_ARM_THREAD_STATE64 *ss = &uc->uc_mcontext->__ss;
    char tn[32] = ""; pthread_getname_np(pthread_self(), tn, sizeof(tn));
    fprintf(stderr, "\n=== CRASH: signal %d, fault address %p, thread '%s' ===\n", sig, info->si_addr, tn);
    describe("pc", (void *)ss->__pc);
    describe("lr", (void *)ss->__lr);
    if (info->si_addr) describe("fault", info->si_addr);
    uintptr_t fp = ss->__fp;
    for (int i = 0; i < 16 && fp && (fp & 7) == 0; i++) {
        uint64_t fr[2];
        if (!safe_read(fp, fr, sizeof(fr))) break;
        describe("frame", (void *)fr[1]);
        fp = fr[0];
    }
    uint64_t *sp = (uint64_t *)ss->__sp; int shown = 0;
    for (int i = 0; i < 4096 && shown < 24; i++) {
        uint64_t v;
        if (!safe_read((uintptr_t)(sp + i), &v, 8)) break;
        if (v > 0x7000000000ull && v < 0x7100000000ull && tl_ld_lib_of((void *)v) && (v & 3) == 0) { describe("stk", (void *)v); shown++; }
    }
    for (int i = 0; i < 29; i += 4)
        fprintf(stderr, "  x%d=%#llx x%d=%#llx x%d=%#llx x%d=%#llx\n", i, ss->__x[i], i + 1, ss->__x[i + 1], i + 2, i + 2 < 29 ? ss->__x[i + 2] : 0, i + 3, i + 3 < 29 ? ss->__x[i + 3] : 0);
    fprintf(stderr, "  sp=%#llx fp=%#llx\n", ss->__sp, ss->__fp);
    fflush(stderr);
    _exit(139);
}

static const char *g_frame_dir;
static void sleep_ms(long ms) { usleep((useconds_t)ms * 1000); }
static void do_swipe(float x1, float y1, float x2, float y2, long ms)
{
    int steps = (int)(ms / 16); if (steps < 2) steps = 2;
    tl_cocos_touch(0, 0, x1, y1);
    for (int i = 1; i <= steps; i++) { sleep_ms(ms / steps); tl_cocos_touch(1, 0, x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps); }
    tl_cocos_touch(2, 0, x2, y2);
}
static void *control_thread(void *arg)
{
    const char *path = arg;
    for (;;) {
        FILE *f = fopen(path, "r");
        if (!f) { sleep_ms(200); continue; }
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            float a, b, c, d; long ms; char p[400];
            if (sscanf(line, "tap %f %f", &a, &b) == 2) { tl_cocos_touch(0, 0, a, b); sleep_ms(80); tl_cocos_touch(2, 0, a, b); }
            else if (sscanf(line, "hold %f %f %ld", &a, &b, &ms) == 3) { tl_cocos_touch(0, 0, a, b); sleep_ms(ms); tl_cocos_touch(2, 0, a, b); }
            else if (sscanf(line, "swipe %f %f %f %f %ld", &a, &b, &c, &d, &ms) == 5) do_swipe(a, b, c, d, ms);
            else if (sscanf(line, "wait %ld", &ms) == 1) sleep_ms(ms);
            else if (sscanf(line, "shot %399s", p) == 1) {
                char cmd[900]; snprintf(cmd, sizeof(cmd), "sips -s format png '%s/latest.bmp' --out '%s' >/dev/null 2>&1", g_frame_dir, p);
                if (system(cmd)) fprintf(stderr, "ctl: shot failed\n");
                else fprintf(stderr, "ctl: shot %s (frame %lu)\n", p, tl_cocos_frames());
            }
            else if (sscanf(line, "text %399s", p) == 1) tl_cocos_insert_text(p);
            else if (!strncmp(line, "bs", 2)) tl_cocos_delete_backward();
            else if (!strncmp(line, "pause", 5)) tl_cocos_set_paused(true);
            else if (!strncmp(line, "resumesound", 11)) tl_cocos_resume_sound();
            else if (!strncmp(line, "resume", 6)) tl_cocos_set_paused(false);
            else if (!strncmp(line, "quit", 4)) { fprintf(stderr, "ctl: quit\n"); fflush(stderr); _exit(0); }
        }
        fclose(f);
    }
    return NULL;
}

/* TL_AUDIO_STATS=1: no speakers, but pace like a device and report how loud the mixer's output is once a second. */
static void stats_hook(const int16_t *samples, int frames, int channels, int rate)
{
    static long total_frames; static int peak; static long last_report;
    for (int i = 0; i < frames * channels; i++) { int v = samples[i] < 0 ? -samples[i] : samples[i]; if (v > peak) peak = v; }
    total_frames += frames;
    if (total_frames - last_report >= rate) { fprintf(stderr, "audio: %ld frames written, peak %d in the last second\n", total_frames, peak); last_report = total_frames; peak = 0; }
    struct timespec ts = { 0, (long)((double)frames * 1e9 / rate) }; nanosleep(&ts, NULL);
}


/* TL_SSL_PROBE=1: Geometry Dash's libcurl and OpenSSL are linked into libcocos2dcpp.so and export their names, so what a failing HTTPS request
 * trips over can be read off their own error calls. */
struct bias_q { const char *name; uintptr_t bias; };
static int bias_cb(uintptr_t bias, const char *name, const void *ph, unsigned n, void *u)
{ (void)ph; (void)n; struct bias_q *q = u; if (!strcmp(name, q->name)) { q->bias = bias; return 1; } return 0; }
static uintptr_t lib_bias(const char *name) { struct bias_q q = { name, 0 }; tl_ld_iterate(bias_cb, &q); return q.bias; }
static void err_probe(uint64_t *r) { tl_log_line("SSL ERR_put_error lib=%d func=%d reason=%d at %s:%d", (int)r[0], (int)r[1], (int)r[2], (const char *)r[3], (int)r[4]); }
static void strerr_probe(uint64_t *r) { tl_log_line("SSL curl_easy_strerror(%d)", (int)r[0]); }
static void loadloc_probe(uint64_t *r) { tl_log_line("SSL SSL_CTX_load_verify_locations(cafile=%s, capath=%s)", r[1] ? (const char *)r[1] : "none", r[2] ? (const char *)r[2] : "none"); }
static void rand_probe(uint64_t *r) { (void)r; tl_log_line("SSL RAND_status()"); }
static void sslconn_probe(uint64_t *r) { (void)r; tl_log_line("SSL SSL_connect()"); }
static void handshake_probe(uint64_t *r) { (void)r; tl_log_line("SSL SSL_do_handshake()"); }
static void install_ssl_probes(void)
{
    tl_lib *L = tl_ld_find_lib("libcocos2dcpp.so");
    uintptr_t bias = lib_bias("libcocos2dcpp.so");
    static const struct { const char *sym; void (*cb)(uint64_t *); } p[] = {
        { "ERR_put_error", err_probe }, { "curl_easy_strerror", strerr_probe }, { "SSL_CTX_load_verify_locations", loadloc_probe },
        { "RAND_status", rand_probe }, { "SSL_connect", sslconn_probe }, { "SSL_do_handshake", handshake_probe },
    };
    for (size_t i = 0; L && i < sizeof(p) / sizeof(p[0]); i++) {
        uintptr_t a = (uintptr_t)tl_ld_sym(L, p[i].sym);
        if (!a || !tl_ld_probe(L, a - bias, p[i].cb)) fprintf(stderr, "ssl probe on %s failed\n", p[i].sym);
    }
}


/* TL_CRYPTO_TEST=1: Geometry Dash's OpenSSL against answers known to be right, to tell a runtime that miscomputes from a handshake that misbehaves. */
static void crypto_selftest(void)
{
    tl_lib *L = tl_ld_find_lib("libcocos2dcpp.so");
    if (!L) return;
    typedef unsigned char *(*hash_fn)(const unsigned char *, size_t, unsigned char *);
    static const struct { const char *name; int len; const char *expect; } t[] = {
        { "MD5", 16, "900150983cd24fb0d6963f7d28e17f72" },
        { "SHA1", 20, "a9993e364706816aba3e25717850c26c9cd0d89d" },
        { "SHA256", 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "SHA384", 48, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7" },
        { "SHA512", 64, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f" },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        hash_fn f = (hash_fn)tl_ld_sym(L, t[i].name);
        unsigned char out[64] = { 0 };
        char hex[140] = "";
        if (f) { f((const unsigned char *)"abc", 3, out); for (int k = 0; k < t[i].len; k++) snprintf(hex + 2 * k, 4, "%02x", out[k]); }
        tl_log_line("CRYPTO %-7s %s %s", t[i].name, f ? (strcmp(hex, t[i].expect) == 0 ? "ok" : "WRONG") : "(missing)", f && strcmp(hex, t[i].expect) ? hex : "");
    }
}

/* TL_DUMP_VADDR=<hex vaddr>:<bytes>:<outfile>: writes what libcocos2dcpp.so's memory holds there, to compare with the file's own bytes. */
static void dump_vaddr(const char *spec)
{
    unsigned long va = 0; long len = 0; char path[300];
    if (sscanf(spec, "%lx:%ld:%299s", &va, &len, path) != 3) return;
    uintptr_t bias = lib_bias("libcocos2dcpp.so");
    FILE *f = fopen(path, "wb");
    if (f) { fwrite((const void *)(bias + va), 1, (size_t)len, f); fclose(f); tl_log_line("DUMP %ld bytes at %#lx (bias %#lx) -> %s", len, va, (unsigned long)bias, path); }
}

/* TL_BN_TEST=1: OpenSSL big-number arithmetic against answers computed elsewhere. */
static void bn_selftest(void)
{
    tl_lib *L = tl_ld_find_lib("libcocos2dcpp.so");
    if (!L) return;
    typedef void *(*new_fn)(void); typedef int (*hex_fn)(void **, const char *); typedef int (*exp_fn)(void *, void *, void *, void *, void *); typedef int (*mul_fn)(void *, void *, void *, void *, void *);
    typedef char *(*tohex_fn)(const void *);
    new_fn bn_new = tl_ld_sym(L, "BN_new"), ctx_new = tl_ld_sym(L, "BN_CTX_new"); hex_fn hex2bn = tl_ld_sym(L, "BN_hex2bn"); exp_fn modexp = tl_ld_sym(L, "BN_mod_exp"); mul_fn modmul = tl_ld_sym(L, "BN_mod_mul"); tohex_fn tohex = tl_ld_sym(L, "BN_bn2hex");
    if (!bn_new || !ctx_new || !hex2bn || !modexp || !modmul || !tohex) { tl_log_line("BN test: functions missing"); return; }
    static const struct { const char *name, *a, *b, *n, *expect; int mul; } t[] = {
        { "modexp-odd-256", "36f67581e74ef5e8e25d940ed904759531985d5d9dc9f81818e811892f902b", "10001", "d23f0824128b2f330c5c7fd0a6a3a4506513270e269e0d37f2a74de452e6b439", "4517c24267857fbef682a7bd0e9a504800592319fd4378e9ff1a429191b39ef8", 0 },
        { "modexp-odd-bigexp-256", "36f67581e74ef5e8e25d940ed904759531985d5d9dc9f81818e811892f902b", "4688b7671738f7d93d9c172411e20b8f6b0d549b6f03675a1600a35a099950d8", "d23f0824128b2f330c5c7fd0a6a3a4506513270e269e0d37f2a74de452e6b439", "18c070c9dabdfc91ab00e214f8ace22c00016af4aa07b7f10cb258770ae88045", 0 },
        { "modexp-odd-521", "2e44158bae97ba94d0eda82f8f6d05584ef8aa38922766581e27a1c08a6a63ec24ede6a46b4cb2424a23d5962217beaddbc496cb8e81973e0becd7b03898d190", "10001", "1f30cb1e29c658cda1495e60af593bd04cf0fd630f1f29d0da9953f48f1a09f76b5a170b33839263059f28c105d1fb17c2390c192cfd3ac94af0f21ddb66cad4a27", "1b364a90b0ddf0bc9d061d273928d46f07079021682a5755032b44f94a19594d75516e6722491ee74966d9fb82116281aad6077e7791e57163c41cfc33419e541a4", 0 },
        { "modexp-odd-bigexp-521", "2e44158bae97ba94d0eda82f8f6d05584ef8aa38922766581e27a1c08a6a63ec24ede6a46b4cb2424a23d5962217beaddbc496cb8e81973e0becd7b03898d190", "6d881ed162ae2eb1547f15052434b9b5df9e7769b10f4205b4907a70c31012f037b64ce4228c38fb2918f135d25f557203301850c5a38fd547923a736994e3bf91", "1f30cb1e29c658cda1495e60af593bd04cf0fd630f1f29d0da9953f48f1a09f76b5a170b33839263059f28c105d1fb17c2390c192cfd3ac94af0f21ddb66cad4a27", "1c0e5a10b864618aa54fbeab215277333cacda32b57eedab74b1c58c363d485f3ee0310f7c8d7f64978570fdf226a21d921622bd4d66dab391e12b41599ae5b2fa", 0 },
        { "modexp-odd-1024", "aa05e1b2715945795e8229451abd81f1d69ed617f5e837d70820fe119a72d174c9df6acc011cdd9474031b7f26144b98289fcd59a54a7bb1fee08f571242425051c1ccd17f9acae01f5057ca02135e92b1d3f28ede0d7ac3baea9e13deef86ab1031d0f646e1f40a097c976bf46c697d2caf82eeeacbe226e875555790f82e", "10001", "c1d3fcff2a3af4d46b0a18e8830e07bc1e398f1012bd4acefaecbd389be4bcfc49b64a0872e6cc3ababced2057ee05cde00902c77ebff206867347214cdd2055930d6eaf14f4733f3e7d1bfbc7a2ea20b2f14c942e05319acb5c74273f98e2774cbd87ad5c90a9587403e430ec66a78795e761d17731af10506bf2efc6f87719", "adaddd7f61a5da48f3655d6321e69fcc7ea2433739ed98c8c28d98fb0612f1f9aef7a6c583f91c8f4fe705a0394a118064b043aa2859dd886b21d84dd0c5d7ddf3d0d23ff521efcbbff8be85673a7540eb627f7e4ef4700f96b9ee4e4759281e1c78f44be4eaa4c35c3ee4d3f298c552a3b2ff477213d0f3497d91faa4ec7a8d", 0 },
        { "modexp-odd-bigexp-1024", "aa05e1b2715945795e8229451abd81f1d69ed617f5e837d70820fe119a72d174c9df6acc011cdd9474031b7f26144b98289fcd59a54a7bb1fee08f571242425051c1ccd17f9acae01f5057ca02135e92b1d3f28ede0d7ac3baea9e13deef86ab1031d0f646e1f40a097c976bf46c697d2caf82eeeacbe226e875555790f82e", "1fb1d7c1bd0561e6211c70cf49952399c4aaeac137dc76fb0f17a3007e62aa0a1df9fd789c6539382b0537e65affb2297631a992f0ce583505c6af0758d5563dab2cd31ee315128862c33a4fb774eb5248db40af72158370d269a9a5ae658f33fe3b890b93f448b3a5aa3c814f426dcbb394fb36bb2d420f0f88080b10a3d6b2", "c1d3fcff2a3af4d46b0a18e8830e07bc1e398f1012bd4acefaecbd389be4bcfc49b64a0872e6cc3ababced2057ee05cde00902c77ebff206867347214cdd2055930d6eaf14f4733f3e7d1bfbc7a2ea20b2f14c942e05319acb5c74273f98e2774cbd87ad5c90a9587403e430ec66a78795e761d17731af10506bf2efc6f87719", "9311680d57261e96206730554bf68195f74c5cc9d24a15f02d694a9490178d902721aee2e70a63fe415dca47241628690a420742c71b9e9d3324a347d9340f19b982c45bf4b85b3961d338e6751dc313f1e96d43dccbaa1ff9e69586344dc30e6f80095c7459f23e0cc640cbe207832b366162d665b7b411fe2c78668a29b3d9", 0 },
        { "modexp-odd-2048", "43c71bbd87a86557b6fb7ebfeaa1551a28f7b324e4e25a15fc899e4fd58dbe7bdc968b7afb2c68774b15d7fa529ba3fe3bfada7cf20724d953ee261d87cec31f7296ab7961fd925d39d0a89a2ef80f58ee8571f4998d7c4093f6dea268aa872607679d6050914a9d33a01c353c631cdfd43f371200339d068739fa9d1de2a05d158a2ff2ee4e4519f9919c895fd7b326b94c7f9118bb16000f49c81a358ca00d75985d99c94309570dc1951c2442f9298cb3a570ccec313571810afc132d0d113db17d30cbc97d0fef792866836886a260cd0b7b45145c1a81682c64e50cad66237a0465e7e4236472f1a38f2c6ec8cc4169a3ae3a2b7fdfe01893f3aed0b6", "10001", "c7ac1491def88334e647cb8f74e69a5d0dd27a65bd628881ad1b72dba7abe1c29e1a8ef4f341e07a83f73f16dbf4a8b2b0c4312d20203626f3fe39c0519088f590fbbd119c1caaf75e8766ed88daf4016b4013ef254b0c4e010c4759482c9cbc43435cc52eae05cf96d0cc5fd4c28c2e7c26847f0316909e3bbbe9eaa8948c893b61867626bb7dbd2d1c9af0153e7c2a26a2c0bd3b1287fff52ddf5d616499c9e25a7605aec6f0245bd86d40fc891b4a6a50df4db4d66a3a47469a4d8cdb305fdd2e16096e36aab0d1bc52d9230d977ee22571594720771f8ca8181166d2287672fdf2022a96fb1a14a0f9e77f1b103cdf1582b0eab477d26415479c65dc9f51", "823f71f55678bfb6b1941c600189ac90c467a0e36afe9e0196813e50e9e5a74b56d6fad1fce6c703d15030d67928651ad0c9790b956b17c865723bc77e11dd27a6cda42f656ffef6e93c2e2c50ca180a0d8dcb146f445da4403962e7a0c8b6dbab5c04674b34ba03710f1d2762dd43e71beff44ddf1d388ab2d42ef27524ff711c571095220e7f779a5ad2e1e27aa0b270cc65cc696b63cacefcd4ef63561095d68dd321f9ffab3ca715ef157fde8790e57790ef9ea9e8634410c598b76643402286b99ebba07e7ffc8d054a2f8a53a5305cb2ac339d928bdbd90ec2c738153f84dd09257e9e6f3058b934529b220ae0ede6aafd809884f831f440ef1e585cf2", 0 },
        { "modexp-odd-bigexp-2048", "43c71bbd87a86557b6fb7ebfeaa1551a28f7b324e4e25a15fc899e4fd58dbe7bdc968b7afb2c68774b15d7fa529ba3fe3bfada7cf20724d953ee261d87cec31f7296ab7961fd925d39d0a89a2ef80f58ee8571f4998d7c4093f6dea268aa872607679d6050914a9d33a01c353c631cdfd43f371200339d068739fa9d1de2a05d158a2ff2ee4e4519f9919c895fd7b326b94c7f9118bb16000f49c81a358ca00d75985d99c94309570dc1951c2442f9298cb3a570ccec313571810afc132d0d113db17d30cbc97d0fef792866836886a260cd0b7b45145c1a81682c64e50cad66237a0465e7e4236472f1a38f2c6ec8cc4169a3ae3a2b7fdfe01893f3aed0b6", "23c3fc9dca44eb860726e25cfd56a926076b3e36bb2313f55b06258e7e26f36a8483f8b8332dd3313a0b9965cda6c6fdbd68516766934036d17e44973d4882a5ce5b2a9231f51707da45e18ac2216b02fc241d0bc9d488b1cfbf33609cfc865239194242a2eddbbd5464ecc280b0c08bc77024208aa4248c8857f9a43908f227c59db9165b0ee76f2ac34446e883a1d45de0099784b5a81842d87208d86f40f6b239f3c7174c77a2dd02de92a49636a2fa7f0eab4c4f9b0687322e25c215a82a06ec41adea0575438b0d590bb0a844e52587be6b5c9bcf35873be078f3b7a50df373ca533488f87605e999f3842e7fc229540a6eb12aa1f6d42fddbb7a86f7a2", "c7ac1491def88334e647cb8f74e69a5d0dd27a65bd628881ad1b72dba7abe1c29e1a8ef4f341e07a83f73f16dbf4a8b2b0c4312d20203626f3fe39c0519088f590fbbd119c1caaf75e8766ed88daf4016b4013ef254b0c4e010c4759482c9cbc43435cc52eae05cf96d0cc5fd4c28c2e7c26847f0316909e3bbbe9eaa8948c893b61867626bb7dbd2d1c9af0153e7c2a26a2c0bd3b1287fff52ddf5d616499c9e25a7605aec6f0245bd86d40fc891b4a6a50df4db4d66a3a47469a4d8cdb305fdd2e16096e36aab0d1bc52d9230d977ee22571594720771f8ca8181166d2287672fdf2022a96fb1a14a0f9e77f1b103cdf1582b0eab477d26415479c65dc9f51", "4a6984011b2ddd9837b197a0d3aa563fdc96ff8b9a37fcaff6db58ebd4b395b8362b7cdc9fddb318fe80e9e57cd17ca1258695efb73d01ebeb24daf8eae1fef7101904b2701c569b03be382d667cd25a07edbdbdf3ab62f2e499494eef7fbe1aad492b0092c4c433f7f030f0078814b5e34e4052014a2d7772827aa4503ad693fad6494b8b5709a6fc63f07d87ab8826a68827ef956d8efe18ce4925a11df1180e70381c3f77f70d3cd5212381e97e62328d5dec74bdc776354d37b8953518c151870f02e9aa1d93038a26fcdaeff386643e55094f4ed703ff8d12f55cc4cba65ce93a28ad6a62c4a2ddc38f5e0718285de114ace70d3408f63f21740244b1c5", 0 },
        { "modexp-even-1024", "f2be4c5ce666c1494e7691b06f6555abfeb8c9817af8be8831f237e45acd02c5e116353d03551fd8f9a2c68e45ca04c79f6f15b6ad2db3997fe39639be7a605a91330698a1c0093492b6246771c845007063771407e8e727891eb20109a91c2439d5ab8b4d15b40aeba4a45effccb573d95810d60ea72991b9e8c14743", "b98c67c215bd448f", "fabec539007d1034d726c86b9c3a23cde67a9b75fc3947249fc2d0a17b8f2ab53451d0135675f6ad325b55dd785729763a12917c1a26f88938703800149e259b5d58c705f979d04af47aebdd597a1ecffcf00fecb91ee9e5efe09f07cefe2a1f727d83495822cb77f4de2c089aea6429b1491e243192b7044259405278e4b98d", "ac180b22e20faf2c5c2fcd51d82b1baa9683b6afeb7d1f6d748219c4470daf314675dde0ba15d078379989f0a64fdfa75aa05de6b6d090efeeb088a1809e80b0bb8df6d450432bc5772f5b263d1a1101b99cf24fb64989fa532b6ee163a8c0d2a62b4cecf2f98553f4d3387d0fab7f8d69f644db4ac066d6ab69abab38166025", 0 },
        { "modmul-p256", "73d23184973f798626b1cffc070d710920859634fe3c9c8f2b855c1f28aaca51", "7d7aaa4b988af3fbd39630d69c9011ef256badf9a7e6529bce76e9f477216e9e", "ffffffff00000001000000000000000000000000ffffffffffffffffffffffff", "50bd14a481445a24856a0c5825bfe62b7bd778c805189a8f62f8778beecea0b6", 1 },
    };
    void *ctx = ctx_new();
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        void *a = NULL, *b = NULL, *n = NULL, *r = bn_new();
        hex2bn(&a, t[i].a); hex2bn(&b, t[i].b); hex2bn(&n, t[i].n);
        int ok = t[i].mul ? modmul(r, a, b, n, ctx) : modexp(r, a, b, n, ctx);
        char *got = tohex(r);
        char up[1200] = ""; for (size_t k = 0; got && got[k] && k < sizeof(up) - 1; k++) up[k] = got[k] >= 'A' && got[k] <= 'F' ? got[k] + 32 : got[k];
        const char *g = up; while (*g == '0' && g[1]) g++;
        tl_log_line("BN %-24s %s", t[i].name, ok && !strcmp(g, t[i].expect) ? "ok" : "WRONG");
    }
}

/* TL_EC_TEST=1: elliptic-curve work (P-256 point multiplication and ECDSA verification) against values computed elsewhere. */
static void ec_selftest(void)
{
    tl_lib *L = tl_ld_find_lib("libcocos2dcpp.so");
    if (!L) return;
    typedef void *(*p0)(void); typedef void *(*key_new)(int); typedef int (*hex_fn)(void **, const char *);
    typedef int (*setpub)(void *, void *, void *); typedef void *(*sig_new)(void); typedef int (*sig_set0)(void *, void *, void *); typedef int (*verify)(const unsigned char *, int, void *, void *);
    typedef void *(*grp_new)(int); typedef void *(*pt_new)(const void *); typedef int (*pt_mul)(const void *, void *, const void *, const void *, const void *, void *); typedef int (*getxy)(const void *, const void *, void *, void *, void *);
    typedef char *(*tohex_fn)(const void *);
    p0 bn_new = tl_ld_sym(L, "BN_new"), ctx_new = tl_ld_sym(L, "BN_CTX_new"); hex_fn hex2bn = tl_ld_sym(L, "BN_hex2bn"); tohex_fn tohex = tl_ld_sym(L, "BN_bn2hex");
    key_new eckey = tl_ld_sym(L, "EC_KEY_new_by_curve_name"); setpub setaff = tl_ld_sym(L, "EC_KEY_set_public_key_affine_coordinates");
    sig_new signew = tl_ld_sym(L, "ECDSA_SIG_new"); sig_set0 set0 = tl_ld_sym(L, "ECDSA_SIG_set0"); verify ver = tl_ld_sym(L, "ECDSA_do_verify");
    grp_new grp = tl_ld_sym(L, "EC_GROUP_new_by_curve_name"); pt_new ptnew = tl_ld_sym(L, "EC_POINT_new"); pt_mul ptmul = tl_ld_sym(L, "EC_POINT_mul"); getxy getaff = tl_ld_sym(L, "EC_POINT_get_affine_coordinates_GFp");
    if (!bn_new || !ctx_new || !hex2bn || !eckey || !setaff || !signew || !set0 || !ver || !grp || !ptnew || !ptmul || !getaff || !tohex) { tl_log_line("EC test: functions missing"); return; }
    void *ctx = ctx_new();
    /* k*G */
    {
        void *g = grp(415), *pt = ptnew(g), *k = NULL, *x = bn_new(), *y = bn_new();
        hex2bn(&k, "244caf9c4dabb4817253edc6181879932fa91425cb0088539d2c67eda13ffe7a");
        int ok = ptmul(g, pt, k, NULL, NULL, ctx) && getaff(g, pt, x, y, ctx);
        char *xs = tohex(x); char low[100] = ""; for (int i = 0; xs && xs[i] && i < 98; i++) low[i] = xs[i] >= 'A' && xs[i] <= 'F' ? xs[i] + 32 : xs[i];
        const char *g0 = low; while (*g0 == '0' && g0[1]) g0++;
        tl_log_line("EC k*G (P-256)         %s", ok && !strcmp(g0, "c9ac0a07ac58f14093154ad072c0c2aa7a29f0d760041f2925a10c38c345fb7d") ? "ok" : "WRONG");
    }
    /* ECDSA verify */
    {
        void *key = eckey(415), *qx = NULL, *qy = NULL, *r = NULL, *s = NULL;
        hex2bn(&qx, "c688edd55bc87c3434993031cafe1046172eb501a7eebd60e66a6f31ddf14b6a"); hex2bn(&qy, "ea857163040eb260dff5a73e041010c16089b2bd4354b7827587268bdb5d5e19"); hex2bn(&r, "61a3fb03b5d645cf2294481c41f334a670f1cf3815052e02c6ee5cf803692eeb"); hex2bn(&s, "34ca90f0454c9651ae5e64ce77a7f7532a70ebc1ee96c3367e01f3356b1bcb05");
        setaff(key, qx, qy);
        void *sig = signew(); set0(sig, r, s);
        static const unsigned char h[32] = {0x07, 0x8f, 0x70, 0xa6, 0xd5, 0x18, 0x08, 0xa3, 0x9c, 0x2a, 0x01, 0x2a, 0xdf, 0x81, 0xdd, 0x9a, 0x0e, 0x68, 0xb9, 0xfe, 0x53, 0x95, 0xfb, 0xb9, 0xe2, 0x6d, 0x35, 0xfb, 0x0b, 0x51, 0x2e, 0x53};
        int v = ver(h, 32, sig, key);
        tl_log_line("EC ECDSA verify         %s (returned %d)", v == 1 ? "ok" : "WRONG", v);
    }
}

static void ec_bisect(void)
{
    tl_lib *L = tl_ld_find_lib("libcocos2dcpp.so");
    if (!L) return;
    typedef void *(*p0)(void); typedef int (*hex_fn)(void **, const char *); typedef void *(*grp_new)(int); typedef void *(*pt_new)(const void *);
    typedef int (*pt_mul)(const void *, void *, const void *, const void *, const void *, void *); typedef int (*getxy)(const void *, const void *, void *, void *, void *);
    typedef int (*setxy)(const void *, void *, const void *, const void *, void *); typedef int (*oncurve)(const void *, const void *, void *); typedef char *(*tohex_fn)(const void *);
    typedef const void *(*gen_fn)(const void *); typedef int (*dbl_fn)(const void *, void *, const void *, void *); typedef int (*add_fn)(const void *, void *, const void *, const void *, void *);
    p0 bn_new = tl_ld_sym(L, "BN_new"), ctx_new = tl_ld_sym(L, "BN_CTX_new"); hex_fn hex2bn = tl_ld_sym(L, "BN_hex2bn"); tohex_fn tohex = tl_ld_sym(L, "BN_bn2hex");
    grp_new grp = tl_ld_sym(L, "EC_GROUP_new_by_curve_name"); pt_new ptnew = tl_ld_sym(L, "EC_POINT_new"); pt_mul ptmul = tl_ld_sym(L, "EC_POINT_mul"); getxy getaff = tl_ld_sym(L, "EC_POINT_get_affine_coordinates_GFp");
    setxy setaff = tl_ld_sym(L, "EC_POINT_set_affine_coordinates_GFp"); oncurve onc = tl_ld_sym(L, "EC_POINT_is_on_curve"); gen_fn getgen = tl_ld_sym(L, "EC_GROUP_get0_generator");
    dbl_fn dbl = tl_ld_sym(L, "EC_POINT_dbl"); add_fn padd = tl_ld_sym(L, "EC_POINT_add");
    void *ctx = ctx_new(), *g = grp(415);
    const void *gen = getgen(g);
    tl_log_line("EC method: is_on_curve(generator) = %d", onc(g, gen, ctx));
    { void *x = bn_new(), *y = bn_new(); getaff(g, gen, x, y, ctx); char *xs = tohex(x); tl_log_line("EC generator x = %s", xs ? xs : "?"); }
    { void *d = ptnew(g); dbl(g, d, gen, ctx); void *x = bn_new(), *y = bn_new(); getaff(g, d, x, y, ctx); char *xs = tohex(x); tl_log_line("EC 2G by dbl x = %s", xs ? xs : "?"); }
    { void *d = ptnew(g); padd(g, d, gen, gen, ctx); void *x = bn_new(), *y = bn_new(); getaff(g, d, x, y, ctx); char *xs = tohex(x); tl_log_line("EC 2G by add x = %s", xs ? xs : "?"); }
    static const struct { const char *k, *x; } t[] = {
        { "1", "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296" },
        { "2", "7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978" },
        { "3", "5ecbe4d1a6330a44c8f7ef951d4bf165e6c6b721efada985fb41661bc6e7fd6c" },
        { "5", "51590b7a515140d2d784c85608668fdfef8c82fd1f5be52421554a0dc3d033ed" },
        { "10", "76a94d138a6b41858b821c629836315fcd28392eff6ca038a5eb4787e1277c6e" },
        { "ff", "f44b39759a2e6db723a6f90249972dfd08e95380f1fca470eacd1d03e5edf214" },
        { "10001", "1a32b7207dface203fdee0ce2a4c7e1d33d4003d227b23a5404751a0106a9bb7" },
        { "1000000000000000d", "d7ebeb3632e10ac37ae45541da036927a1230dff51e980f4e5684b7c3417471d" },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        void *k = NULL, *pt = ptnew(g), *x = bn_new(), *y = bn_new();
        hex2bn(&k, t[i].k);
        int ok = ptmul(g, pt, k, NULL, NULL, ctx) && getaff(g, pt, x, y, ctx);
        char *xs = tohex(x); char low[100] = ""; for (int j = 0; xs && xs[j] && j < 98; j++) low[j] = xs[j] >= 'A' && xs[j] <= 'F' ? xs[j] + 32 : xs[j];
        const char *g0 = low; while (*g0 == '0' && g0[1]) g0++;
        tl_log_line("EC k=%-18s %s", t[i].k, ok && !strcmp(g0, t[i].x) ? "ok" : "WRONG");
    }
}

/* TL_BN2_TEST=1: more big-number operations, on 256-bit values. */
static void bn2_selftest(void)
{
    tl_lib *L = tl_ld_find_lib("libcocos2dcpp.so");
    if (!L) return;
    typedef void *(*p0)(void); typedef int (*hex_fn)(void **, const char *); typedef char *(*tohex_fn)(const void *);
    typedef int (*f3)(void *, const void *, const void *, void *); typedef int (*f2)(void *, const void *);
    typedef int (*shift)(void *, const void *, int); typedef int (*f3n)(void *, const void *, const void *); typedef int (*divf)(void *, void *, const void *, const void *, void *);
    typedef int (*modf)(void *, const void *, const void *, void *); typedef void *(*invf)(void *, const void *, const void *, void *); typedef int (*mm)(void *, const void *, const void *, const void *, void *);
    p0 bn_new = tl_ld_sym(L, "BN_new"), ctx_new = tl_ld_sym(L, "BN_CTX_new"); hex_fn hex2bn = tl_ld_sym(L, "BN_hex2bn"); tohex_fn tohex = tl_ld_sym(L, "BN_bn2hex");
    f3 mul = tl_ld_sym(L, "BN_mul"); f2 sqr_ = tl_ld_sym(L, "BN_sqr"); divf dv = tl_ld_sym(L, "BN_div"); modf nnmod = tl_ld_sym(L, "BN_nnmod");
    shift rsh = tl_ld_sym(L, "BN_rshift"), lsh = tl_ld_sym(L, "BN_lshift"); f2 rsh1 = tl_ld_sym(L, "BN_rshift1"); f3n add = tl_ld_sym(L, "BN_add"), sub = tl_ld_sym(L, "BN_sub");
    invf inv = tl_ld_sym(L, "BN_mod_inverse"); modf msqr = tl_ld_sym(L, "BN_mod_sqr"); mm madd = tl_ld_sym(L, "BN_mod_add"), msub = tl_ld_sym(L, "BN_mod_sub");
    typedef int (*sqrf)(void *, const void *, void *); sqrf sqr = tl_ld_sym(L, "BN_sqr");
    void *ctx = ctx_new();
    static const struct { const char *name, *op, *a, *b, *m, *expect; } t[] = {
        { "mul-256x256", "mul", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "c6a5387777330bdbd7210dff076ce2ef87b0b125ec1d7da0a6eb8c9ebd69fe29", "0", "a7298ae897f21c1305add4351f4f8394323d08d63cca7d4af26c0a3d34a4b47ca1d886488ed56de04c72b679bb0895d140686f08145cb6aaca7a1d6002935d0d" },
        { "sqr-256", "sqr", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "0", "0", "b548b57e7e91092d48461dfe1fba6028b54f59803fcc6699df5d56c0511b4b6b67b24fa2ac064fec1f10c392743c8dfc33a75269721a143677b42bee81eeea99" },
        { "mul-256x128", "mul", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "617959ce3f1f65a8de5271007814e8a2", "0", "520684acd8f35ace4213bc2a97edf0e0a5648cd5baff0c69762109600f244f0ad3c3039e6ee22edd9df9817c56fd2baa" },
        { "div-512/256", "div", "a7298ae897f21c1305add4351f4f8394323d08d63cca7d4af26c0a3d34a4b47ca1d886488ed56de04c72b679bb0895d140686f08145cb6aaca7a1d6002935d0d", "df2dd97f1cfb10f62827688de6a16a3b0d464138a62332553fc1ea36f17fd375", "0", "bfbecbb9cb773d527ff7aa1ec9dcb6f020b9b150a204e37b859dcf3133d0a5bc" },
        { "mod-512%256", "mod", "a7298ae897f21c1305add4351f4f8394323d08d63cca7d4af26c0a3d34a4b47ca1d886488ed56de04c72b679bb0895d140686f08145cb6aaca7a1d6002935d0d", "df2dd97f1cfb10f62827688de6a16a3b0d464138a62332553fc1ea36f17fd375", "0", "ab556e00dee76c9290c470b6cbae873b93b95599b2407eb027effe2759aa21" },
        { "rshift-1", "rsh1", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "0", "0", "6bb6a19878a235f558608fef65c8e71bade47dde5ef2e04ca0b26c1ccfbb3e22" },
        { "lshift-77", "lsh", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "77", "0", "1aeda8661e288d7d561823fbd97239c6eb791f7797bcb813282c9b0733eecf88a0000000000000000000" },
        { "rshift-77", "rsh", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "77", "0", "6bb6a19878a235f558608fef65c8e71bade47dde5ef2e" },
        { "add-256", "add", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "c6a5387777330bdbd7210dff076ce2ef87b0b125ec1d7da0a6eb8c9ebd69fe29", "0", "19e127ba8687777c687e22dddd2feb126e379ace2aa033e39e85064d85ce07a6e" },
        { "sub-256", "sub", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "c6a5387777330bdbd7210dff076ce2ef87b0b125ec1d7da0a6eb8c9ebd69fe29", "0", "10c80ab97a11600ed9a011dfc424eb47d4184a96d1c842f89a794b9ae20c7e1c" },
        { "modinv-256", "inv", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "df2dd97f1cfb10f62827688de6a16a3b0d464138a62332553fc1ea36f17fd375", "0", "ccf8b98ec6fc51fc0647e7b16368e2500f3884ca68fb873a9fe2fe6b5df8596f" },
        { "modsqr-256", "msqr", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "df2dd97f1cfb10f62827688de6a16a3b0d464138a62332553fc1ea36f17fd375", "0", "2e1ce3447e91480e118a9d198b5912778c97380b7299de613dc6f492f693ea61" },
        { "modadd-256", "madd", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "c6a5387777330bdbd7210dff076ce2ef87b0b125ec1d7da0a6eb8c9ebd69fe29", "df2dd97f1cfb10f62827688de6a16a3b0d464138a62332553fc1ea36f17fd375", "bee4a2294b7c66d05fbac54fec5d46ebd6336baa03e00be4a88e7aa16b60a6f9" },
        { "modsub-256", "msub", "d76d4330f1446beab0c11fdecb91ce375bc8fbbcbde5c0994164d8399f767c45", "c6a5387777330bdbd7210dff076ce2ef87b0b125ec1d7da0a6eb8c9ebd69fe29", "df2dd97f1cfb10f62827688de6a16a3b0d464138a62332553fc1ea36f17fd375", "10c80ab97a11600ed9a011dfc424eb47d4184a96d1c842f89a794b9ae20c7e1c" },
    };
    (void)sqr_; (void)rsh1;
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        void *a = NULL, *b = NULL, *m = NULL, *r = bn_new(), *r2 = bn_new();
        hex2bn(&a, t[i].a); if (t[i].op[0] != 'l' && strcmp(t[i].op, "rsh")) hex2bn(&b, t[i].b); hex2bn(&m, t[i].m);
        int sh = atoi(t[i].b);
        const char *o = t[i].op; int ok = 0;
        if (!strcmp(o, "mul")) ok = mul(r, a, b, ctx);
        else if (!strcmp(o, "sqr")) ok = sqr(r, a, ctx);
        else if (!strcmp(o, "div")) { void *mm2 = NULL; hex2bn(&mm2, t[i].b); ok = dv(r, r2, a, mm2, ctx); }
        else if (!strcmp(o, "mod")) { void *mm2 = NULL; hex2bn(&mm2, t[i].b); ok = nnmod(r, a, mm2, ctx); }
        else if (!strcmp(o, "rsh1")) ok = rsh1(r, a);
        else if (!strcmp(o, "lsh")) ok = lsh(r, a, sh);
        else if (!strcmp(o, "rsh")) ok = rsh(r, a, sh);
        else if (!strcmp(o, "add")) ok = add(r, a, b);
        else if (!strcmp(o, "sub")) ok = sub(r, a, b);
        else if (!strcmp(o, "inv")) { void *mm2 = NULL; hex2bn(&mm2, t[i].b); ok = inv(r, a, mm2, ctx) != NULL; }
        else if (!strcmp(o, "msqr")) { void *mm2 = NULL; hex2bn(&mm2, t[i].b); ok = msqr(r, a, mm2, ctx); }
        else if (!strcmp(o, "madd")) ok = madd(r, a, b, m, ctx);
        else if (!strcmp(o, "msub")) ok = msub(r, a, b, m, ctx);
        char *got = tohex(r); char up[1400] = ""; for (size_t k = 0; got && got[k] && k < sizeof(up) - 1; k++) up[k] = got[k] >= 'A' && got[k] <= 'F' ? got[k] + 32 : got[k];
        const char *g = up; while (*g == '0' && g[1]) g++;
        tl_log_line("BN2 %-14s %s", t[i].name, (ok || !strcmp(t[i].expect, "-1")) && (!strcmp(g, t[i].expect) || !strcmp(t[i].expect, "-1")) ? "ok" : "WRONG");
    }
}

static void ec_bisect2(void)
{
    tl_lib *L = tl_ld_find_lib("libcocos2dcpp.so");
    if (!L) return;
    typedef void *(*p0)(void); typedef int (*hex_fn)(void **, const char *); typedef void *(*grp_new)(int); typedef void *(*pt_new)(const void *);
    typedef int (*pt_mul)(const void *, void *, const void *, const void *, const void *, void *); typedef int (*getxy)(const void *, const void *, void *, void *, void *);
    typedef int (*setxy)(const void *, void *, const void *, const void *, void *); typedef int (*oncurve)(const void *, const void *, void *); typedef char *(*tohex_fn)(const void *);
    typedef const void *(*gen_fn)(const void *); typedef int (*dbl_fn)(const void *, void *, const void *, void *); typedef int (*add_fn)(const void *, void *, const void *, const void *, void *);
    p0 bn_new = tl_ld_sym(L, "BN_new"), ctx_new = tl_ld_sym(L, "BN_CTX_new"); hex_fn hex2bn = tl_ld_sym(L, "BN_hex2bn"); tohex_fn tohex = tl_ld_sym(L, "BN_bn2hex");
    grp_new grp = tl_ld_sym(L, "EC_GROUP_new_by_curve_name"); pt_new ptnew = tl_ld_sym(L, "EC_POINT_new"); pt_mul ptmul = tl_ld_sym(L, "EC_POINT_mul"); getxy getaff = tl_ld_sym(L, "EC_POINT_get_affine_coordinates_GFp");
    setxy setaff = tl_ld_sym(L, "EC_POINT_set_affine_coordinates_GFp"); oncurve onc = tl_ld_sym(L, "EC_POINT_is_on_curve"); gen_fn getgen = tl_ld_sym(L, "EC_GROUP_get0_generator");
    dbl_fn dbl = tl_ld_sym(L, "EC_POINT_dbl"); add_fn padd = tl_ld_sym(L, "EC_POINT_add");
    void *ctx = ctx_new(), *g = grp(415);
    const void *gen = getgen(g);
    tl_log_line("EC method: is_on_curve(generator) = %d", onc(g, gen, ctx));
    { void *x = bn_new(), *y = bn_new(); getaff(g, gen, x, y, ctx); char *xs = tohex(x); tl_log_line("EC generator x = %s", xs ? xs : "?"); }
    { void *d = ptnew(g); dbl(g, d, gen, ctx); void *x = bn_new(), *y = bn_new(); getaff(g, d, x, y, ctx); char *xs = tohex(x); tl_log_line("EC 2G by dbl x = %s", xs ? xs : "?"); }
    { void *d = ptnew(g); padd(g, d, gen, gen, ctx); void *x = bn_new(), *y = bn_new(); getaff(g, d, x, y, ctx); char *xs = tohex(x); tl_log_line("EC 2G by add x = %s", xs ? xs : "?"); }
    static const struct { const char *k, *x; } t[] = {
        { "400000000000000009", "14d20f76582649cd34f13c61f8fedc52b7df644dc6e5bf9f0026c33f2985ddc9" },
        { "1000000000000000000000001", "418d68dea064219700d1a0a5fd208dfb48f9c1875e98c12dc761c1fecc049786" },
        { "10000000000000000000000007", "71011f7c356a50a33565df118a881483651a60a6ad083368f2672e72be919fa3" },
        { "100000000000000000000000000000005", "cc652454ca186143ed3379b352f8a9652ce139ff3d59325a8d8540c09a42dc2c" },
        { "10000000000000000000000000000000000000003", "45fe8bad8a9c2e8ca96c27e46cddac3421c7868faa29d275fa312700da04a4e3" },
        { "100000000000000000000000000000000000000000000000b", "630a4f031b27a80684e115ab9956f630204174393888df692b6376dbb0ee74a4" },
        { "100000000000000000000000000000000000000000000000000000001", "cf6b116fa174d4d3024cd871d984505727f187b97eda11d3fd4c1473c0c1ce9f" },
        { "400000000000000000000000000000000000000000000000000000000000003", "2e4335aa63a024c282e814e13cafdf25a1255dcf96e91c56f6ccf70c76a9f2c7" },
        { "800000000000000000000000000000000000000000000000000000000000000b", "cac2ac7cba1a2583bdfb5859a1c4a5d32cadbb4c1d0c0889789c9c1a62d5c5be" },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        void *k = NULL, *pt = ptnew(g), *x = bn_new(), *y = bn_new();
        hex2bn(&k, t[i].k);
        int ok = ptmul(g, pt, k, NULL, NULL, ctx) && getaff(g, pt, x, y, ctx);
        char *xs = tohex(x); char low[100] = ""; for (int j = 0; xs && xs[j] && j < 98; j++) low[j] = xs[j] >= 'A' && xs[j] <= 'F' ? xs[j] + 32 : xs[j];
        const char *g0 = low; while (*g0 == '0' && g0[1]) g0++;
        tl_log_line("EC k=%-18s %s", t[i].k, ok && !strcmp(g0, t[i].x) ? "ok" : "WRONG");
    }
}

static void *start_thread(void *arg)
{
    pthread_setname_np("husk-native-start");
    if (!tl_cocos_start(arg) || !tl_cocos_run()) { fprintf(stderr, "cocos: start failed\n"); return NULL; }
    if (getenv("TL_START_THREAD_KEEP")) for (;;) pause();     /* what the app does now */
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <apk> [seconds] [width height]\n", argv[0]); return 2; }
    static uint8_t altstack[1 << 16];
    stack_t ss = { .ss_sp = altstack, .ss_size = sizeof(altstack) };
    sigaltstack(&ss, NULL);
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash; sa.sa_flags = SA_SIGINFO | SA_ONSTACK; sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL); sigaction(SIGILL, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL); sigaction(SIGABRT, &sa, NULL);

    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 1);
    tl_jni_set_trace(getenv("TL_JNI_TRACE") ? atoi(getenv("TL_JNI_TRACE")) : 1);
    char tmp[600] = "/Volumes/GTAV/husk2/tmp/husk-cocos-XXXXXX";
    if (getenv("TL_DATA")) { snprintf(tmp, sizeof(tmp), "%s", getenv("TL_DATA")); mkdir(tmp, 0755); }   /* kept between runs */
    else mkdtemp(tmp);
    const char *cef = "/Users/davi/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Chromium Embedded Framework.framework/Versions/A/Libraries";
    char egl[600], gles[600]; snprintf(egl, sizeof(egl), "%s/libEGL.dylib", cef); snprintf(gles, sizeof(gles), "%s/libGLESv2.dylib", cef);
    char frames[] = "/Volumes/GTAV/husk2/tmp/husk-cframes-XXXXXX"; mkdtemp(frames);
    fprintf(stderr, "frames: %s\ndata: %s\n", frames, tmp);
    int w = argc > 4 ? atoi(argv[3]) : 1200, h = argc > 4 ? atoi(argv[4]) : 552;
    /* TL_SPLITS: the app's split APKs, colon-separated (a Google Play install keeps libraries and asset packs in their own APKs) */
    if (getenv("TL_SPLITS")) { void tl_ld_queue_split(const char *); char *sl = strdup(getenv("TL_SPLITS")); for (char *t = strtok(sl, ":"); t; t = strtok(NULL, ":")) tl_ld_queue_split(t); }
    tl_cocos_config cfg = { .apk_path = argv[1], .data_dir = tmp, .package_name = getenv("TL_PACKAGE") ? getenv("TL_PACKAGE") : "com.robtopx.geometryjump", .width = w, .height = h,
                            .angle_egl = getenv("TL_ANGLE_EGL") ? getenv("TL_ANGLE_EGL") : egl, .angle_gles = getenv("TL_ANGLE_GLES") ? getenv("TL_ANGLE_GLES") : gles,
                            .frame_dir = frames, .frame_every = getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : -6 };
    g_frame_dir = frames;
    tl_cocos_text_install();
    if (getenv("TL_AUDIO_STATS")) tl_cocos_audio_hook = stats_hook;
    if (getenv("TL_AUDIO")) tl_audio_install();            /* off by default: a test run should not play through the speakers */
    /* TL_GEODE=<Geode.android64.so> [TL_GEODE_LAUNCHER=<geode launcher apk>]: load Geode into the game, as its launcher does. */
    if (getenv("TL_GEODE_RESOURCES")) {                    /* Geode's own resources, where its launcher unpacks them */
        char cmd[2400];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/geode/game/geode/resources/geode.loader' && cp -R '%s'/. '%s/geode/game/geode/resources/geode.loader/'",
                 tmp, getenv("TL_GEODE_RESOURCES"), tmp);
        if (system(cmd)) fprintf(stderr, "geode: could not copy the resources\n");
    }
    if (getenv("TL_GEODE")) tl_geode_configure(getenv("TL_GEODE"), getenv("TL_GEODE_LAUNCHER"), cfg.data_dir, getenv("TL_GD_VERSION") ? atoi(getenv("TL_GD_VERSION")) : 40);
    /* TL_START_THREAD=1: start the game on a thread that ends once it has, as the app's launch thread did before it was kept
     * (a Geode 5 crash: thread-local cleanup on that thread's exit). */
    if (getenv("TL_START_THREAD")) {
        static tl_cocos_config c2; c2 = cfg;
        pthread_t st;
        pthread_create(&st, NULL, start_thread, &c2);
        if (getenv("TL_START_THREAD_KEEP")) pthread_detach(st);
        else { pthread_join(st, NULL); fprintf(stderr, "cocos: the start thread has ended\n"); }
    } else {
        if (!tl_cocos_start(&cfg)) { fprintf(stderr, "cocos: start failed\n"); return 1; }
        if (!tl_cocos_run()) { fprintf(stderr, "cocos: run failed\n"); return 1; }
    }
    if (getenv("TL_SSL_PROBE")) install_ssl_probes();
    if (getenv("TL_CRYPTO_TEST")) crypto_selftest();
    if (getenv("TL_BN_TEST")) bn_selftest();
    if (getenv("TL_EC_TEST")) ec_selftest();
    if (getenv("TL_EC_BISECT")) ec_bisect();
    if (getenv("TL_BN2_TEST")) bn2_selftest();
    if (getenv("TL_EC_BISECT2")) ec_bisect2();
    if (getenv("TL_DUMP_VADDR")) dump_vaddr(getenv("TL_DUMP_VADDR"));
    if (getenv("TL_CTL")) { static pthread_t ct; pthread_create(&ct, NULL, control_thread, getenv("TL_CTL")); }
    int secs = argc > 2 ? atoi(argv[2]) : 5;
    for (int i = 0; i < secs && !tl_cocos_ended(); i++) sleep(1);
    fprintf(stderr, "cocos: %lu frames in %d s\n", tl_cocos_frames(), secs);
    tl_cocos_stop();
    return 0;
}
