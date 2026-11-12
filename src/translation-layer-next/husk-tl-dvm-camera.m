// SPDX-License-Identifier: GPL-2.0-or-later
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <TargetConditionals.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "husk-tl-dvm-internal.h"
#include "husk-tl-bionic.h"
static jvalue camera_int(int32_t value) { jvalue result={0};result.i=value;return result; }
#define I(value) camera_int(value)
#define J(value) ((jvalue){.j=(value)})
#define L(value) ((jvalue){.l=(value)})

static int64_t camera_now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (int64_t)t.tv_sec*1000000000ll+t.tv_nsec; }
static NSData *camera_jpeg(CGImageRef image, int rotation) {
    if (!image) return nil;
    NSMutableData *bytes=[NSMutableData data];
    CGImageDestinationRef destination=CGImageDestinationCreateWithData((__bridge CFMutableDataRef)bytes,CFSTR("public.jpeg"),1,NULL);
    if (!destination) return nil;
    int orientation=rotation==90?6:rotation==180?3:rotation==270?8:1;
    CGImageDestinationAddImage(destination,image,(__bridge CFDictionaryRef)@{(__bridge NSString *)kCGImageDestinationLossyCompressionQuality:@0.92,(__bridge NSString *)kCGImagePropertyOrientation:@(orientation)});
    bool success=CGImageDestinationFinalize(destination);CFRelease(destination);return success?bytes:nil;
}

#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreImage/CoreImage.h>
static dispatch_queue_t camera_queue(void) { static dispatch_queue_t q;static dispatch_once_t once;dispatch_once(&once,^{q=dispatch_queue_create("husk.camera",dispatch_queue_attr_make_with_autorelease_frequency(DISPATCH_QUEUE_SERIAL,DISPATCH_AUTORELEASE_FREQUENCY_WORK_ITEM));});return q; }
static AVCaptureDevice *camera_device(NSString *identifier) {
    AVCaptureDevicePosition position=[identifier isEqualToString:@"0"]?AVCaptureDevicePositionBack:[identifier isEqualToString:@"1"]?AVCaptureDevicePositionFront:AVCaptureDevicePositionUnspecified;
    return position==AVCaptureDevicePositionUnspecified?nil:[AVCaptureDevice defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera mediaType:AVMediaTypeVideo position:position];
}
static NSArray<NSString *> *camera_ids(void) { NSMutableArray *ids=[NSMutableArray new];for(NSString *i in @[@"0",@"1"])if(camera_device(i))[ids addObject:i];return ids; }
@interface HuskCameraSource:NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
@public pthread_mutex_t lock;CVPixelBufferRef latest;int64_t timestamp,exposure;int iso;
}
@property(atomic) int state;
@property(atomic) BOOL closed;
@property(atomic,strong) AVCaptureSession *session;
@property(nonatomic,strong) AVCaptureDevice *device;
@property(nonatomic,strong) AVCaptureVideoDataOutput *output;
@property(nonatomic) BOOL infinity;
@property(nonatomic) BOOL focusConfigured;
- (void)start;
- (void)requestStart;
- (void)stop;
@end
@implementation HuskCameraSource
- (instancetype)init { if((self=[super init])){pthread_mutex_init(&lock,NULL);[NSNotificationCenter.defaultCenter addObserver:self selector:@selector(background:) name:UIApplicationDidEnterBackgroundNotification object:nil];[NSNotificationCenter.defaultCenter addObserver:self selector:@selector(foreground:) name:UIApplicationDidBecomeActiveNotification object:nil];[NSNotificationCenter.defaultCenter addObserver:self selector:@selector(interrupted:) name:AVCaptureSessionRuntimeErrorNotification object:nil];[NSNotificationCenter.defaultCenter addObserver:self selector:@selector(interrupted:) name:AVCaptureSessionWasInterruptedNotification object:nil];}return self; }
- (void)requestStart { dispatch_async(dispatch_get_main_queue(),^{if(!self.closed && UIApplication.sharedApplication.applicationState==UIApplicationStateActive)dispatch_async(camera_queue(),^{[self start];});}); }
- (void)foreground:(NSNotification *)note { (void)note;[self requestStart]; }
- (void)background:(NSNotification *)note { (void)note; self.closed=YES;self.state=4;dispatch_async(camera_queue(),^{[self stop];}); }
- (void)interrupted:(NSNotification *)note { if(note.object==self.session && !self.closed){self.state=4;tl_log_line("camera: capture session interrupted");dispatch_async(camera_queue(),^{[self stop];});} }
- (void)start {
    if(self.closed)return;
    if(self.session)return;
    if(!self.device){self.state=3;return;}
    if(![NSBundle.mainBundle objectForInfoDictionaryKey:@"NSCameraUsageDescription"]){self.state=4;tl_log_line("camera: missing usage description; access was not requested");return;}
    AVAuthorizationStatus authorization=[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if(authorization==AVAuthorizationStatusNotDetermined){[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted){if(self.closed)return;if(granted)[self requestStart];else self.state=2;}];return;}
    if(authorization!=AVAuthorizationStatusAuthorized){self.state=2;return;}
    NSError *error=nil;AVCaptureDeviceInput *input=[AVCaptureDeviceInput deviceInputWithDevice:self.device error:&error];
    AVCaptureSession *session=[AVCaptureSession new];self.session=session;[session beginConfiguration];
    if([session canSetSessionPreset:AVCaptureSessionPreset1920x1080])session.sessionPreset=AVCaptureSessionPreset1920x1080;
    else if([session canSetSessionPreset:AVCaptureSessionPreset1280x720])session.sessionPreset=AVCaptureSessionPreset1280x720;
    if(!input || ![session canAddInput:input]){[session commitConfiguration];self.state=4;tl_log_line("camera: input failed (%ld)",(long)error.code);return;}
    [session addInput:input];
    AVCaptureVideoDataOutput *output=[AVCaptureVideoDataOutput new];self.output=output;
    output.alwaysDiscardsLateVideoFrames=YES;
    output.videoSettings=@{(NSString *)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)};
    [output setSampleBufferDelegate:self queue:camera_queue()];
    if(![session canAddOutput:output]){[session commitConfiguration];self.state=4;return;}
    [session addOutput:output];
    AVCaptureConnection *connection=[output connectionWithMediaType:AVMediaTypeVideo];
    if(@available(iOS 17.0,*)){if([connection isVideoRotationAngleSupported:0])connection.videoRotationAngle=0;}
    else if(connection.supportsVideoOrientation)connection.videoOrientation=AVCaptureVideoOrientationLandscapeRight;
    if(connection.supportsVideoMirroring){connection.automaticallyAdjustsVideoMirroring=NO;connection.videoMirrored=NO;}
    [session commitConfiguration];
    if([self.device lockForConfiguration:&error]){
        for(AVFrameRateRange *range in self.device.activeFormat.videoSupportedFrameRateRanges)if(range.minFrameRate<=15 && range.maxFrameRate>=15){self.device.activeVideoMinFrameDuration=CMTimeMake(1,15);self.device.activeVideoMaxFrameDuration=CMTimeMake(1,15);break;}
        [self.device unlockForConfiguration];
    }
    [session startRunning];self.state=session.running?1:4;tl_log_line("camera: video source state %d",self.state);
}
- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sample fromConnection:(AVCaptureConnection *)connection {
    (void)output;(void)connection;if(self.closed)return;CVPixelBufferRef frame=CMSampleBufferGetImageBuffer(sample);if(!frame)return;
    double frameTime=CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sample));
    double age=fmax(0,CMTimeGetSeconds(CMClockGetTime(CMClockGetHostTimeClock()))-frameTime);
    int64_t stamp=camera_now()-(int64_t)(age*1e9);
    pthread_mutex_lock(&lock);if(latest)CVPixelBufferRelease(latest);latest=CVPixelBufferRetain(frame);timestamp=stamp>0?stamp:camera_now();exposure=(int64_t)(CMTimeGetSeconds(self.device.exposureDuration)*1e9);iso=(int)self.device.ISO;pthread_mutex_unlock(&lock);
}
- (void)stop { [self.output setSampleBufferDelegate:nil queue:NULL];[self.session stopRunning];self.output=nil;self.session=nil;pthread_mutex_lock(&lock);if(latest){CVPixelBufferRelease(latest);latest=NULL;}pthread_mutex_unlock(&lock); }
- (void)dealloc { [NSNotificationCenter.defaultCenter removeObserver:self];if(latest)CVPixelBufferRelease(latest);pthread_mutex_destroy(&lock); }
@end
static NSMutableDictionary<NSNumber *,HuskCameraSource *> *g_cameras;
static int64_t g_camera_next;
static HuskCameraSource *camera_source(int64_t handle) { @synchronized(HuskCameraSource.class){return g_cameras[@(handle)];} }
static int64_t camera_open(NSString *identifier) {
    HuskCameraSource *source=[HuskCameraSource new];source.device=camera_device(identifier);
    int64_t handle;@synchronized(HuskCameraSource.class){if(!g_cameras)g_cameras=[NSMutableDictionary new];handle=++g_camera_next;g_cameras[@(handle)]=source;}
    [source requestStart];return handle;
}
static void camera_close(int64_t handle) { HuskCameraSource *source=camera_source(handle);source.closed=YES;source.state=4;@synchronized(HuskCameraSource.class){[g_cameras removeObjectForKey:@(handle)];}dispatch_async(camera_queue(),^{[source stop];}); }
static int camera_status(int64_t handle) { HuskCameraSource *source=camera_source(handle);return source?source.state:4; }
static CVPixelBufferRef camera_buffer(int64_t handle) {
    HuskCameraSource *s=camera_source(handle);if(!s || s.closed || s.state!=1)return NULL;
    pthread_mutex_lock(&s->lock);CVPixelBufferRef buffer=s->latest?CVPixelBufferRetain(s->latest):NULL;pthread_mutex_unlock(&s->lock);return buffer;
}
static bool camera_frame(int64_t handle,int64_t values[3]) {
    HuskCameraSource *s=camera_source(handle);if(!s || s.closed || s.state!=1)return false;
    pthread_mutex_lock(&s->lock);bool ready=s->latest!=NULL;if(ready){values[0]=s->timestamp;values[1]=s->exposure;values[2]=s->iso;}pthread_mutex_unlock(&s->lock);return ready;
}
static CIContext *camera_ci(void) { static CIContext *context;static dispatch_once_t once;dispatch_once(&once,^{context=[CIContext contextWithOptions:@{kCIContextCacheIntermediates:@NO}];});return context; }
static CGImageRef camera_rgb(CVPixelBufferRef buffer,int width,int height,bool portrait) {
    CIImage *image=[CIImage imageWithCVPixelBuffer:buffer];if(portrait)image=[image imageByApplyingOrientation:6];
    CGRect extent=image.extent;CGFloat scale=MAX(width/extent.size.width,height/extent.size.height);
    image=[image imageByApplyingTransform:CGAffineTransformMakeScale(scale,scale)];extent=image.extent;
    image=[image imageByApplyingTransform:CGAffineTransformMakeTranslation((width-extent.size.width)/2-extent.origin.x,(height-extent.size.height)/2-extent.origin.y)];
    return [camera_ci() createCGImage:image fromRect:CGRectMake(0,0,width,height)];
}
static NSData *camera_image(int64_t handle,int format,int width,int height,int rotation) {
    CVPixelBufferRef buffer=camera_buffer(handle);if(!buffer)return nil;NSData *result=nil;
    if(format==256){CGImageRef image=camera_rgb(buffer,width,height,false);result=camera_jpeg(image,rotation);if(image)CGImageRelease(image);}
    else if(format==35 && CVPixelBufferGetPlaneCount(buffer)==2){
        CVPixelBufferLockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly);
        size_t sw=CVPixelBufferGetWidthOfPlane(buffer,0),sh=CVPixelBufferGetHeightOfPlane(buffer,0),ys=CVPixelBufferGetBytesPerRowOfPlane(buffer,0),cs=CVPixelBufferGetBytesPerRowOfPlane(buffer,1);
        const uint8_t *y=CVPixelBufferGetBaseAddressOfPlane(buffer,0),*uv=CVPixelBufferGetBaseAddressOfPlane(buffer,1);
        int cw=(width+1)/2,ch=(height+1)/2;NSMutableData *bytes=[NSMutableData dataWithLength:(size_t)width*height+2*(size_t)cw*ch];uint8_t *out=bytes.mutableBytes;
        // Match the JPEG/preview's center crop, keeping chroma offsets even. No megapixel loops in the interpreter.
        size_t rw=sw,rh=sh;if((int64_t)sw*height>(int64_t)sh*width)rw=sh*width/height;else rh=sw*height/width;
        rw=rw/2*2;rh=rh/2*2;size_t ox=((sw-rw)/2)/2*2,oy=((sh-rh)/2)/2*2;
        for(int row=0;row<height;row++)for(int x=0;x<width;x++)out[(size_t)row*width+x]=y[(oy+(size_t)row*rh/height)*ys+ox+(size_t)x*rw/width];
        uint8_t *u=out+(size_t)width*height,*v=u+(size_t)cw*ch;
        for(int row=0;row<ch;row++)for(int x=0;x<cw;x++){size_t p=(oy/2+(size_t)row*(rh/2)/ch)*cs+ox+2*((size_t)x*(rw/2)/cw);u[(size_t)row*cw+x]=uv[p];v[(size_t)row*cw+x]=uv[p+1];}
        CVPixelBufferUnlockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly);result=bytes;
    }
    CVPixelBufferRelease(buffer);return result;
}
static NSData *camera_rgba(int64_t handle,int width,int height) {
    CVPixelBufferRef buffer=camera_buffer(handle);if(!buffer)return nil;CGImageRef image=camera_rgb(buffer,width,height,height>width);CVPixelBufferRelease(buffer);if(!image)return nil;
    NSMutableData *bytes=[NSMutableData dataWithLength:(size_t)width*height*4];CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGContextRef context=CGBitmapContextCreate(bytes.mutableBytes,width,height,8,(size_t)width*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(space);
    if(context){CGContextTranslateCTM(context,0,height);CGContextScaleCTM(context,1,-1);CGContextDrawImage(context,CGRectMake(0,0,width,height),image);CGContextRelease(context);}CGImageRelease(image);return context?bytes:nil;
}
static void camera_focus(int64_t handle,bool infinity) { HuskCameraSource *source=camera_source(handle);dispatch_async(camera_queue(),^{if(!source || source.closed || (source.focusConfigured && source.infinity==infinity))return;source.focusConfigured=YES;source.infinity=infinity;NSError *error=nil;if([source.device lockForConfiguration:&error]){if(infinity && source.device.lockingFocusWithCustomLensPositionSupported)[source.device setFocusModeLockedWithLensPosition:0 completionHandler:nil];else if([source.device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])source.device.focusMode=AVCaptureFocusModeContinuousAutoFocus;[source.device unlockForConfiguration];}}); }

@interface HuskPhotoPicker:NSObject<UIImagePickerControllerDelegate,UINavigationControllerDelegate>
@property(nonatomic,strong) UIImagePickerController *picker;
@property(nonatomic,copy) NSString *path;
@property(atomic) int state;
@end
static HuskPhotoPicker *g_photo;
static int64_t g_photo_token;
static UIViewController *camera_presenter(void) { for(UIScene *scene in UIApplication.sharedApplication.connectedScenes)if([scene isKindOfClass:UIWindowScene.class] && scene.activationState==UISceneActivationStateForegroundActive)for(UIWindow *window in ((UIWindowScene *)scene).windows)if(window.isKeyWindow){UIViewController *vc=window.rootViewController;while(vc.presentedViewController)vc=vc.presentedViewController;return vc;}return nil; }
@implementation HuskPhotoPicker
- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker { self.state=2;[picker dismissViewControllerAnimated:YES completion:nil];self.picker=nil; }
- (void)imagePickerController:(UIImagePickerController *)picker didFinishPickingMediaWithInfo:(NSDictionary<UIImagePickerControllerInfoKey,id> *)info {
    UIImage *image=info[UIImagePickerControllerOriginalImage];
    if(!image){self.state=4;[picker dismissViewControllerAnimated:YES completion:nil];self.picker=nil;return;}
    // Normalize the orientation before returning JPEG bytes to Android's caller.
    UIGraphicsImageRendererFormat *format=[UIGraphicsImageRendererFormat defaultFormat];format.scale=1;
    UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc] initWithSize:image.size format:format];UIImage *upright=[renderer imageWithActions:^(UIGraphicsImageRendererContext *ctx){(void)ctx;[image drawInRect:CGRectMake(0,0,image.size.width,image.size.height)];}];
    NSData *data=UIImageJPEGRepresentation(upright,0.95);NSError *error=nil;
    @synchronized(HuskPhotoPicker.class){if(self==g_photo && self.state==0){BOOL saved=data && [data writeToFile:self.path options:NSDataWritingAtomic error:&error];self.state=saved?1:4;}}
    tl_log_line("camera: photo result %d",self.state);[picker dismissViewControllerAnimated:YES completion:nil];self.picker=nil;
}
@end
static bool camera_photo_current(HuskPhotoPicker *photo,int64_t token) { @synchronized(HuskPhotoPicker.class){return g_photo==photo && g_photo_token==token;} }
static int64_t camera_photo(NSString *path) {
    HuskPhotoPicker *photo=[HuskPhotoPicker new];photo.path=path;
    __block HuskPhotoPicker *previous;int64_t token;
    @synchronized(HuskPhotoPicker.class){previous=g_photo;g_photo=photo;token=++g_photo_token;}
    dispatch_async(dispatch_get_main_queue(),^{
        if(previous.picker){[previous.picker dismissViewControllerAnimated:NO completion:nil];previous.state=2;previous.picker=nil;}
        if(!camera_photo_current(photo,token) || photo.state!=0)return;
        UIViewController *presenter=camera_presenter();
        if(!presenter || ![UIImagePickerController isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]){photo.state=3;return;}
        if(![NSBundle.mainBundle objectForInfoDictionaryKey:@"NSCameraUsageDescription"]){photo.state=4;return;}
        AVAuthorizationStatus status=[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
        void(^show)(void)=^{if(!camera_photo_current(photo,token) || photo.state!=0)return;UIImagePickerController *picker=[UIImagePickerController new];photo.picker=picker;picker.delegate=photo;picker.sourceType=UIImagePickerControllerSourceTypeCamera;picker.cameraCaptureMode=UIImagePickerControllerCameraCaptureModePhoto;[presenter presentViewController:picker animated:YES completion:nil];};
        if(status==AVAuthorizationStatusAuthorized)show();
        else if(status==AVAuthorizationStatusNotDetermined)[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL allowed){dispatch_async(dispatch_get_main_queue(),^{if(allowed)show();else photo.state=2;});}];
        else photo.state=2;
    });return token;
}
static int camera_photo_status(int64_t token) { @synchronized(HuskPhotoPicker.class){return token==g_photo_token?g_photo.state:2;} }
static void camera_cancel_photo(int64_t token) {
    HuskPhotoPicker *photo;@synchronized(HuskPhotoPicker.class){photo=token==g_photo_token?g_photo:nil;photo.state=2;}
    dispatch_async(dispatch_get_main_queue(),^{[photo.picker dismissViewControllerAnimated:NO completion:nil];photo.picker=nil;});
}

#else
/* An explicit test-only pattern exercises CameraX on the desktop; the Mac's physical cameras are never accessed. */
static bool camera_pattern(void) { const char *v=getenv("TL_CAMERA_TEST_PATTERN");return v && !strcmp(v,"1"); }
static NSArray<NSString *> *camera_ids(void) { return camera_pattern()?@[@"0"]:@[]; }
static int64_t camera_open(NSString *identifier) { return camera_pattern() && [identifier isEqualToString:@"0"]?1:0; }
static void camera_close(int64_t handle) { (void)handle; }
static int camera_status(int64_t handle) { return camera_pattern() && handle==1?1:3; }
static bool camera_frame(int64_t handle,int64_t values[3]) { if(camera_status(handle)!=1)return false;values[0]=camera_now();values[1]=5000000;values[2]=100;return true; }
static NSData *camera_rgba(int64_t handle,int width,int height) {
    if(camera_status(handle)!=1)return nil;NSMutableData *data=[NSMutableData dataWithLength:(size_t)width*height*4];uint8_t *p=data.mutableBytes;
    for(int y=0;y<height;y++)for(int x=0;x<width;x++){size_t i=((size_t)y*width+x)*4;bool right=x>=width/2,bottom=y>=height/2;p[i]=right?0:255;p[i+1]=bottom?255:0;p[i+2]=right?255:0;p[i+3]=255;}return data;
}
static NSData *camera_image(int64_t handle,int format,int width,int height,int rotation) {
    if(camera_status(handle)!=1)return nil;
    if(format==35){NSMutableData *data=[NSMutableData dataWithLength:(size_t)width*height+2*(size_t)((width+1)/2)*((height+1)/2)];memset(data.mutableBytes,128,data.length);memset(data.mutableBytes,76,(size_t)width*height);return data;}
    if(format!=256)return nil;NSData *rgba=camera_rgba(handle,width,height);CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();CGContextRef context=CGBitmapContextCreate((void *)rgba.bytes,width,height,8,(size_t)width*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(space);CGImageRef image=CGBitmapContextCreateImage(context);NSData *data=camera_jpeg(image,rotation);CGImageRelease(image);CGContextRelease(context);return data;
}
static void camera_focus(int64_t handle,bool infinity) { (void)handle;(void)infinity; }
static int64_t g_pattern_photo;
static int g_pattern_photo_status;
static int64_t camera_photo(NSString *path) {
    if(!camera_pattern())return 0;g_pattern_photo++;
    if(getenv("TL_CAMERA_TEST_PHOTO_CANCEL")){g_pattern_photo_status=2;return g_pattern_photo;}
    NSData *jpeg=camera_image(1,256,1920,1080,90);g_pattern_photo_status=[jpeg writeToFile:path atomically:YES]?1:4;return g_pattern_photo;
}
static int camera_photo_status(int64_t token) { return camera_pattern()?(token==g_pattern_photo?g_pattern_photo_status:2):3; }
static void camera_cancel_photo(int64_t token) { if(token==g_pattern_photo)g_pattern_photo_status=2; }
#endif

static bool camera_dimensions(int w,int h) { return w>0 && h>0 && w<=4096 && h<=4096 && (int64_t)w*h<=4096ll*2160; }
static bool cam_ids(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;(void)a;NSArray *ids=camera_ids();jobj *array=tl_jni_new_obj_array(tl_jni_class("java/lang/String"),(uint32_t)ids.count);for(NSUInteger i=0;i<ids.count;i++)((jobj **)array->arr.data)[i]=tl_jni_new_string([ids[i] UTF8String]);*ret=L(array);return true; } }
static bool cam_open(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;const char *id=tl_jni_string(a[0].l);*ret=J(camera_open(id?[NSString stringWithUTF8String:id]:@""));return true; } }
static bool cam_status(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;*ret=I(camera_status(a[0].j));return true; } }
static bool cam_close(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;(void)ret;camera_close(a[0].j);return true; } }
static bool cam_frame(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;int64_t frame[3];ret->l=NULL;if(camera_frame(a[0].j,frame)){jobj *array=tl_jni_new_prim_array('J',3);memcpy(array->arr.data,frame,sizeof(frame));*ret=L(array);}return true; } }
static bool cam_image(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;ret->l=NULL;if(!camera_dimensions(a[2].i,a[3].i))return true;NSData *bytes=camera_image(a[0].j,a[1].i,a[2].i,a[3].i,a[4].i);if(bytes){jobj *array=tl_jni_new_prim_array('B',(uint32_t)bytes.length);memcpy(array->arr.data,bytes.bytes,bytes.length);*ret=L(array);}return true; } }
static bool cam_preview(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;(void)ret;if(!a[1].l || !camera_dimensions(a[2].i,a[3].i))return true;NSData *bytes=camera_rgba(a[0].j,a[2].i,a[3].i);if(bytes){jvalue args[4]={J((int64_t)(uintptr_t)bytes.bytes),I(a[2].i),I(a[3].i),I(0)};tl_jni_call(a[1].l,"huskPostRgba","(JIIZ)V",args);}return true; } }
static bool cam_focus(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;(void)ret;camera_focus(a[0].j,a[1].z);return true; } }
static bool cam_photo(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;const char *path=tl_jni_string(a[0].l);*ret=J(camera_photo(path?[NSString stringWithUTF8String:path]:@""));return true; } }
static bool cam_photo_status(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;*ret=I(camera_photo_status(a[0].j));return true; } }
static bool cam_cancel_photo(jobj *self,const jvalue *a,jvalue *ret) { @autoreleasepool { (void)self;(void)ret;camera_cancel_photo(a[0].j);return true; } }
dvm_native_fn tl_camera_native(const char *name,const char *signature) {
    static const struct{const char *name,*signature;dvm_native_fn fn;} methods[]={
        {"ids","()[Ljava/lang/String;",cam_ids},{"open","(Ljava/lang/String;)J",cam_open},{"status","(J)I",cam_status},{"close","(J)V",cam_close},
        {"frame","(J)[J",cam_frame},{"image","(JIIII)[B",cam_image},{"preview","(JLandroid/view/Surface;II)V",cam_preview},{"focus","(JZ)V",cam_focus},
        {"photo","(Ljava/lang/String;)J",cam_photo},{"photoStatus","(J)I",cam_photo_status},{"cancelPhoto","(J)V",cam_cancel_photo}};
    for(size_t i=0;i<sizeof(methods)/sizeof(*methods);i++)if(!strcmp(name,methods[i].name) && !strcmp(signature,methods[i].signature))return methods[i].fn;return NULL;
}
