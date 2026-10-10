/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host harness for the NativeActivity driver (Unreal Engine 4 games such as ARK): a copy of ga-test.c with the Minecraft-only probes removed. */
/*
 * Host harness for the GameActivity driver: runs an APK's GameActivity game (Minecraft) on a Mac, off-screen.
 *
 *   ga-test <apk> [seconds] [width height]
 *
 * Landscape by default (the phone's aspect). Environment: TL_JNI_TRACE=1|2, TL_VERBOSE=0..2,
 * TL_CTL=<fifo> (lines "tap X Y", "hold X Y MS", "swipe X1 Y1 X2 Y2 MS", "wait MS", "shot PNG", "quit").
 */
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/thread_act.h>
#include <mach/arm/thread_status.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-gameactivity.h"
#include "husk-tl-sdl.h"
#include "husk-tl-gamepad.h"
#include "husk-tl-audio.h"
#include "husk-tl-xmem.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"
#include "husk-tl-vulkan.h"
#include "husk-tl-vulkan.h"

void tl_log_line(const char *fmt, ...)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static struct timespec t0;
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    pthread_mutex_lock(&m);
    if (!t0.tv_sec) t0 = t;
    char line[4096];
    int n = 0;
    if (getenv("TL_LOG_TIME")) n = snprintf(line, sizeof(line), "[%7.3f] ", (t.tv_sec - t0.tv_sec) + (t.tv_nsec - t0.tv_nsec) / 1e9);
    va_list ap; va_start(ap, fmt);
    int w = vsnprintf(line + n, sizeof(line) - (size_t)n - 2, fmt, ap);
    va_end(ap);
    if (w > (int)(sizeof(line) - (size_t)n - 3)) w = (int)(sizeof(line) - (size_t)n - 3);
    n += w;
    line[n++] = '\n';
    fwrite(line, 1, (size_t)n, stderr);
    pthread_mutex_unlock(&m);
}

void tl_egl_recent_calls(char *out, size_t n, int count);

static void describe(const char *label, const void *addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &lib, &sa);
    if (lib) fprintf(stderr, "  %-6s %p  %s  %s+%#lx\n", label, addr, lib, sym ? sym : "?", sa ? (unsigned long)((const char *)addr - (const char *)sa) : 0ul);
    else {
        Dl_info di;
        if (dladdr(addr, &di) && di.dli_fname) fprintf(stderr, "  %-6s %p  [host] %s  %s+%#lx\n", label, addr, strrchr(di.dli_fname, '/') ? strrchr(di.dli_fname, '/') + 1 : di.dli_fname, di.dli_sname ? di.dli_sname : "?", di.dli_saddr ? (unsigned long)((const char *)addr - (const char *)di.dli_saddr) : 0ul);
        else fprintf(stderr, "  %-6s %p\n", label, addr);
    }
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
    if (info->si_addr) {
        vm_address_t ra = (vm_address_t)info->si_addr; vm_size_t rs = 0; vm_region_basic_info_data_64_t bi; mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64; mach_port_t obj;
        kern_return_t kr = vm_region_64(mach_task_self(), &ra, &rs, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&bi, &cnt, &obj);
        if (kr == KERN_SUCCESS) fprintf(stderr, "  region at/after the fault: [%#lx, %#lx) prot %d max %d%s\n", (unsigned long)ra, (unsigned long)(ra + rs), bi.protection, bi.max_protection, ra > (vm_address_t)info->si_addr ? " (the fault itself is in an unmapped gap before it)" : " (the fault is inside it)");
        else fprintf(stderr, "  no region at or after the fault address\n");
    }
    uintptr_t fp = ss->__fp;
    for (int i = 0; i < 16 && fp && (fp & 7) == 0; i++) {
        uint64_t fr[2];
        if (!safe_read(fp, fr, sizeof(fr))) break;
        describe("frame", (void *)fr[1]);
        fp = fr[0];
    }
    uint64_t *sp = (uint64_t *)ss->__sp; int shown = 0;
    for (int i = 0; i < 8192 && shown < 24; i++) {
        uint64_t v;
        if (!safe_read((uintptr_t)(sp + i), &v, 8)) break;
        if (v > 0x7000000000ull && v < 0x7100000000ull && tl_ld_lib_of((void *)v) && (v & 3) == 0) { describe("stk", (void *)v); shown++; }
    }
    for (int i = 0; i < 29; i += 4)
        fprintf(stderr, "  x%d=%#llx x%d=%#llx x%d=%#llx x%d=%#llx\n", i, ss->__x[i], i + 1, ss->__x[i + 1], i + 2, i + 2 < 29 ? ss->__x[i + 2] : 0, i + 3, i + 3 < 29 ? ss->__x[i + 3] : 0);
    fprintf(stderr, "  sp=%#llx fp=%#llx\n", ss->__sp, ss->__fp);
    { char gl[2048]; tl_egl_recent_calls(gl, sizeof(gl), 40); if (gl[0]) fprintf(stderr, "  last GL calls: %s\n", gl); }
    fflush(stderr);
    _exit(139);
}

static uintptr_t lib_bias(const char *name);
static int safe_read(uintptr_t addr, void *out, size_t n);
static void eos_result_probe(uint64_t *r) { tl_log_line("EOS_Initialize returned %d (0 = success, 10 = invalid parameters, 15 = already configured, 7 = no connection)", (int)r[0]); }
static void rd_cstr(uint64_t p, char *out, size_t cap)
{
    out[0] = 0;
    for (size_t k = 0; p && k + 1 < cap; k++) { char c; if (!safe_read((uintptr_t)p + k, &c, 1) || !c) break; out[k] = c; out[k + 1] = 0; }
}
static void eos_platform_probe(uint64_t *r)
{
    uint64_t w[16] = { 0 };
    if (!safe_read((uintptr_t)r[0], w, sizeof(w))) { tl_log_line("EOS_Platform_Create: options unreadable"); return; }
    char prod[100], sand[100], cid[100], dep[100], cache[160];
    rd_cstr(w[2], prod, sizeof(prod)); rd_cstr(w[3], sand, sizeof(sand)); rd_cstr(w[4], cid, sizeof(cid)); rd_cstr(w[10], dep, sizeof(dep)); rd_cstr(w[12], cache, sizeof(cache));
    tl_log_line("EOS_Platform_Create: ApiVersion=%u ProductId='%s' SandboxId='%s' ClientId='%s' (secret %s) DeploymentId='%s' Flags=%#llx Cache='%s'",
                (unsigned)(w[0] & 0xffffffff), prod, sand, cid, w[5] ? "set" : "null", dep, (unsigned long long)w[11], cache);
}
static void eos_init_probe(uint64_t *r)
{
    uint64_t w[10] = { 0 };
    if (!safe_read((uintptr_t)r[0], w, sizeof(w))) { tl_log_line("EOS_Initialize: options unreadable"); return; }
    char name[80] = "", ver[80] = "";
    for (int k = 0; k < 79; k++) { char c; if (!w[2] || !safe_read((uintptr_t)w[4] + (uintptr_t)k, &c, 1) || !c) break; name[k] = c; name[k + 1] = 0; }
    for (int k = 0; k < 79; k++) { char c; if (!w[5] || !safe_read((uintptr_t)w[5] + (uintptr_t)k, &c, 1) || !c) break; ver[k] = c; ver[k + 1] = 0; }
    uint64_t sys[4] = { 0 };
    if (w[7]) safe_read((uintptr_t)w[7], sys, sizeof(sys));
    tl_log_line("EOS_Initialize: ApiVersion=%u ProductName='%s' ProductVersion='%s' Reserved=%#llx System=%#llx{api=%u, %#llx, %#llx, %#llx} Affinity=%#llx",
                (unsigned)(w[0] & 0xffffffff), name, ver, (unsigned long long)w[6], (unsigned long long)w[7], (unsigned)(sys[0] & 0xffffffff), (unsigned long long)sys[1], (unsigned long long)sys[2], (unsigned long long)sys[3], (unsigned long long)w[8]);
}

/* TL_PROBE=lib:symbol,lib:symbol,...: say each time one of those functions is entered, with its first four arguments (and a string, if one looks like a pointer to one).
 * For finding which of the engine's start-up steps are reached when its own log is compiled out. */
static char g_probe_name[16][120];
static void probe_log(int n, uint64_t *r)
{
    char str[2][60] = { "", "" };
    for (int k = 0; k < 2; k++) {
        const char *c = (const char *)r[k];
        if ((uintptr_t)c > 0x100000 && ((uintptr_t)c >> 40) < 0x1000) {
            unsigned char b[8];
            if (safe_read((uintptr_t)c, b, 8) && b[0] >= 32 && b[0] < 127 && b[1] >= 32 && b[1] < 127) { int j = 0; while (j < 56 && safe_read((uintptr_t)c + (uintptr_t)j, b, 1) && b[0] >= 32 && b[0] < 127) { str[k][j] = (char)b[0]; j++; } str[k][j] = 0; }
        }
    }
    tl_log_line("PROBE %s(%#llx, %#llx, %#llx, %#llx)%s%s%s%s", g_probe_name[n], (unsigned long long)r[0], (unsigned long long)r[1], (unsigned long long)r[2], (unsigned long long)r[3],
                str[0][0] ? " x0=\"" : "", str[0], str[0][0] ? "\"" : "", str[1][0] ? " (x1 is a string)" : "");
}
#define PCB(n) static void probe_cb##n(uint64_t *r) { probe_log(n, r); }
PCB(0) PCB(1) PCB(2) PCB(3) PCB(4) PCB(5) PCB(6) PCB(7) PCB(8) PCB(9) PCB(10) PCB(11) PCB(12) PCB(13) PCB(14) PCB(15)
static void (*const g_probe_cbs[16])(uint64_t *) = { probe_cb0, probe_cb1, probe_cb2, probe_cb3, probe_cb4, probe_cb5, probe_cb6, probe_cb7, probe_cb8, probe_cb9, probe_cb10, probe_cb11, probe_cb12, probe_cb13, probe_cb14, probe_cb15 };
static void install_probes(const char *spec)
{
    char buf[2000]; snprintf(buf, sizeof(buf), "%s", spec);
    int n = 0;
    for (char *tok = strtok(buf, ","); tok && n < 16; tok = strtok(NULL, ",")) {
        char *colon = strchr(tok, ':'); if (!colon) continue;
        *colon = 0;
        tl_lib *L = tl_ld_find_lib(tok);
        uintptr_t a = L ? (uintptr_t)tl_ld_sym(L, colon + 1) : 0;
        snprintf(g_probe_name[n], sizeof(g_probe_name[n]), "%s", colon + 1);
        if (!a || !tl_ld_probe(L, a - lib_bias(tok), g_probe_cbs[n])) fprintf(stderr, "probe on %s:%s failed\n", tok, colon + 1); else n++;
    }
}

/* TL_BLR_PROBE=lib:lo-hi (hex vaddrs): before every `blr x9` in that range say where x9 points (library offset) and the object in x0. For finding which of a chain of virtual init calls is the last to run. */
static uintptr_t g_blr_bias;
#define BLRCB(n) static void blr_probe##n(uint64_t *r) { tl_log_line("BLR x" #n " -> %#llx (offset in the library), x0=%#llx", (unsigned long long)((uintptr_t)r[n] - g_blr_bias), (unsigned long long)r[0]); }
BLRCB(1) BLRCB(2) BLRCB(3) BLRCB(4) BLRCB(5) BLRCB(6) BLRCB(7) BLRCB(8) BLRCB(9) BLRCB(10) BLRCB(11) BLRCB(12) BLRCB(13) BLRCB(14) BLRCB(15) BLRCB(16) BLRCB(17) BLRCB(19) BLRCB(20) BLRCB(21) BLRCB(22) BLRCB(23) BLRCB(24) BLRCB(25) BLRCB(26) BLRCB(27) BLRCB(28)
static void (*const g_blr_cbs[29])(uint64_t *) = { 0, blr_probe1, blr_probe2, blr_probe3, blr_probe4, blr_probe5, blr_probe6, blr_probe7, blr_probe8, blr_probe9, blr_probe10, blr_probe11, blr_probe12, blr_probe13, blr_probe14, blr_probe15, blr_probe16, blr_probe17, 0, blr_probe19, blr_probe20, blr_probe21, blr_probe22, blr_probe23, blr_probe24, blr_probe25, blr_probe26, blr_probe27, blr_probe28 };
static void install_blr_probes(const char *spec)
{
    char buf[200]; snprintf(buf, sizeof(buf), "%s", spec);
    char *colon = strchr(buf, ':'); if (!colon) return; *colon = 0;
    unsigned long lo = 0, hi = 0; sscanf(colon + 1, "%lx-%lx", &lo, &hi);
    tl_lib *L = tl_ld_find_lib(buf); if (!L) return;
    g_blr_bias = lib_bias(buf);
    int n = 0;
    for (unsigned long a = lo; a < hi; a += 4) {
        uint32_t w; memcpy(&w, (const void *)(g_blr_bias + a), 4);
        if ((w & 0xFFFFFC1Fu) == 0xD63F0000u) { unsigned reg = (w >> 5) & 31; if (reg < 29 && g_blr_cbs[reg] && tl_ld_probe(L, a, g_blr_cbs[reg])) n++; }
    }
    fprintf(stderr, "BLR probes: %d\n", n);
}

/* TL_WINFLAGS_PROBE=<vaddr in libmain.so, hex>: at that instruction x0 is SDL_GetWindowFlags()'s result; say it each time it changes. */
static void winflags_probe(uint64_t *r)
{
    static uint64_t last = ~0ull;
    if (r[0] != last) { last = r[0]; tl_log_line("SDL window flags now %#llx (HIDDEN 0x8, MINIMIZED 0x40, INPUT_FOCUS 0x200, FULLSCREEN 0x1, OPENGL 0x2, MOUSE_FOCUS 0x400)", (unsigned long long)r[0]); }
}

static const char *g_frame_dir;
static void sleep_ms(long ms) { usleep((useconds_t)ms * 1000); }
static void do_swipe(float x1, float y1, float x2, float y2, long ms)
{
    int steps = (int)(ms / 16); if (steps < 2) steps = 2;
    tl_sdl_touch(0, 0, x1, y1);
    for (int i = 1; i <= steps; i++) { sleep_ms(ms / steps); tl_sdl_touch(1, 0, x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps); }
    tl_sdl_touch(2, 0, x2, y2);
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
            if (sscanf(line, "tap %f %f", &a, &b) == 2) { tl_sdl_touch(0, 0, a, b); sleep_ms(80); tl_sdl_touch(2, 0, a, b); }
            else if (sscanf(line, "hold %f %f %ld", &a, &b, &ms) == 3) { tl_sdl_touch(0, 0, a, b); sleep_ms(ms); tl_sdl_touch(2, 0, a, b); }
            else if (sscanf(line, "swipe %f %f %f %f %ld", &a, &b, &c, &d, &ms) == 5) do_swipe(a, b, c, d, ms);
            else if (sscanf(line, "wait %ld", &ms) == 1) sleep_ms(ms);
            else if (sscanf(line, "keyhold %f %ld", &a, &ms) == 2) { tl_sdl_key((int)a, true); sleep_ms(ms); tl_sdl_key((int)a, false); }   /* held for ms */
            else if (sscanf(line, "key %f", &a) == 1) { tl_sdl_key((int)a, true); sleep_ms(50); tl_sdl_key((int)a, false); }          /* an Android key code */
            else if (sscanf(line, "mouse %f %f", &a, &b) == 2) { tl_sdl_mouse(1, a, b); sleep_ms(50); tl_sdl_mouse(0, a, b); sleep_ms(80); tl_sdl_mouse(2, a, b); }
            else if (!strncmp(line, "pause", 5)) tl_sdl_set_paused(true);
            else if (!strncmp(line, "resume", 6)) tl_sdl_set_paused(false);
            else if (!strncmp(line, "type ", 5)) { char t[200]; snprintf(t, sizeof(t), "%s", line + 5); t[strcspn(t, "\r\n")] = 0; tl_sdl_commit_text(t); }
            else if (sscanf(line, "shot %399s", p) == 1) {
                char cmd[900]; snprintf(cmd, sizeof(cmd), "sips -s format png '%s/latest.bmp' --out '%s' >/dev/null 2>&1", g_frame_dir, p);
                if (system(cmd)) fprintf(stderr, "ctl: shot failed\n");
                else fprintf(stderr, "ctl: shot %s (frame %lu)\n", p, tl_sdl_frames());
            }
            /* a virtual controller: padon / padoff, "pad <buttons hex> lx ly rx ry lt rt" (y down, as Android), padtap <button bit> <ms> */
            else if (!strncmp(line, "padon", 5)) tl_pad_connect(0, "Xbox Wireless Controller");
            else if (!strncmp(line, "padoff", 6)) tl_pad_disconnect(0);
            else if (!strncmp(line, "padtap", 6) && sscanf(line + 6, "%f %ld", &a, &ms) == 2) {
                tl_pad_state st = { .buttons = 1u << (int)a }; tl_pad_update(0, &st); sleep_ms(ms); st.buttons = 0; tl_pad_update(0, &st);
            }
            else if (!strncmp(line, "pad ", 4)) {
                unsigned btn = 0; tl_pad_state st = { 0 };
                if (sscanf(line + 4, "%x %f %f %f %f %f %f", &btn, &st.lx, &st.ly, &st.rx, &st.ry, &st.lt, &st.rt) >= 1) { st.buttons = btn; tl_pad_update(0, &st); }
            }
            else if (!strncmp(line, "pause", 5)) tl_sdl_set_paused(true);
            else if (!strncmp(line, "resume", 6)) tl_sdl_set_paused(false);
            else if (!strncmp(line, "quit", 4)) { fprintf(stderr, "ctl: quit\n"); fflush(stderr); _exit(0); }
        }
        fclose(f);
    }
    return NULL;
}

static int g_ran;
static void tl_ran(void) { g_ran++; }

/* Bedrock's assertion handler builds its message just before it decides to crash on purpose (store 0xdeadc0de to
 * address 0). A probe there prints the message, which the game itself only logs after the crash. */
static void assert_probe(uint64_t *r)
{
    const char *msg = (const char *)r[5];
    tl_log_line("ASSERTION FAILED: %.1500s", msg && (uintptr_t)msg > 0x100000 ? msg : "(no message)");
}

/* Bedrock's soft assertion (message, expression, line, file, function): logged and carried on from, so a probe shows which ones the game hits.
 * 0x150df538 is its entry in libminecraftpe 1.26.60.29. */
static void soft_assert_probe(uint64_t *r)
{
    static int n;
    if (n++ > 60) return;
    tl_log_line("SOFT ASSERT: %.120s | %.100s | line %d | %.90s | %.90s", (const char *)r[0], (const char *)r[1], (int)r[2], (const char *)r[3], (const char *)r[4]);
}

/* TL_CXA_THROW=1: say what C++ exception each throw is (type name, thrower and the exception's first words), for finding what a terminate was about. */
static void throw_probe(uint64_t *r)
{
    const void *ti = (const void *)r[1];
    const char *tn = ti ? *(const char *const *)((const char *)ti + 8) : "?";
    const char *ex = (const char *)r[0], *msg = "";
    char shown[100]; shown[0] = 0;
    /* libc++'s logic_error/runtime_error keep a pointer to the message just after the vtable */
    if (ex) { const char *const *q = (const char *const *)ex; for (int i = 1; i <= 2; i++) { const char *c = q[i]; if ((uintptr_t)c > 0x100000 && ((uintptr_t)c >> 40) < 0x1000) { size_t k = 0; while (k < 90 && c[k] >= 32 && c[k] < 127) { shown[k] = c[k]; k++; } shown[k] = 0; if (k > 3) { msg = shown; break; } } } }
    uint64_t lr; __asm__ volatile("mov %0, x30" : "=r"(lr));
    tl_log_line("C++ THROW: %s %s", tn ? tn : "?", msg);
}

/* RenderDragon's bgfx callback: fatal(code in x1, message in x2) */
static void fatal_probe(uint64_t *r)
{
    const char *msg = (const char *)r[2];
    tl_log_line("BGFX FATAL %d: %.1500s", (int)r[1], msg && (uintptr_t)msg > 0x100000 ? msg : "(no message)");
}

/* TL_SAMPLE=<seconds>: every so often, say where every thread is (the guest library and offset of its pc and lr). For finding what a stuck game is waiting for. */
static void *sampler(void *arg)
{
    int period = (int)(intptr_t)arg;
    for (;;) {
        sleep((unsigned)period);
        thread_act_array_t th; mach_msg_type_number_t n;
        if (task_threads(mach_task_self(), &th, &n) != KERN_SUCCESS) continue;
        fprintf(stderr, "---- sample: %u threads ----\n", n);
        { char gl[1500]; tl_egl_recent_calls(gl, sizeof(gl), 24); if (gl[0]) fprintf(stderr, "  last GL calls: %s\n", gl); }
        for (mach_msg_type_number_t i = 0; i < n; i++) {
            arm_thread_state64_t st; mach_msg_type_number_t cnt = ARM_THREAD_STATE64_COUNT;
            pthread_t pt = pthread_from_mach_thread_np(th[i]);
            char name[40] = "";
            if (pt) pthread_getname_np(pt, name, sizeof(name));
            if (th[i] == mach_thread_self()) continue;
            thread_suspend(th[i]);
            if (thread_get_state(th[i], ARM_THREAD_STATE64, (thread_state_t)&st, &cnt) == KERN_SUCCESS) {
                const char *lib = NULL; const void *sa = NULL;
                const char *sym = tl_ld_symbol_at((void *)arm_thread_state64_get_pc(st), &lib, &sa);
                const char *lib2 = NULL; const void *sa2 = NULL;
                const char *sym2 = tl_ld_symbol_at((void *)arm_thread_state64_get_lr(st), &lib2, &sa2);
                Dl_info di; const char *host = "";
                if (!lib && dladdr((void *)arm_thread_state64_get_pc(st), &di) && di.dli_sname) host = di.dli_sname;
                fprintf(stderr, "  [%-24s] pc %s%s+%#lx  lr %s%s+%#lx\n", name, lib ? lib : "[host] ", lib ? sym ? sym : "?" : host,
                        lib && sa ? (unsigned long)(arm_thread_state64_get_pc(st) - (uintptr_t)sa) : 0ul,
                        lib2 ? lib2 : "", lib2 ? sym2 ? sym2 : "?" : "",
                        lib2 && sa2 ? (unsigned long)(arm_thread_state64_get_lr(st) - (uintptr_t)sa2) : 0ul);
            }
            if (getenv("TL_SAMPLE_BT") && strstr(name, getenv("TL_SAMPLE_BT"))) {
                uintptr_t fp = arm_thread_state64_get_fp(st);
                for (int d = 0; d < 14 && fp && (fp & 7) == 0; d++) {
                    uint64_t fr[2];
                    if (!safe_read(fp, fr, sizeof(fr))) break;
                    const char *l3 = NULL; const void *s3 = NULL;
                    const char *sy3 = tl_ld_symbol_at((void *)fr[1], &l3, &s3);
                    fprintf(stderr, "      frame %s %s+%#lx\n", l3 ? l3 : "?", l3 && sy3 ? sy3 : "?", l3 && s3 ? (unsigned long)(fr[1] - (uintptr_t)s3) : 0ul);
                    fp = fr[0];
                }
            }
            thread_resume(th[i]);
        }
    }
    return NULL;
}


struct bias_q { const char *name; uintptr_t bias; };
static int bias_cb(uintptr_t bias, const char *name, const void *ph, unsigned n, void *u)
{ (void)ph; (void)n; struct bias_q *q = u; if (!strcmp(name, q->name)) { q->bias = bias; return 1; } return 0; }
static uintptr_t lib_bias(const char *name) { struct bias_q q = { name, 0 }; tl_ld_iterate(bias_cb, &q); return q.bias; }

/* TL_PROF=<thread-name-part>[:delay[:seconds]]: samples that thread every 2 ms and prints which call sites its stacks pass through most,
 * and the commonest whole stacks (guest return addresses, as library+offset). For finding what a busy-looking thread is doing. */
typedef struct { char key[1200]; int n; } prof_chain;
static void *profiler(void *arg)
{
    char spec[200]; snprintf(spec, sizeof(spec), "%s", (const char *)arg);
    int delay = 15, secs = 6;
    char *c1 = strchr(spec, ':');
    if (c1) { *c1++ = 0; delay = atoi(c1); char *c2 = strchr(c1, ':'); if (c2) secs = atoi(c2 + 1); }
    sleep((unsigned)delay);
    static prof_chain chains[4000]; int nch = 0;
    static struct { uintptr_t addr; int n; } sites[8000]; int ns = 0;
    int total = 0;
    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
        if ((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000 >= secs * 1000) break;
        usleep(1000);
        thread_act_array_t th; mach_msg_type_number_t n;
        if (task_threads(mach_task_self(), &th, &n) != KERN_SUCCESS) continue;
        for (mach_msg_type_number_t i = 0; i < n; i++) {
            pthread_t pt = pthread_from_mach_thread_np(th[i]);
            char name[40] = "";
            if (pt) pthread_getname_np(pt, name, sizeof(name));
            if (th[i] == mach_thread_self() || !strstr(name, spec)) continue;
            arm_thread_state64_t st; mach_msg_type_number_t cnt = ARM_THREAD_STATE64_COUNT;
            thread_suspend(th[i]);
            if (thread_get_state(th[i], ARM_THREAD_STATE64, (thread_state_t)&st, &cnt) == KERN_SUCCESS) {
                uintptr_t addrs[40]; int na = 0;
                addrs[na++] = arm_thread_state64_get_pc(st);
                addrs[na++] = arm_thread_state64_get_lr(st);
                uintptr_t fp = arm_thread_state64_get_fp(st);
                for (int d = 0; d < 36 && fp && (fp & 7) == 0; d++) {
                    uint64_t fr[2];
                    if (!safe_read(fp, fr, sizeof(fr))) break;
                    addrs[na++] = fr[1]; fp = fr[0];
                }
                char key[1200]; size_t k = 0; key[0] = 0;
                k += (size_t)snprintf(key, sizeof(key), "[%s] ", name);
                for (int a = 0; a < na; a++) {
                    const char *l = NULL; const void *sb = NULL;
                    const char *sy = tl_ld_symbol_at((void *)addrs[a], &l, &sb);
                    (void)sy;
                    if (!l) continue;
                    uintptr_t base = (uintptr_t)lib_bias(l);
                    if (k < sizeof(key) - 40) k += (size_t)snprintf(key + k, sizeof(key) - k, "%s+%lx ", l, (unsigned long)(addrs[a] - base));
                    int f = -1;
                    for (int q = 0; q < ns; q++) if (sites[q].addr == addrs[a]) { f = q; break; }
                    if (f < 0 && ns < 8000) { sites[ns].addr = addrs[a]; sites[ns].n = 0; f = ns++; }
                    if (f >= 0) sites[f].n++;
                }
                int f = -1;
                for (int q = 0; q < nch; q++) if (!strcmp(chains[q].key, key)) { f = q; break; }
                if (f < 0 && nch < 4000) { snprintf(chains[nch].key, sizeof(chains[nch].key), "%s", key); chains[nch].n = 0; f = nch++; }
                if (f >= 0) chains[f].n++;
                total++;
            }
            thread_resume(th[i]);
        }
    }
    fprintf(stderr, "---- profile of '%s': %d samples, %d call sites, %d distinct stacks ----\n", spec, total, ns, nch);
    for (int r = 0; r < 40; r++) {
        int best = -1;
        for (int q = 0; q < ns; q++) if (sites[q].n > 0 && (best < 0 || sites[q].n > sites[best].n)) best = q;
        if (best < 0) break;
        const char *l = NULL; const void *sb = NULL; const char *sy = tl_ld_symbol_at((void *)sites[best].addr, &l, &sb);
        (void)sy;
        fprintf(stderr, "  %5d  %s+%#lx\n", sites[best].n, l ? l : "?", l ? (unsigned long)(sites[best].addr - (uintptr_t)lib_bias(l)) : 0ul);
        sites[best].n = -sites[best].n;
    }
    for (int r = 0; r < 8; r++) {
        int best = -1;
        for (int q = 0; q < nch; q++) if (chains[q].n > 0 && (best < 0 || chains[q].n > chains[best].n)) best = q;
        if (best < 0) break;
        fprintf(stderr, "  stack x%d: %s\n", chains[best].n, chains[best].key);
        chains[best].n = -chains[best].n;
    }
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
    char tmp[600] = "/Volumes/GTAV/husk2/tmp/husk-sdl-XXXXXX";
    if (getenv("TL_DATA")) snprintf(tmp, sizeof(tmp), "%s", getenv("TL_DATA")); else mkdtemp(tmp);   /* TL_DATA: keep the data dir between runs */
    const char *cef = "/Users/davi/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/Frameworks/Chromium Embedded Framework.framework/Versions/A/Libraries";
    char egl[600], gles[600]; snprintf(egl, sizeof(egl), "%s/libEGL.dylib", cef); snprintf(gles, sizeof(gles), "%s/libGLESv2.dylib", cef);
    char frames[] = "/Volumes/GTAV/husk2/tmp/husk-sdlframes-XXXXXX"; mkdtemp(frames);
    fprintf(stderr, "frames: %s\ndata: %s\n", frames, tmp);
    int w = argc > 4 ? atoi(argv[3]) : 1200, h = argc > 4 ? atoi(argv[4]) : 552;
    tl_ga_config cfg = { .apk_path = argv[1], .data_dir = tmp, .package_name = getenv("TL_PKG") ? getenv("TL_PKG") : "com.vectorunit.cobalt.googleplay", .width = w, .height = h,
                         .angle_egl = getenv("TL_ANGLE_EGL") ? getenv("TL_ANGLE_EGL") : egl, .angle_gles = getenv("TL_ANGLE_GLES") ? getenv("TL_ANGLE_GLES") : gles,
                         .frame_dir = frames, .frame_every = getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : -6 };
    g_frame_dir = frames;
    /* TL_VK: give the game Vulkan over this MoltenVK (1 = Homebrew's); presented frames go to the frames dir */
    if (getenv("TL_VK")) tl_vk_configure(strcmp(getenv("TL_VK"), "1") ? getenv("TL_VK") : "/opt/homebrew/lib/libMoltenVK.dylib", frames, getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : 6);
    /* TL_VK: give the game Vulkan over this MoltenVK (1 = Homebrew's); presented frames go to the frames dir */
    if (getenv("TL_VK")) tl_vk_configure(strcmp(getenv("TL_VK"), "1") ? getenv("TL_VK") : "/opt/homebrew/lib/libMoltenVK.dylib", frames, getenv("TL_FRAMES") ? atoi(getenv("TL_FRAMES")) : 6);
    if (getenv("TL_STRESS_ENV")) for (int i = 0; i < 400; i++) { char k[32], v[8]; snprintf(k, sizeof(k), "HUSK_STRESS_%d", i); snprintf(v, sizeof(v), "%d", i); setenv(k, v, 1); }   /* reallocates the process environment, as the app's own setenv calls do */
    if (getenv("TL_EXTRA_APKS")) { char ex[2000]; snprintf(ex, sizeof(ex), "%s", getenv("TL_EXTRA_APKS")); for (char *p = strtok(ex, ":"); p; p = strtok(NULL, ":")) if (!tl_sdl_add_package(p)) fprintf(stderr, "cannot add %s\n", p); }
    if (getenv("TL_ARGS")) tl_sdl_set_arguments(getenv("TL_ARGS"));
    if (getenv("TL_AUDIO")) tl_audio_install();
    if (getenv("TL_PAD")) tl_pad_connect(0, "Xbox Wireless Controller");
    if (!tl_sdl_start(&cfg, getenv("TL_ACTIVITY") ? getenv("TL_ACTIVITY") : getenv("TL_PKG") ? NULL : "com/vectorunit/cobalt/MainActivity")) { fprintf(stderr, "ue4: start failed\n"); return 1; }
    if (getenv("TL_PROBE")) install_probes(getenv("TL_PROBE"));
    if (getenv("TL_BLR_PROBE")) install_blr_probes(getenv("TL_BLR_PROBE"));
    if (getenv("TL_WINFLAGS_PROBE")) { tl_lib *L = tl_ld_find_lib("libmain.so"); if (L && !tl_ld_probe(L, strtoull(getenv("TL_WINFLAGS_PROBE"), NULL, 16), winflags_probe)) fprintf(stderr, "window flags probe failed\n"); }
    if (getenv("TL_EOS_PROBE")) { tl_lib *L = tl_ld_find_lib("libEOSSDK.so"); uintptr_t a = L ? (uintptr_t)tl_ld_sym(L, "EOS_Initialize") : 0; if (!a || !tl_ld_probe(L, a - lib_bias("libEOSSDK.so"), eos_init_probe)) fprintf(stderr, "EOS probe failed\n"); { uintptr_t pc = L ? (uintptr_t)tl_ld_sym(L, "EOS_Platform_Create") : 0; if (!pc || !tl_ld_probe(L, pc - lib_bias("libEOSSDK.so"), eos_platform_probe)) fprintf(stderr, "EOS platform probe failed\n"); } tl_lib *U = tl_ld_find_lib("libUE4.so"); if (U && !tl_ld_probe(U, 0x7347c2c, eos_result_probe)) fprintf(stderr, "EOS result probe failed\n"); }
    if (!tl_sdl_run()) { fprintf(stderr, "ue4: run failed\n"); return 1; }
    if (getenv("TL_CTL")) { static pthread_t ct; pthread_create(&ct, NULL, control_thread, getenv("TL_CTL")); }
    if (getenv("TL_PROF")) { pthread_t st; pthread_create(&st, NULL, profiler, getenv("TL_PROF")); }
    if (getenv("TL_SAMPLE")) { pthread_t st; pthread_create(&st, NULL, sampler, (void *)(intptr_t)atoi(getenv("TL_SAMPLE"))); }
    int secs = argc > 2 ? atoi(argv[2]) : 5;
    for (int i = 0; i < secs; i++) sleep(1);
    fprintf(stderr, "ue4: executable memory used %zu of %zu MiB\n", tl_xmem_used() >> 20, tl_xmem_size() >> 20);
    fprintf(stderr, "ue4: %lu frames in %d s\n", tl_sdl_frames(), secs);
    return 0;
}
