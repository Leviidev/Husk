/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * pthreads for guest code.
 *
 * bionic's synchronisation objects are small and must work from zeroed memory:
 * guests embed them in their own structures at bionic's sizes (a mutex is 40 bytes,
 * a condition variable 48, a read-write lock 56, a semaphore 32) and rely on an
 * all-zero object being a valid normal mutex. Darwin's objects are bigger and have
 * their own layouts, so each guest object here holds a small header -- a flags word,
 * a magic, and a pointer to a real Darwin object made on first use -- and fits in the
 * guest's bytes with room to spare.
 *
 * A zeroed object has magic 0 and is initialised on first use, racing safely: one
 * thread wins the transition to "initialising" and the others spin until it is done.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-bionic.h"
#include "husk-tl-ld.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

/* General-dynamic thread-local access: the code passes {module, offset}. */
void *tl_ld_tls_get_addr(uint64_t module, uint64_t offset);
static void *b___tls_get_addr(const uint64_t *ti) { return tl_ld_tls_get_addr(ti[0], ti[1]); }


#define MAGIC_READY 0x7468726du   /* "thrm" */
#define MAGIC_INIT  1u

typedef struct { uint32_t value; _Atomic uint32_t magic; void *host; } guest_obj;

/* Get (making if needed) the Darwin object behind a guest object. `make` builds one
 * from the guest's flags word. */
static void *obj_host(void *g, void *(*make)(uint32_t value))
{
    guest_obj *o = g;
    uint32_t m = atomic_load_explicit(&o->magic, memory_order_acquire);
    for (;;) {
        if (m == MAGIC_READY) return o->host;
        if (m == MAGIC_INIT) { sched_yield(); m = atomic_load_explicit(&o->magic, memory_order_acquire); continue; }
        if (atomic_compare_exchange_weak_explicit(&o->magic, &m, MAGIC_INIT, memory_order_acq_rel, memory_order_acquire)) {
            o->host = make(o->value);
            atomic_store_explicit(&o->magic, MAGIC_READY, memory_order_release);
            return o->host;
        }
    }
}

static int rc(int darwin) { return darwin ? tl_errno_to_guest(darwin) : 0; }

/* ---------------------------------------------------------------- mutexes */

/* bionic types: NORMAL 0, RECURSIVE 1, ERRORCHECK 2. Darwin's: NORMAL 0, ERRORCHECK 1, RECURSIVE 2. */
static void *make_mutex(uint32_t value)
{
    unsigned type = (value >> 14) & 3;
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, type == 1 ? PTHREAD_MUTEX_RECURSIVE : type == 2 ? PTHREAD_MUTEX_ERRORCHECK : PTHREAD_MUTEX_NORMAL);
    pthread_mutex_t *m = malloc(sizeof(*m));
    pthread_mutex_init(m, &a);
    pthread_mutexattr_destroy(&a);
    return m;
}

static int b_mutex_init(void *g, const long *attr)
{
    guest_obj *o = g;
    memset(g, 0, 40);
    unsigned type = attr ? (unsigned)(*attr & 0xf) : 0;
    o->value = (type & 3) << 14;
    return 0;                       /* the Darwin object appears on first use */
}
static int b_mutex_destroy(void *g)
{
    guest_obj *o = g;
    if (atomic_load(&o->magic) == MAGIC_READY) { pthread_mutex_destroy(o->host); free(o->host); }
    uint32_t v = o->value;
    memset(g, 0, 40);
    o->value = v;
    return 0;
}
/*
 * TL_MUTEX_TRACE: remember who last locked each mutex (in the spare bytes of the guest's 40-byte object, which it never
 * touches) and say, when a lock has waited five seconds, who holds it and from where it was locked. For finding what a
 * stuck game is waiting for.
 */
static bool mutex_trace(void) { static int on = -1; if (on < 0) on = getenv("TL_MUTEX_TRACE") ? 1 : 0; return on; }
static void note_owner(void *g, void *lr) { uint64_t *w = g; w[2] = (uint64_t)(uintptr_t)pthread_self(); w[3] = (uint64_t)(uintptr_t)lr; }
static void describe_site(char *out, size_t n, uint64_t addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at((void *)(uintptr_t)addr, &lib, &sa);
    if (lib) snprintf(out, n, "%s %s+%#lx", lib, sym ? sym : "?", sa ? (unsigned long)(addr - (uintptr_t)sa) : 0ul);
    else snprintf(out, n, "%#llx", (unsigned long long)addr);
}
static int b_mutex_lock(void *g)
{
    pthread_mutex_t *m = obj_host(g, make_mutex);
    if (!mutex_trace()) return rc(pthread_mutex_lock(m));
    void *lr = __builtin_return_address(0);
    for (long i = 0;; i++) {
        int r = pthread_mutex_trylock(m);
        if (r == 0) { note_owner(g, lr); return 0; }
        if (r != EBUSY) return rc(r);
        usleep(200);
        if (i == 25000) {
            uint64_t *w = g; char owner[40] = "?", site[200], waiter[200];
            pthread_t t = (pthread_t)(uintptr_t)w[2];
            if (t) pthread_getname_np(t, owner, sizeof(owner));
            describe_site(site, sizeof(site), w[3]);
            describe_site(waiter, sizeof(waiter), (uint64_t)(uintptr_t)lr);
            char me[40] = ""; pthread_getname_np(pthread_self(), me, sizeof(me));
            tl_log_line("mutex: '%s' has waited 5 s on mutex %p, held by '%s' (locked from %s); waiting at %s", me, g, owner, site, waiter);
        }
    }
}
static int b_mutex_trylock(void *g) { int r = pthread_mutex_trylock(obj_host(g, make_mutex)); if (r == 0 && mutex_trace()) note_owner(g, __builtin_return_address(0)); return rc(r); }
static int b_mutex_unlock(void *g)  { if (mutex_trace()) ((uint64_t *)g)[2] = 0; return rc(pthread_mutex_unlock(obj_host(g, make_mutex))); }

static int b_mutexattr_init(long *a)    { *a = 0; return 0; }
static int b_mutexattr_destroy(long *a) { (void)a; return 0; }
static int b_mutexattr_settype(long *a, int type)
{
    if (type < 0 || type > 2) return 22;
    *a = (*a & ~0xfL) | type;
    return 0;
}
static int b_mutexattr_gettype(const long *a, int *type) { *type = (int)(*a & 0xf); return 0; }

/* ------------------------------------------------------ condition variables */

static void *make_cond(uint32_t value)
{
    (void)value;
    pthread_cond_t *c = malloc(sizeof(*c));
    pthread_cond_init(c, NULL);
    return c;
}

static int b_cond_init(void *g, const long *attr)
{
    guest_obj *o = g;
    memset(g, 0, 48);
    o->value = attr ? (uint32_t)(*attr & 3) : 0;
    return 0;
}
static int b_cond_destroy(void *g)
{
    guest_obj *o = g;
    if (atomic_load(&o->magic) == MAGIC_READY) { pthread_cond_destroy(o->host); free(o->host); }
    memset(g, 0, 48);
    return 0;
}
static int b_cond_signal(void *g)    { return rc(pthread_cond_signal(obj_host(g, make_cond))); }
static int b_cond_broadcast(void *g) { return rc(pthread_cond_broadcast(obj_host(g, make_cond))); }
static int b_cond_wait(void *g, void *m)
{
    return rc(pthread_cond_wait(obj_host(g, make_cond), obj_host(m, make_mutex)));
}

static int b_cond_timedwait(void *g, void *m, const struct timespec *abs)
{
    guest_obj *o = g;
    obj_host(g, make_cond);
    clockid_t clk = (o->value & 2) ? CLOCK_MONOTONIC : CLOCK_REALTIME;
    struct timespec now, rel;
    clock_gettime(clk, &now);
    rel.tv_sec = abs->tv_sec - now.tv_sec;
    rel.tv_nsec = abs->tv_nsec - now.tv_nsec;
    if (rel.tv_nsec < 0) { rel.tv_sec--; rel.tv_nsec += 1000000000L; }
    if (rel.tv_sec < 0) { rel.tv_sec = 0; rel.tv_nsec = 0; }
    return rc(pthread_cond_timedwait_relative_np(o->host, obj_host(m, make_mutex), &rel));
}

/* Bionic's clock ids: 0 is CLOCK_REALTIME, 1 CLOCK_MONOTONIC. */
static int cond_wait_until(void *g, void *m, clockid_t clk, const struct timespec *abs)
{
    guest_obj *o = g;
    obj_host(g, make_cond);
    struct timespec now, rel;
    clock_gettime(clk, &now);
    rel.tv_sec = abs->tv_sec - now.tv_sec;
    rel.tv_nsec = abs->tv_nsec - now.tv_nsec;
    if (rel.tv_nsec < 0) { rel.tv_sec--; rel.tv_nsec += 1000000000L; }
    if (rel.tv_sec < 0) { rel.tv_sec = 0; rel.tv_nsec = 0; }
    return rc(pthread_cond_timedwait_relative_np(o->host, obj_host(m, make_mutex), &rel));
}

static int b_cond_clockwait(void *g, void *m, int clock, const struct timespec *abs)
{
    if (clock != 0 && clock != 1) return 22;
    return cond_wait_until(g, m, clock == 1 ? CLOCK_MONOTONIC : CLOCK_REALTIME, abs);
}

static int b_cond_timedwait_monotonic(void *g, void *m, const struct timespec *abs) { return cond_wait_until(g, m, CLOCK_MONOTONIC, abs); }

static int b_condattr_init(long *a)    { *a = 0; return 0; }
static int b_condattr_destroy(long *a) { (void)a; return 0; }
static int b_condattr_setclock(long *a, int clock)
{
    if (clock == 0) *a &= ~2L;
    else if (clock == 1) *a |= 2L;
    else return 22;
    return 0;
}

/* -------------------------------------------------------- read-write locks */

static void *make_rwlock(uint32_t value)
{
    (void)value;
    pthread_rwlock_t *l = malloc(sizeof(*l));
    pthread_rwlock_init(l, NULL);
    return l;
}
static int b_rwlock_init(void *g, const void *attr) { (void)attr; memset(g, 0, 56); return 0; }
static int b_rwlock_destroy(void *g)
{
    guest_obj *o = g;
    if (atomic_load(&o->magic) == MAGIC_READY) { pthread_rwlock_destroy(o->host); free(o->host); }
    memset(g, 0, 56);
    return 0;
}
static int b_rwlock_rdlock(void *g)    { return rc(pthread_rwlock_rdlock(obj_host(g, make_rwlock))); }
static int b_rwlock_wrlock(void *g)    { return rc(pthread_rwlock_wrlock(obj_host(g, make_rwlock))); }
static int b_rwlock_tryrdlock(void *g) { return rc(pthread_rwlock_tryrdlock(obj_host(g, make_rwlock))); }
static int b_rwlock_trywrlock(void *g) { return rc(pthread_rwlock_trywrlock(obj_host(g, make_rwlock))); }
static int b_rwlock_unlock(void *g)    { return rc(pthread_rwlock_unlock(obj_host(g, make_rwlock))); }

/* ---------------------------------------------------------------- semaphores */

/* bionic's sem_t is 16 bytes on LP64 (a count and three reserved words): clearing more tramples the next field of whatever embeds it. */

typedef struct { pthread_mutex_t m; pthread_cond_t c; int count; } host_sem;

static void *make_sem(uint32_t value)
{
    host_sem *s = calloc(1, sizeof(*s));
    pthread_mutex_init(&s->m, NULL);
    pthread_cond_init(&s->c, NULL);
    s->count = (int)value;
    return s;
}

static int b_sem_init(void *g, int pshared, unsigned value)
{
    (void)pshared;
    guest_obj *o = g;
    memset(g, 0, 16);
    o->host = make_sem(value);
    atomic_store(&o->magic, MAGIC_READY);
    return 0;
}
static int b_sem_destroy(void *g)
{
    guest_obj *o = g;
    if (atomic_load(&o->magic) == MAGIC_READY) {
        host_sem *s = o->host;
        pthread_cond_destroy(&s->c); pthread_mutex_destroy(&s->m); free(s);
    }
    memset(g, 0, 16);
    return 0;
}
static host_sem *hs(void *g) { return obj_host(g, make_sem); }
static int b_sem_post(void *g)
{
    host_sem *s = hs(g);
    pthread_mutex_lock(&s->m);
    s->count++;
    pthread_cond_signal(&s->c);
    pthread_mutex_unlock(&s->m);
    return 0;
}
static int b_sem_wait(void *g)
{
    host_sem *s = hs(g);
    pthread_mutex_lock(&s->m);
    while (s->count <= 0) pthread_cond_wait(&s->c, &s->m);
    s->count--;
    pthread_mutex_unlock(&s->m);
    return 0;
}
static int b_sem_trywait(void *g)
{
    host_sem *s = hs(g);
    int ok = 0;
    pthread_mutex_lock(&s->m);
    if (s->count > 0) { s->count--; ok = 1; }
    pthread_mutex_unlock(&s->m);
    if (!ok) { tl_set_guest_errno(11); return -1; }
    return 0;
}
static int b_sem_timedwait(void *g, const struct timespec *abs)
{
    host_sem *s = hs(g);
    struct timespec now, rel;
    clock_gettime(CLOCK_REALTIME, &now);
    rel.tv_sec = abs->tv_sec - now.tv_sec;
    rel.tv_nsec = abs->tv_nsec - now.tv_nsec;
    if (rel.tv_nsec < 0) { rel.tv_sec--; rel.tv_nsec += 1000000000L; }
    if (rel.tv_sec < 0) { rel.tv_sec = 0; rel.tv_nsec = 0; }
    pthread_mutex_lock(&s->m);
    int r = 0;
    while (s->count <= 0 && r == 0) r = pthread_cond_timedwait_relative_np(&s->c, &s->m, &rel);
    if (r == 0) s->count--;
    pthread_mutex_unlock(&s->m);
    if (r) { tl_set_guest_errno(tl_errno_to_guest(r)); return -1; }
    return 0;
}
static int b_sem_getvalue(void *g, int *v)
{
    host_sem *s = hs(g);
    pthread_mutex_lock(&s->m);
    *v = s->count;
    pthread_mutex_unlock(&s->m);
    return 0;
}

/* --------------------------------------------------------------- once, keys */

static int b_once(_Atomic int *once, void (*fn)(void))
{
    int expect = 0;
    if (atomic_compare_exchange_strong(once, &expect, 2)) {
        fn();
        atomic_store(once, 1);
    } else {
        while (atomic_load(once) == 2) sched_yield();
    }
    return 0;
}

/* bionic's pthread_key_t is an int; Darwin's is an unsigned long. */
static int b_key_create(int *key, void (*dtor)(void *))
{
    pthread_key_t k;
    int r = pthread_key_create(&k, dtor);
    if (r) return rc(r);
    *key = (int)k;
    return 0;
}
static int b_key_delete(int key) { return rc(pthread_key_delete((pthread_key_t)key)); }
static void *b_getspecific(int key) { return pthread_getspecific((pthread_key_t)key); }
static int b_setspecific(int key, const void *v) { return rc(pthread_setspecific((pthread_key_t)key, v)); }

/* ---------------------------------------------------------------- threads */

/* bionic's pthread_attr_t is 56 bytes on LP64: flags, stack_base, stack_size, guard_size, sched... */
typedef struct { uint32_t flags; uint32_t pad; void *stack_base; size_t stack_size; size_t guard_size; int32_t policy, priority; char reserved[16]; } guest_attr;
_Static_assert(sizeof(guest_attr) == 56, "bionic pthread_attr_t is 56 bytes on LP64");
_Static_assert(sizeof(guest_obj) <= 16, "guest_obj must fit bionic's smallest object, sem_t");
#define GATTR_DETACHED 1u
#define DEFAULT_STACK (1024u * 1024u)

static int b_attr_init(guest_attr *a)
{
    memset(a, 0, sizeof(*a));
    a->stack_size = DEFAULT_STACK;
    a->guard_size = 16384;
    return 0;
}
static int b_attr_destroy(guest_attr *a) { (void)a; return 0; }
static int b_attr_setdetachstate(guest_attr *a, int s)
{
    if (s != 0 && s != 1) return 22;
    a->flags = (a->flags & ~GATTR_DETACHED) | (s ? GATTR_DETACHED : 0);
    return 0;
}
static int b_attr_setstacksize(guest_attr *a, size_t n)
{
    if (n < 16384) return 22;
    a->stack_size = n;
    return 0;
}
static int b_attr_getstacksize(const guest_attr *a, size_t *n) { *n = a->stack_size; return 0; }
/* Scheduling attributes: a thread on this host runs at the system's own priority; the guest is told the default (SCHED_OTHER, priority 0) and its requests are accepted. */
static int b_attr_getschedparam(const guest_attr *a, int *prio) { (void)a; *prio = 0; return 0; }
static int b_attr_getschedpolicy(const guest_attr *a, int *policy) { (void)a; *policy = 0; return 0; }
static int b_attr_setschedpolicy(guest_attr *a, int policy) { (void)a; (void)policy; return 0; }
static int b_attr_getguardsize(const guest_attr *a, size_t *n) { *n = a->guard_size; return 0; }
static int b_attr_getstack(const guest_attr *a, void **base, size_t *size) { *base = a->stack_base; *size = a->stack_size; return 0; }

static int b_getattr_np(pthread_t t, guest_attr *a)
{
    b_attr_init(a);
    size_t size = pthread_get_stacksize_np(t);
    uint8_t *top = pthread_get_stackaddr_np(t);
    a->stack_size = size;
    a->stack_base = top - size;
    a->flags = 0;
    return 0;
}

typedef struct { void *(*fn)(void *); void *arg; } start_ctx;

const char *tl_ld_symbol_at(const void *addr, const char **lib_name, const void **sym_addr);

static int g_thread_trace = -1;
static void *start_thunk(void *p)
{
    start_ctx c = *(start_ctx *)p;
    free(p);
    if (g_thread_trace < 0) g_thread_trace = getenv("TL_THREAD_TRACE") ? 1 : 0;
    if (g_thread_trace) {
        const char *ln = NULL; const void *sa = NULL;
        const char *sym = tl_ld_symbol_at((const void *)c.fn, &ln, &sa);
        tl_log_line("thread: started, entry %s %s+%#lx", ln ? ln : "?", sym ? sym : "?", sa ? (unsigned long)((const char *)c.fn - (const char *)sa) : 0ul);
    }
    void *r = c.fn(c.arg);
    if (g_thread_trace) { char nm[32] = ""; pthread_getname_np(pthread_self(), nm, sizeof(nm)); tl_log_line("thread: '%s' EXITED", nm); }
    return r;
}

static int b_create(pthread_t *out, const guest_attr *attr, void *(*fn)(void *), void *arg)
{
    pthread_attr_t a;
    pthread_attr_init(&a);
    size_t stack = attr && attr->stack_size ? attr->stack_size : DEFAULT_STACK;
    if (stack < 256 * 1024) stack = 256 * 1024;
    stack = (stack + 16383) & ~(size_t)16383;
    pthread_attr_setstacksize(&a, stack);
    if (attr && (attr->flags & GATTR_DETACHED)) pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    start_ctx *c = malloc(sizeof(*c));
    c->fn = fn; c->arg = arg;
    pthread_t t;
    int r = pthread_create(&t, &a, start_thunk, c);
    pthread_attr_destroy(&a);
    if (r) { free(c); return rc(r); }
    *out = t;
    return 0;
}

static int b_kill(pthread_t t, int sig)
{
    if (getenv("TL_SIGNAL_TRACE")) { char nm[32] = "", tn[32] = ""; pthread_getname_np(pthread_self(), nm, sizeof(nm)); pthread_getname_np(t, tn, sizeof(tn)); tl_log_line("signal[%s]: pthread_kill(%s, %d)", nm, tn, sig); }
    int d = tl_signal_to_darwin(sig);
    if (d < 0) return 22;
    return rc(pthread_kill(t, d));
}

static int b_setname_np(pthread_t t, const char *name)
{
    if (pthread_equal(t, pthread_self())) pthread_setname_np(name);
    return 0;
}

static int b_join(pthread_t t, void **ret) { return rc(pthread_join(t, ret)); }
static int b_detach(pthread_t t) { return rc(pthread_detach(t)); }

/* sigset_t is one 64-bit word on bionic, bit (sig-1); Darwin's is a 32-bit mask. */
static uint32_t sigset_to_darwin(uint64_t g)
{
    uint32_t d = 0;
    for (int s = 1; s < 32; s++) if (g & (1ull << (s - 1))) { int ds = tl_signal_to_darwin(s); if (ds > 0) d |= 1u << (ds - 1); }
    return d;
}
static uint64_t sigset_from_darwin(uint32_t d)
{
    uint64_t g = 0;
    for (int s = 1; s < 32; s++) if (d & (1u << (s - 1))) { int gs = tl_signal_from_darwin(s); if (gs > 0 && gs < 64) g |= 1ull << (gs - 1); }
    return g;
}
static int b_sigmask(int how, const uint64_t *set, uint64_t *old)
{
    sigset_t ds, od;
    int dh = how == 0 ? SIG_BLOCK : how == 1 ? SIG_UNBLOCK : SIG_SETMASK;
    if (set) { uint32_t m = sigset_to_darwin(*set); memcpy(&ds, &m, sizeof(m)); }
    int r = pthread_sigmask(dh, set ? &ds : NULL, old ? &od : NULL);
    if (old) { uint32_t m; memcpy(&m, &od, sizeof(m)); *old = sigset_from_darwin(m); }
    return rc(r);
}

const tl_bionic_entry tl_tab_pthread[] = {
    TL_WRAP("pthread_mutex_init", b_mutex_init), TL_WRAP("pthread_mutex_destroy", b_mutex_destroy),
    TL_WRAP("pthread_mutex_lock", b_mutex_lock), TL_WRAP("pthread_mutex_trylock", b_mutex_trylock),
    TL_WRAP("pthread_mutex_unlock", b_mutex_unlock),
    TL_WRAP("pthread_mutexattr_init", b_mutexattr_init), TL_WRAP("pthread_mutexattr_destroy", b_mutexattr_destroy),
    TL_WRAP("pthread_mutexattr_settype", b_mutexattr_settype), TL_WRAP("pthread_mutexattr_gettype", b_mutexattr_gettype),
    TL_WRAP("pthread_cond_init", b_cond_init), TL_WRAP("pthread_cond_destroy", b_cond_destroy),
    TL_WRAP("pthread_cond_signal", b_cond_signal), TL_WRAP("pthread_cond_broadcast", b_cond_broadcast),
    TL_WRAP("pthread_cond_wait", b_cond_wait), TL_WRAP("pthread_cond_timedwait", b_cond_timedwait), TL_WRAP("pthread_cond_clockwait", b_cond_clockwait),
    TL_WRAP("pthread_cond_timedwait_monotonic_np", b_cond_timedwait_monotonic), TL_WRAP("pthread_cond_timedwait_monotonic", b_cond_timedwait_monotonic),
    TL_WRAP("pthread_condattr_init", b_condattr_init), TL_WRAP("pthread_condattr_destroy", b_condattr_destroy),
    TL_WRAP("pthread_condattr_setclock", b_condattr_setclock),
    TL_WRAP("pthread_rwlock_init", b_rwlock_init), TL_WRAP("pthread_rwlock_destroy", b_rwlock_destroy),
    TL_WRAP("pthread_rwlock_rdlock", b_rwlock_rdlock), TL_WRAP("pthread_rwlock_wrlock", b_rwlock_wrlock),
    TL_WRAP("pthread_rwlock_tryrdlock", b_rwlock_tryrdlock), TL_WRAP("pthread_rwlock_trywrlock", b_rwlock_trywrlock),
    TL_WRAP("pthread_rwlock_unlock", b_rwlock_unlock),
    TL_WRAP("sem_init", b_sem_init), TL_WRAP("sem_destroy", b_sem_destroy), TL_WRAP("sem_post", b_sem_post),
    TL_WRAP("sem_wait", b_sem_wait), TL_WRAP("sem_trywait", b_sem_trywait), TL_WRAP("sem_timedwait", b_sem_timedwait),
    TL_WRAP("sem_getvalue", b_sem_getvalue),
    TL_WRAP("pthread_once", b_once),
    TL_WRAP("__tls_get_addr", b___tls_get_addr),
    TL_WRAP("pthread_key_create", b_key_create), TL_WRAP("pthread_key_delete", b_key_delete),
    TL_WRAP("pthread_getspecific", b_getspecific), TL_WRAP("pthread_setspecific", b_setspecific),
    TL_WRAP("pthread_attr_init", b_attr_init), TL_WRAP("pthread_attr_destroy", b_attr_destroy),
    TL_WRAP("pthread_attr_setdetachstate", b_attr_setdetachstate), TL_WRAP("pthread_attr_setstacksize", b_attr_setstacksize),
    TL_WRAP("pthread_attr_getstacksize", b_attr_getstacksize), TL_WRAP("pthread_attr_getguardsize", b_attr_getguardsize), TL_WRAP("pthread_attr_getstack", b_attr_getstack),
    TL_WRAP("pthread_attr_getschedparam", b_attr_getschedparam),
    TL_WRAP("pthread_attr_getschedpolicy", b_attr_getschedpolicy), TL_WRAP("pthread_attr_setschedpolicy", b_attr_setschedpolicy),
    TL_WRAP("pthread_getattr_np", b_getattr_np),
    TL_WRAP("pthread_create", b_create), TL_WRAP("pthread_join", b_join), TL_WRAP("pthread_detach", b_detach),
    TL_DIRECT(pthread_exit), TL_DIRECT(pthread_self), TL_DIRECT(pthread_equal),
    TL_WRAP("pthread_kill", b_kill), TL_WRAP("pthread_setname_np", b_setname_np),
    TL_WRAP("pthread_sigmask", b_sigmask),
    TL_END
};
