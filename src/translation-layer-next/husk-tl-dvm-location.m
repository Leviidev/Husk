// SPDX-License-Identifier: GPL-2.0-or-later
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "husk-tl-location.h"
#include "husk-tl-dvm-internal.h"
#include "husk-tl-bionic.h"

#if TARGET_OS_IOS
#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>
static pthread_mutex_t g_geo_lock = PTHREAD_MUTEX_INITIALIZER;
static double g_geo_sample[HUSK_LOCATION_SAMPLE_SIZE];
static uint64_t g_geo_sequence;
static _Atomic int g_geo_status = HUSK_LOCATION_PENDING;
static _Atomic bool g_geo_wanted;

@interface HuskLocationSource : NSObject <CLLocationManagerDelegate>
@property(nonatomic, strong) CLLocationManager *manager;
@property(nonatomic) BOOL updating;
@property(nonatomic) BOOL authorizationRequested;
- (void)update;
@end
static HuskLocationSource *g_geo_source;
@implementation HuskLocationSource
- (instancetype)init {
    if ((self = [super init])) {
        _manager = [CLLocationManager new]; _manager.delegate = self;
        _manager.desiredAccuracy = kCLLocationAccuracyBest;
        _manager.distanceFilter = kCLDistanceFilterNone;
        _manager.pausesLocationUpdatesAutomatically = YES;
        _manager.allowsBackgroundLocationUpdates = NO;
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(foreground:) name:UIApplicationDidBecomeActiveNotification object:nil];
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(background:) name:UIApplicationDidEnterBackgroundNotification object:nil];
    }
    return self;
}
- (void)foreground:(NSNotification *)note { (void)note; [self update]; }
- (void)background:(NSNotification *)note { (void)note; [self.manager stopUpdatingLocation]; self.updating = NO; }
- (void)update {
    CLAuthorizationStatus authorization = self.manager.authorizationStatus;
    int status = authorization == kCLAuthorizationStatusDenied || authorization == kCLAuthorizationStatusRestricted ? HUSK_LOCATION_DENIED
        : !CLLocationManager.locationServicesEnabled ? HUSK_LOCATION_DISABLED
        : authorization == kCLAuthorizationStatusNotDetermined ? HUSK_LOCATION_PENDING : HUSK_LOCATION_AUTHORIZED;
    int previous = atomic_exchange(&g_geo_status, status);
    if (previous != status) tl_log_line("location: authorization state %d", status);
    BOOL active = UIApplication.sharedApplication.applicationState == UIApplicationStateActive;
    if (!atomic_load(&g_geo_wanted) || !active || status == HUSK_LOCATION_DENIED || status == HUSK_LOCATION_DISABLED) {
        [self.manager stopUpdatingLocation]; self.updating = NO; return;
    }
    if (status == HUSK_LOCATION_PENDING) {
        if ([NSBundle.mainBundle objectForInfoDictionaryKey:@"NSLocationWhenInUseUsageDescription"]) {
            if (!self.authorizationRequested) { self.authorizationRequested = YES; [self.manager requestWhenInUseAuthorization]; }
        }
        else { atomic_store(&g_geo_status, HUSK_LOCATION_UNAVAILABLE); tl_log_line("location: missing usage description; access was not requested"); }
        return;
    }
    if (!self.updating) { self.updating = YES; [self.manager startUpdatingLocation]; }
}
- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager { (void)manager; self.authorizationRequested = NO; [self update]; }
- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray<CLLocation *> *)locations {
    (void)manager;
    CLLocation *location = locations.lastObject;
    if (!atomic_load(&g_geo_wanted) || atomic_load(&g_geo_status) != HUSK_LOCATION_AUTHORIZED || !location
        || location.horizontalAccuracy < 0 || !CLLocationCoordinate2DIsValid(location.coordinate)) return;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    double monotonic = now.tv_sec * 1e9 + now.tv_nsec;
    double age = fmax(0, -location.timestamp.timeIntervalSinceNow);
    double sample[HUSK_LOCATION_SAMPLE_SIZE] = { location.coordinate.latitude, location.coordinate.longitude, location.altitude,
        location.horizontalAccuracy, location.verticalAccuracy, location.speed, location.course,
        location.timestamp.timeIntervalSince1970 * 1000, fmax(1, monotonic - age * 1e9), location.speedAccuracy, location.courseAccuracy,
        location.sourceInformation.isSimulatedBySoftware ? 1 : 0 };
    pthread_mutex_lock(&g_geo_lock); memcpy(g_geo_sample, sample, sizeof(sample)); g_geo_sequence++; pthread_mutex_unlock(&g_geo_lock);
}
- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error {
    (void)manager;
    if (error.code == kCLErrorDenied) { atomic_store(&g_geo_status, HUSK_LOCATION_DENIED); [self.manager stopUpdatingLocation]; self.updating = NO; }
    if (error.code != kCLErrorLocationUnknown) tl_log_line("location: Core Location error %ld", (long)error.code);
}
@end
static void geo_update(void) {
    dispatch_async(dispatch_get_main_queue(), ^{ if (!g_geo_source) g_geo_source = [HuskLocationSource new]; [g_geo_source update]; });
}
int husk_location_status(void) { geo_update(); return atomic_load(&g_geo_status); }
void husk_location_enable(bool enabled) { atomic_store(&g_geo_wanted, enabled); geo_update(); }
uint64_t husk_location_read(double *sample, size_t count) {
    if (!sample || count < HUSK_LOCATION_SAMPLE_SIZE || atomic_load(&g_geo_status) != HUSK_LOCATION_AUTHORIZED) return 0;
    pthread_mutex_lock(&g_geo_lock); uint64_t sequence = g_geo_sequence;
    if (sequence) memcpy(sample, g_geo_sample, sizeof(g_geo_sample)); pthread_mutex_unlock(&g_geo_lock);
    return sequence;
}
void husk_location_reset(void) {
    husk_location_enable(false);
    pthread_mutex_lock(&g_geo_lock); g_geo_sequence = 0; memset(g_geo_sample, 0, sizeof(g_geo_sample)); pthread_mutex_unlock(&g_geo_lock);
}
#else
/* Desktop harnesses never query the Mac user's physical location. Tests provide their own Java bridge samples. */
int husk_location_status(void) { return HUSK_LOCATION_UNAVAILABLE; }
void husk_location_enable(bool enabled) { (void)enabled; }
uint64_t husk_location_read(double *sample, size_t count) { (void)sample; (void)count; return 0; }
void husk_location_reset(void) {}
#endif

static bool geo_status(jobj *self, const jvalue *a, jvalue *ret) { (void)self; (void)a; ret->j = 0; ret->i = husk_location_status(); return true; }
static bool geo_enable(jobj *self, const jvalue *a, jvalue *ret) { (void)self; (void)ret; husk_location_enable(a[0].z); return true; }
static bool geo_read(jobj *self, const jvalue *a, jvalue *ret) {
    (void)self; jobj *array = a[0].l;
    ret->j = array && array->kind == TL_K_PRIM_ARRAY && array->arr.etype == 'D' ? (int64_t)husk_location_read(array->arr.data, array->arr.len) : 0;
    return true;
}
static bool geo_reset(jobj *self, const jvalue *a, jvalue *ret) { (void)self; (void)a; (void)ret; husk_location_reset(); return true; }
dvm_native_fn tl_location_native(const char *name, const char *sig) {
    if (!strcmp(name, "status") && !strcmp(sig, "()I")) return geo_status;
    if (!strcmp(name, "enable") && !strcmp(sig, "(Z)V")) return geo_enable;
    if (!strcmp(name, "read") && !strcmp(sig, "([D)J")) return geo_read;
    if (!strcmp(name, "reset") && !strcmp(sig, "()V")) return geo_reset;
    return NULL;
}
