#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include "husk-tl-location.h"

/* Standalone simulator probe using the production Core Location implementation. simctl supplies synthetic coordinates. */
void tl_log_line(const char *format, ...) { va_list args; va_start(args,format); vfprintf(stderr,format,args); fputc('\n',stderr); va_end(args); }
@interface ProbeDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow *window;
@property(nonatomic) int ticks;
@property(nonatomic) BOOL finished;
- (void)start;
@end
@interface ProbeSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
@implementation ProbeSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    (void)session; (void)options;
    self.window=[[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    UIViewController *controller=[UIViewController new]; controller.view.backgroundColor=UIColor.whiteColor;
    self.window.rootViewController=controller; [self.window makeKeyAndVisible];
}
- (void)sceneDidBecomeActive:(UIScene *)scene { (void)scene; [(ProbeDelegate *)UIApplication.sharedApplication.delegate start]; }
@end
@implementation ProbeDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application; (void)options;
    fprintf(stderr,"Core Location probe: launched\n");
    return YES;
}
- (void)start {
    if (self.finished || self.ticks) return;
    self.ticks=1;
    fprintf(stderr,"Core Location probe: active\n");
    husk_location_enable(true);
    [NSTimer scheduledTimerWithTimeInterval:0.25 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
}
- (void)tick:(NSTimer *)timer {
    if (self.finished) return;
    int status=husk_location_status(); double sample[HUSK_LOCATION_SAMPLE_SIZE]={0}; uint64_t sequence=husk_location_read(sample,HUSK_LOCATION_SAMPLE_SIZE);
    BOOL denied=getenv("HUSK_GEO_EXPECT_DENIED")!=NULL;
    BOOL success=denied ? status==HUSK_LOCATION_DENIED && sequence==0 : sequence>0 && status==HUSK_LOCATION_AUTHORIZED && fabs(sample[0]-10)<0.00001 && fabs(sample[1]-20)<0.00001 && sample[3]>=0 && sample[7]>0 && sample[8]>0;
    if (!success && ++self.ticks<120) return;
    self.finished=YES; [timer invalidate]; husk_location_enable(false);
    NSDictionary *result=@{@"success":@(success), @"status":@(status), @"sequence":@(sequence), @"latitude":@(sample[0]), @"longitude":@(sample[1]), @"denied_test":@(denied)};
    NSData *json=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];
    NSURL *documents=[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    [NSFileManager.defaultManager createDirectoryAtURL:documents withIntermediateDirectories:YES attributes:nil error:nil];
    [json writeToURL:[documents URLByAppendingPathComponent:@"result.json"] atomically:YES];
    fprintf(stderr,"Core Location probe: %s\n",success?"PASS":"FAIL");
}
@end
int main(int argc,char **argv) { @autoreleasepool { return UIApplicationMain(argc,argv,nil,NSStringFromClass(ProbeDelegate.class)); } }
