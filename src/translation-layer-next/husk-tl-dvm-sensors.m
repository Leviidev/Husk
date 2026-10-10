/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Sensors for Java apps (husk.Sensors' natives): the iPhone's motion sensors through CoreMotion, in Android's units and signs.
 *
 * Android's accelerometer reports the force holding the phone up (+9.81 m/s^2 on z lying face up); CoreMotion reports acceleration
 * in g with gravity pointing down (-1 on z face up): Android = -9.81 x CoreMotion, on the same device axes (x right, y up, z out of
 * the screen, relative to the natural portrait orientation). Gyroscope (rad/s) and magnetometer (uT) have the same axes and signs.
 * Gravity and linear acceleration come from device motion's split of the same signal; the rotation vectors are its attitude
 * quaternion (x, y, z, w, then the heading accuracy), as Android's TYPE_ROTATION_VECTOR / TYPE_GAME_ROTATION_VECTOR carry it.
 */
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if TARGET_OS_IOS
#import <CoreMotion/CoreMotion.h>
#endif

#include "husk-tl-dvm-internal.h"

enum { T_ACCEL = 1, T_MAGNETIC = 2, T_GYRO = 4, T_GRAVITY = 9, T_LINEAR = 10, T_ROTATION = 11, T_GAME_ROTATION = 15, T_MAX = 16 };

static struct {
    _Atomic int64_t ts[T_MAX];
    float v[T_MAX][5];
    int nv[T_MAX];
    int users[T_MAX];
} S;
static NSLock *g_lock;

static int64_t now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int64_t)t.tv_sec * 1000000000ll + t.tv_nsec; }

static void put(int type, const float *v, int n)
{
    [g_lock lock];
    memcpy(S.v[type], v, (size_t)n * sizeof(float));
    S.nv[type] = n;
    atomic_store(&S.ts[type], now_ns());
    [g_lock unlock];
}

#if TARGET_OS_IOS
static CMMotionManager *g_mm;
static NSOperationQueue *g_q;
static void ensure(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{ g_mm = [CMMotionManager new]; g_q = [NSOperationQueue new]; g_q.maxConcurrentOperationCount = 1; g_lock = [NSLock new]; });
}
static bool motion_users(void) { return S.users[T_GRAVITY] || S.users[T_LINEAR] || S.users[T_ROTATION] || S.users[T_GAME_ROTATION]; }
static void restart(int type, double period)
{
    if (type == T_ACCEL) {
        if (S.users[T_ACCEL] && g_mm.accelerometerAvailable) {
            g_mm.accelerometerUpdateInterval = period;
            if (!g_mm.accelerometerActive) [g_mm startAccelerometerUpdatesToQueue:g_q withHandler:^(CMAccelerometerData *d, NSError *e) {
                if (!d) return;
                float v[3] = { (float)(-d.acceleration.x * 9.80665), (float)(-d.acceleration.y * 9.80665), (float)(-d.acceleration.z * 9.80665) };
                put(T_ACCEL, v, 3);
            }];
        } else if (!S.users[T_ACCEL] && g_mm.accelerometerActive) [g_mm stopAccelerometerUpdates];
    } else if (type == T_GYRO) {
        if (S.users[T_GYRO] && g_mm.gyroAvailable) {
            g_mm.gyroUpdateInterval = period;
            if (!g_mm.gyroActive) [g_mm startGyroUpdatesToQueue:g_q withHandler:^(CMGyroData *d, NSError *e) {
                if (!d) return;
                float v[3] = { (float)d.rotationRate.x, (float)d.rotationRate.y, (float)d.rotationRate.z };
                put(T_GYRO, v, 3);
            }];
        } else if (!S.users[T_GYRO] && g_mm.gyroActive) [g_mm stopGyroUpdates];
    } else if (type == T_MAGNETIC) {
        if (S.users[T_MAGNETIC] && g_mm.magnetometerAvailable) {
            g_mm.magnetometerUpdateInterval = period;
            if (!g_mm.magnetometerActive) [g_mm startMagnetometerUpdatesToQueue:g_q withHandler:^(CMMagnetometerData *d, NSError *e) {
                if (!d) return;
                float v[3] = { (float)d.magneticField.x, (float)d.magneticField.y, (float)d.magneticField.z };
                put(T_MAGNETIC, v, 3);
            }];
        } else if (!S.users[T_MAGNETIC] && g_mm.magnetometerActive) [g_mm stopMagnetometerUpdates];
    } else {
        if (motion_users() && g_mm.deviceMotionAvailable) {
            g_mm.deviceMotionUpdateInterval = period;
            if (!g_mm.deviceMotionActive) [g_mm startDeviceMotionUpdatesUsingReferenceFrame:CMAttitudeReferenceFrameXArbitraryZVertical toQueue:g_q withHandler:^(CMDeviceMotion *m, NSError *e) {
                if (!m) return;
                float g[3] = { (float)(-m.gravity.x * 9.80665), (float)(-m.gravity.y * 9.80665), (float)(-m.gravity.z * 9.80665) };
                float l[3] = { (float)(-m.userAcceleration.x * 9.80665), (float)(-m.userAcceleration.y * 9.80665), (float)(-m.userAcceleration.z * 9.80665) };
                CMQuaternion q = m.attitude.quaternion;
                float r[5] = { (float)q.x, (float)q.y, (float)q.z, (float)q.w, 0 };
                put(T_GRAVITY, g, 3); put(T_LINEAR, l, 3); put(T_ROTATION, r, 5); put(T_GAME_ROTATION, r, 4);
            }];
        } else if (!motion_users() && g_mm.deviceMotionActive) [g_mm stopDeviceMotionUpdates];
    }
}
static bool available(int type)
{
    ensure();
    switch (type) {
    case T_ACCEL: return g_mm.accelerometerAvailable;
    case T_GYRO: return g_mm.gyroAvailable;
    case T_MAGNETIC: return g_mm.magnetometerAvailable;
    case T_GRAVITY: case T_LINEAR: case T_ROTATION: case T_GAME_ROTATION: return g_mm.deviceMotionAvailable;
    }
    return false;
}
#else
/* macOS (the test harness): no motion hardware; TL_SENSORS=1 gives a phone lying flat face up, so apps that need the sensors run */
static void ensure(void) { static dispatch_once_t once; dispatch_once(&once, ^{ g_lock = [NSLock new]; }); }
static bool available(int type) { ensure(); return getenv("TL_SENSORS") && (type == T_ACCEL || type == T_GYRO || type == T_GRAVITY || type == T_LINEAR || type == T_GAME_ROTATION || type == T_ROTATION); }
static void restart(int type, double period)
{
    (void)period;
    if (!S.users[type]) return;
    float z[5] = { 0, 0, 0, 1, 0 }, flat[3] = { 0, 0, 9.80665f }, zero[3] = { 0, 0, 0 };
    if (type == T_ACCEL || type == T_GRAVITY) put(type, flat, 3);
    else if (type == T_GYRO || type == T_LINEAR) put(type, zero, 3);
    else put(type, z, type == T_ROTATION ? 5 : 4);
}
#endif

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue Z(bool b) { jvalue v; v.j = 0; v.z = b; return v; }
static jvalue J(int64_t j) { jvalue v; v.j = j; return v; }

NAT(N_available) { (void)self; int t = a[0].i; *ret = Z(t > 0 && t < T_MAX && available(t)); return true; }
NAT(N_start)
{
    (void)self; (void)ret;
    int t = a[0].i; double period = a[1].i > 0 ? a[1].i / 1e6 : 0.005;
    if (t <= 0 || t >= T_MAX || !available(t)) return true;
    S.users[t]++;
    dispatch_async(dispatch_get_main_queue(), ^{ restart(t, period); });
    return true;
}
NAT(N_stop)
{
    (void)self; (void)ret;
    int t = a[0].i;
    if (t <= 0 || t >= T_MAX || S.users[t] <= 0) return true;
    S.users[t]--;
    dispatch_async(dispatch_get_main_queue(), ^{ restart(t, 0.02); });
    return true;
}
/* The latest sample: its values into out, its time (ns, CLOCK_MONOTONIC) returned; 0 when there is none yet. */
NAT(N_read)
{
    (void)self;
    int t = a[0].i; jobj *o = a[1].l;
    if (t <= 0 || t >= T_MAX || !o) { *ret = J(0); return true; }
    ensure();
    [g_lock lock];
    int64_t ts = atomic_load(&S.ts[t]);
    int n = S.nv[t] < (int)o->arr.len ? S.nv[t] : (int)o->arr.len;
    memcpy(o->arr.data, S.v[t], (size_t)n * sizeof(float));
    [g_lock unlock];
    *ret = J(ts);
    return true;
}

static const struct { const char *name, *sig; dvm_native_fn fn; } k_sensors[] = {
    { "available", "(I)Z", N_available },
    { "start", "(II)V", N_start },
    { "stop", "(I)V", N_stop },
    { "read", "(I[F)J", N_read },
    { NULL, NULL, NULL },
};
dvm_native_fn tl_sensors_native(const char *name, const char *sig);
dvm_native_fn tl_sensors_native(const char *name, const char *sig)
{
    for (int i = 0; k_sensors[i].name; i++) if (!strcmp(k_sensors[i].name, name) && !strcmp(k_sensors[i].sig, sig)) return k_sensors[i].fn;
    return NULL;
}
