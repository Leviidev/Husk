/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * JNI natives for AOSP android.os.Parcel / Binder / BinderProxy, so framework.jar
 * talks the in-process binder without libandroid_runtime.
 */
#include "husk-tl-dvm-internal.h"
#include "husk-tl-binder.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
#define UNUSED (void)self; (void)a; (void)ret

#define MAG 0x48504C43u /* HPLC */

typedef struct {
    uint32_t mag;
    uint8_t *buf;
    uint32_t cap, pos, size;
    uint64_t offs[32];
    int noff;
} npcl;

static npcl *of(int64_t p)
{
    npcl *n = (npcl *)(uintptr_t)p;
    return n && n->mag == MAG ? n : NULL;
}

static void grow(npcl *n, uint32_t need)
{
    if (need <= n->cap) return;
    uint32_t c = n->cap ? n->cap * 2 : 64;
    while (c < need) c *= 2;
    uint8_t *q = realloc(n->buf, c);
    if (!q) return;
    memset(q + n->cap, 0, c - n->cap);
    n->buf = q;
    n->cap = c;
}

static void w_i32(npcl *n, int32_t v)
{
    grow(n, n->pos + 4);
    if (n->pos + 4 > n->cap) return;
    memcpy(n->buf + n->pos, &v, 4);
    n->pos += 4;
    if (n->pos > n->size) n->size = n->pos;
}

static int32_t r_i32(npcl *n)
{
    if (n->pos + 4 > n->size) return 0;
    int32_t v;
    memcpy(&v, n->buf + n->pos, 4);
    n->pos += 4;
    return v;
}

static jvalue J(int64_t v) { jvalue r; r.j = v; return r; }
static jvalue I(int32_t v) { jvalue r; r.j = 0; r.i = v; return r; }
static jvalue L(void *p) { jvalue r; r.j = 0; r.l = p; return r; }
static jvalue Z(bool v) { jvalue r; r.j = 0; r.z = v; return r; }

static int64_t field_j(jobj *o, const char *name)
{
    if (!o) return 0;
    return tl_jni_get_field(o, name, "J").j;
}

static void set_field_j(jobj *o, const char *name, int64_t v)
{
    if (!o) return;
    tl_jni_set_field(o, name, "J", J(v));
}

NAT(P_create)
{
    UNUSED;
    npcl *n = calloc(1, sizeof(*n));
    n->mag = MAG;
    *ret = J((int64_t)(uintptr_t)n);
    return true;
}

NAT(P_destroy)
{
    UNUSED;
    npcl *n = of(a[0].j);
    if (!n) return true;
    n->mag = 0;
    free(n->buf);
    free(n);
    return true;
}

NAT(P_writeInt) { UNUSED; npcl *n = of(a[0].j); if (n) w_i32(n, a[1].i); return true; }
NAT(P_readInt) { UNUSED; npcl *n = of(a[0].j); *ret = I(n ? r_i32(n) : 0); return true; }
NAT(P_writeLong)
{
    UNUSED;
    npcl *n = of(a[0].j);
    if (!n) return true;
    grow(n, n->pos + 8);
    memcpy(n->buf + n->pos, &a[1].j, 8);
    n->pos += 8;
    if (n->pos > n->size) n->size = n->pos;
    return true;
}
NAT(P_readLong)
{
    UNUSED;
    npcl *n = of(a[0].j);
    int64_t v = 0;
    if (n && n->pos + 8 <= n->size) { memcpy(&v, n->buf + n->pos, 8); n->pos += 8; }
    *ret = J(v);
    return true;
}
NAT(P_dataSize) { UNUSED; npcl *n = of(a[0].j); *ret = I(n ? (int32_t)n->size : 0); return true; }
NAT(P_dataAvail) { UNUSED; npcl *n = of(a[0].j); *ret = I(n && n->size > n->pos ? (int32_t)(n->size - n->pos) : 0); return true; }
NAT(P_dataPos) { UNUSED; npcl *n = of(a[0].j); *ret = I(n ? (int32_t)n->pos : 0); return true; }
NAT(P_dataCap) { UNUSED; npcl *n = of(a[0].j); *ret = I(n ? (int32_t)n->cap : 0); return true; }
NAT(P_setPos) { UNUSED; npcl *n = of(a[0].j); if (n) n->pos = (uint32_t)a[1].i; return true; }
NAT(P_setSize)
{
    UNUSED;
    npcl *n = of(a[0].j);
    if (!n) return true;
    n->size = (uint32_t)a[1].i;
    grow(n, n->size);
    if (n->pos > n->size) n->pos = n->size;
    return true;
}

static void w_str16(npcl *n, jobj *s)
{
    if (!s) { w_i32(n, -1); return; }
    int32_t nc = 0;
    const uint16_t *ch = tl_dvm_string_chars(s, &nc);
    w_i32(n, nc);
    uint32_t bytes = (uint32_t)(nc + 1) * 2;
    uint32_t pad = (bytes + 3) & ~3u;
    grow(n, n->pos + pad);
    if (ch && nc) memcpy(n->buf + n->pos, ch, (size_t)nc * 2);
    memset(n->buf + n->pos + (size_t)nc * 2, 0, pad - (size_t)nc * 2);
    n->pos += pad;
    if (n->pos > n->size) n->size = n->pos;
}

NAT(P_writeString) { UNUSED; npcl *n = of(a[0].j); if (n) w_str16(n, a[1].l); return true; }
NAT(P_readString)
{
    UNUSED;
    npcl *n = of(a[0].j);
    *ret = L(NULL);
    if (!n) return true;
    int32_t nc = r_i32(n);
    if (nc < 0) return true;
    uint32_t bytes = (uint32_t)(nc + 1) * 2;
    uint32_t pad = (bytes + 3) & ~3u;
    if (n->pos + (uint32_t)nc * 2 > n->size) return true;
    *ret = L(tl_dvm_string_u16((const uint16_t *)(n->buf + n->pos), nc));
    n->pos += pad;
    return true;
}

NAT(P_writeToken)
{
    UNUSED;
    npcl *n = of(a[0].j);
    if (!n) return true;
    w_i32(n, 0x01000000);
    w_i32(n, -1);
    w_str16(n, a[1].l);
    return true;
}

NAT(P_enforce)
{
    UNUSED;
    npcl *n = of(a[0].j);
    if (!n) return true;
    r_i32(n);
    int32_t x = 0;
    if (n->pos + 4 <= n->size) memcpy(&x, n->buf + n->pos, 4);
    if (x < 0 || x > 512) r_i32(n);
    /* consume the interface name */
    int32_t nc = r_i32(n);
    if (nc >= 0) {
        uint32_t pad = ((uint32_t)(nc + 1) * 2 + 3) & ~3u;
        n->pos += pad;
    }
    return true;
}

#define BINDER_TYPE_HANDLE 0x73682a85U
#define BINDER_TYPE_BINDER 0x73622a85U

static void w_binder(npcl *n, jobj *b)
{
    grow(n, n->pos + 24);
    uint32_t type = b ? BINDER_TYPE_BINDER : BINDER_TYPE_HANDLE;
    uint32_t flags = 0, handle = 0, pad = 0;
    uint64_t cookie = 0, binder = (uint64_t)(uintptr_t)b;
    size_t at = n->pos;
    memcpy(n->buf + at, &type, 4);
    memcpy(n->buf + at + 4, &flags, 4);
    if (b) memcpy(n->buf + at + 8, &binder, 8);
    else {
        memcpy(n->buf + at + 8, &handle, 4);
        memcpy(n->buf + at + 12, &pad, 4);
    }
    memcpy(n->buf + at + 16, &cookie, 8);
    if (n->noff < 32) n->offs[n->noff++] = at;
    n->pos += 24;
    if (n->pos > n->size) n->size = n->pos;
}

NAT(P_writeBinder) { UNUSED; npcl *n = of(a[0].j); if (n) w_binder(n, a[1].l); return true; }

static jobj *proxy_for(uint32_t handle)
{
    void *p = husk_binder_ptr(handle);
    if (p) return p;
    tl_jclass *jc = dvm_class_named("android/os/BinderProxy");
    if (!jc) jc = dvm_class_named("android/os/Binder");
    if (!jc) return NULL;
    jobj *o = dvm_new_object(jc);
    if (!o) return NULL;
    o->refs = 1u << 30;
    set_field_j(o, "mNativeData", (int64_t)handle);
    set_field_j(o, "mObject", (int64_t)handle);
    husk_binder_set_ptr(handle, o);
    return o;
}

NAT(P_readBinder)
{
    UNUSED;
    npcl *n = of(a[0].j);
    *ret = L(NULL);
    if (!n || !n->noff) return true;
    size_t at = (size_t)n->offs[0];
    memmove(n->offs, n->offs + 1, (size_t)(n->noff - 1) * 8);
    n->noff--;
    if (at + 24 > n->size) return true;
    uint32_t type = 0, handle = 0;
    memcpy(&type, n->buf + at, 4);
    memcpy(&handle, n->buf + at + 8, 4);
    if (type == BINDER_TYPE_BINDER) {
        uint64_t binder = 0;
        memcpy(&binder, n->buf + at + 8, 8);
        *ret = L((void *)(uintptr_t)binder);
        return true;
    }
    if (!handle) return true;
    *ret = L(proxy_for(handle));
    n->pos = (uint32_t)(at + 24);
    return true;
}

NAT(P_marshall)
{
    UNUSED;
    npcl *n = of(a[0].j);
    if (!n) { *ret = L(NULL); return true; }
    jobj *arr = tl_jni_new_prim_array('B', n->size);
    if (arr && n->buf && n->size) memcpy(arr->arr.data, n->buf, n->size);
    *ret = L(arr);
    return true;
}

NAT(P_zero) { UNUSED; if (ret) *ret = J(0); return true; }

static npcl *parcel_obj(jobj *o)
{
    if (!o) return NULL;
    int64_t p = field_j(o, "mNativePtr");
    return of(p);
}

NAT(B_transact)
{
    /* BinderProxy.transactNative(code, data, reply, flags) */
    uint32_t handle = (uint32_t)field_j(self, "mNativeData");
    if (!handle && self) handle = (uint32_t)field_j(self, "mObject");
    npcl *in = parcel_obj(a[1].l);
    npcl *out = parcel_obj(a[2].l);
    husk_binder_reply r = {0};
    int rc = husk_binder_transact(handle, (uint32_t)a[0].i, (uint32_t)a[3].i,
                                  in ? in->buf : NULL, in ? in->size : 0,
                                  in && in->noff ? in->offs : NULL, (size_t)(in ? in->noff : 0), &r);
    if (rc == 0 && out && r.data) {
        grow(out, (uint32_t)r.data_size);
        memcpy(out->buf, r.data, r.data_size);
        out->size = (uint32_t)r.data_size;
        out->pos = 0;
        out->noff = r.n_offsets < 32 ? (int)r.n_offsets : 32;
        if (r.offsets) memcpy(out->offs, r.offsets, (size_t)out->noff * 8);
    }
    husk_binder_reply_free(&r);
    *ret = Z(rc == 0);
    return true;
}

NAT(B_context)
{
    UNUSED;
    *ret = L(proxy_for(0));
    return true;
}

NAT(B_pid) { UNUSED; *ret = I((int32_t)getpid()); return true; }
NAT(B_uid) { UNUSED; *ret = I(1000); return true; }
NAT(B_true) { UNUSED; *ret = Z(true); return true; }

dvm_native_fn dvm_binder_native(const char *cls, const char *name, const char *sig);

dvm_native_fn dvm_binder_native(const char *cls, const char *name, const char *sig)
{
    (void)sig;
    int which = 0;
    if (!strcmp(cls, "android/os/Parcel")) which = 1;
    else if (!strcmp(cls, "android/os/BinderProxy")) which = 2;
    else if (!strcmp(cls, "android/os/Binder")) which = 3;
    else if (!strcmp(cls, "com/android/internal/os/BinderInternal") || !strcmp(cls, "android/os/BinderInternal")) which = 4;
    else return NULL;

    if (which == 1) {
        if (strstr(name, "Create") && strchr(sig, 'J')) return P_create;
        if (strstr(name, "Destroy")) return P_destroy;
        if (strstr(name, "WriteInt")) return P_writeInt;
        if (strstr(name, "ReadInt")) return P_readInt;
        if (strstr(name, "WriteLong")) return P_writeLong;
        if (strstr(name, "ReadLong")) return P_readLong;
        if (strstr(name, "DataSize") && !strstr(name, "Set")) return P_dataSize;
        if (strstr(name, "DataAvail")) return P_dataAvail;
        if (strstr(name, "DataPosition") && !strstr(name, "Set")) return P_dataPos;
        if (strstr(name, "DataCapacity")) return P_dataCap;
        if (strstr(name, "SetDataPosition")) return P_setPos;
        if (strstr(name, "SetDataSize")) return P_setSize;
        if (strstr(name, "WriteString")) return P_writeString;
        if (strstr(name, "ReadString")) return P_readString;
        if (strstr(name, "WriteInterfaceToken")) return P_writeToken;
        if (strstr(name, "EnforceInterface")) return P_enforce;
        if (strstr(name, "WriteStrongBinder")) return P_writeBinder;
        if (strstr(name, "ReadStrongBinder")) return P_readBinder;
        if (strstr(name, "Marshall") && !strstr(name, "Un")) return P_marshall;
        return P_zero;
    }
    if (which == 2) {
        if (strstr(name, "transact")) return B_transact;
        if (strstr(name, "ping") || strstr(name, "isBinderAlive")) return B_true;
        return P_zero;
    }
    if (which == 3) {
        if (!strcmp(name, "getCallingPid") || strstr(name, "getCallingPid")) return B_pid;
        if (strstr(name, "getCallingUid")) return B_uid;
        if (strstr(name, "transact")) return B_transact;
        return P_zero;
    }
    if (which == 4) {
        if (strstr(name, "getContextObject")) return B_context;
        return P_zero;
    }
    return NULL;
}
