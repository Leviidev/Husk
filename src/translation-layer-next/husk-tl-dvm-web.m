/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * android.webkit.WebView for Java apps (husk.Web's natives), on WKWebView.
 *
 * Each WebView in the app's view tree is a WKWebView in a container under the app's screen: the view tree leaves a clear hole where
 * the WebView sits (as SurfaceView does), the screen's layer stops being opaque while any web view shows, and touches that land in
 * a web view's frame go through to it. Every call from Java is queued onto the main thread; everything WebKit reports comes back as
 * an event string ("kind \x1f id \x1f fields...") that Java polls (input phase 9) and hands to the WebView's clients.
 *
 * What WebKit cannot do itself goes through Java, asynchronously, by call ids that Java answers with Web.reply / Web.respond:
 *  - shouldOverrideUrlLoading: the navigation waits on its decision handler;
 *  - alert / confirm / prompt, and the addJavascriptInterface bridge (a prompt() whose message starts "husk:"), wait on the panel's
 *    completion handler, so a bridged call is synchronous for the page as on Android;
 *  - the app's own content (file:///android_asset, and http(s) origins the app serves from shouldInterceptRequest, as Capacitor
 *    does) loads as huskapp://<host>/<path>: Java maps those URLs, asks the app, and answers with the bytes, or with the real URL
 *    for the host to fetch.
 */
#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>
#include <TargetConditionals.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
typedef UIView HView;
#else
#import <AppKit/AppKit.h>
typedef NSView HView;
#endif

#include "husk-tl-dvm-internal.h"
#include "husk-tl-dvm-javaapp.h"

void tl_log_line(const char *fmt, ...);

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue L(jobj *o) { jvalue r; r.l = o; return r; }

/* ---- events for Java */

static pthread_mutex_t g_ev_lock = PTHREAD_MUTEX_INITIALIZER;
static char **g_ev;
static int g_nev, g_cev;

static void emit(NSString *kind, int wid, NSArray<NSString *> *fields)
{
    NSMutableString *s = [NSMutableString stringWithFormat:@"%@\x1f%d", kind, wid];
    for (NSString *f in fields) [s appendFormat:@"\x1f%@", f ?: @""];
    const char *u = s.UTF8String;
    pthread_mutex_lock(&g_ev_lock);
    if (g_nev == g_cev) { g_cev = g_cev ? g_cev * 2 : 32; g_ev = realloc(g_ev, (size_t)g_cev * sizeof(*g_ev)); }
    g_ev[g_nev++] = strdup(u ? u : "");
    pthread_mutex_unlock(&g_ev_lock);
    tl_javaapp_touch(9, 0, 0, 0);
}

/* ---- the host's screen and the container the web views live in */

static __weak HView *g_screen;    /* the app's GL view */
static HView *g_container;
static float g_scale = 1;         /* surface pixels per point */
static pthread_mutex_t g_hit_lock = PTHREAD_MUTEX_INITIALIZER;
typedef struct { int id; bool visible, touchable; float x, y, w, h; } hit_rect;   /* points */
static hit_rect g_hits[16];
static int g_nhits;

#if !TARGET_OS_IPHONE
@interface HuskFlippedView : NSView
@end
@implementation HuskFlippedView
- (BOOL)isFlipped { return YES; }
@end
static NSWindow *g_window;
#endif

static void ensure_container(void)
{
    if (g_container) return;
#if TARGET_OS_IPHONE
    tl_log_line("web: the host has given no container for web views");      /* husk_java_web_attach comes before the app starts */
#else
    /* the harness: an off-screen window the size of the app's surface. WebKit only renders a page whose window is on screen:
       this one shows one column of pixels at the screen's right edge */
    CGFloat w = g_screen ? g_screen.frame.size.width : 400, h = g_screen ? g_screen.frame.size.height : 800;
    NSRect scr = NSScreen.mainScreen ? NSScreen.mainScreen.frame : NSMakeRect(0, 0, 1440, 900);
    g_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(NSMaxX(scr) - 1, NSMinY(scr), w, h) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    g_window.ignoresMouseEvents = YES;
    g_window.level = NSFloatingWindowLevel;
    g_container = [[HuskFlippedView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    g_window.contentView = g_container;
    [g_window orderFrontRegardless];
#endif
}

static void set_screen_opaque(bool opaque)
{
#if TARGET_OS_IPHONE
    if (!g_screen) return;
    g_screen.opaque = opaque;
    g_screen.backgroundColor = opaque ? UIColor.blackColor : UIColor.clearColor;
    g_screen.layer.opaque = opaque;
    for (CALayer *l in g_screen.layer.sublayers) l.opaque = opaque;
#else
    (void)opaque;
#endif
}

/* ---- one web view */

@class HuskWeb;
static NSMutableDictionary<NSNumber *, HuskWeb *> *g_views;
static NSMutableDictionary<NSNumber *, id> *g_pending;      /* call id -> completion block / scheme task */
static int g_next_call = 1;
static atomic_int g_visible_webs;

static NSString *bridge_js(void)
{
    /* Android's addJavascriptInterface objects call through prompt(); the console reaches WebChromeClient.onConsoleMessage */
    /* Android's WebView has no speech synthesis: apps put their own (a native TTS plugin) at window.speechSynthesis, which WebKit's
       getter would refuse; it stays WebKit's until they do */
    return @"(function(){if(window.__husk)return;try{Object.defineProperty(window,'speechSynthesis',{value:window.speechSynthesis,writable:true,configurable:true,enumerable:true})}catch(e){}"
            "window.__husk={call:function(o,m,a){var r=prompt('husk:'+JSON.stringify({o:o,m:m,a:Array.prototype.slice.call(a)}));"
            "if(r===null||r===undefined)return undefined;var v=JSON.parse(r);if(v&&v.e)throw new Error(v.e);return v.v;}};"
            "var lv=['log','debug','info','warn','error'];lv.forEach(function(k,i){var o=console[k];console[k]=function(){try{var s=Array.prototype.map.call(arguments,function(x){"
            "try{return typeof x==='object'?JSON.stringify(x):String(x)}catch(e){return String(x)}}).join(' ');"
            "window.webkit.messageHandlers.huskConsole.postMessage({l:i,m:s,s:(document.currentScript&&document.currentScript.src)||location.href});}catch(e){}"
            "if(o)o.apply(console,arguments);};});"
            "window.addEventListener('error',function(e){try{window.webkit.messageHandlers.huskConsole.postMessage({l:4,m:'Uncaught '+e.message+(e.error&&e.error.stack?' | '+String(e.error.stack).slice(0,600):''),s:e.filename||'',n:e.lineno||0});}catch(x){}});})();";
}

@interface HuskWeb : NSObject <WKNavigationDelegate, WKUIDelegate, WKURLSchemeHandler, WKScriptMessageHandler>
@property (nonatomic) int wid;
@property (nonatomic, strong) WKWebView *web;
@property (nonatomic) bool apiLoad;            /* the next navigation is the app's own loadUrl: not offered to shouldOverrideUrlLoading */
@property (nonatomic, strong) NSMutableArray<NSString *> *scripts;
@property (nonatomic) bool edgeLinked;
@end

@implementation HuskWeb
- (instancetype)initWithId:(int)wid
{
    self = [super init];
    _wid = wid;
    _scripts = [NSMutableArray array];
    WKWebViewConfiguration *c = [[WKWebViewConfiguration alloc] init];
    [c setURLSchemeHandler:self forURLScheme:@"huskapp"];
    c.userContentController = [[WKUserContentController alloc] init];
    [c.userContentController addScriptMessageHandler:self name:@"huskConsole"];
    [c.userContentController addUserScript:[[WKUserScript alloc] initWithSource:bridge_js() injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]];
#if TARGET_OS_IPHONE
    c.allowsInlineMediaPlayback = YES;
    c.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
#endif
    @try { [c.preferences setValue:@YES forKey:@"allowFileAccessFromFileURLs"]; } @catch (NSException *e) {}
    _web = [[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 10, 10) configuration:c];
    _web.navigationDelegate = self;
    _web.UIDelegate = self;
    _web.customUserAgent = @"Mozilla/5.0 (Linux; Android 14; Husk; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/124.0.0.0 Mobile Safari/537.36";
    if (@available(iOS 16.4, macOS 13.3, *)) _web.inspectable = YES;
#if TARGET_OS_IPHONE
    _web.opaque = NO;
    _web.backgroundColor = UIColor.whiteColor;
    _web.scrollView.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
#endif
    _web.hidden = YES;
#if !TARGET_OS_IPHONE
    /* the harness's window is off-screen: without this the page counts as hidden (no animation frames, nothing composited) */
    @try { [_web setValue:@NO forKey:@"_windowOcclusionDetectionEnabled"]; } @catch (NSException *e) {}
#endif
    [_web addObserver:self forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:NULL];
    [_web addObserver:self forKeyPath:@"estimatedProgress" options:NSKeyValueObservingOptionNew context:NULL];
    [_web addObserver:self forKeyPath:@"URL" options:NSKeyValueObservingOptionNew context:NULL];
    ensure_container();
    if (g_container) [g_container addSubview:_web];
    return self;
}
- (void)teardown
{
    [_web removeObserver:self forKeyPath:@"title"];
    [_web removeObserver:self forKeyPath:@"estimatedProgress"];
    [_web removeObserver:self forKeyPath:@"URL"];
    [_web stopLoading];
    [_web.configuration.userContentController removeScriptMessageHandlerForName:@"huskConsole"];
    _web.navigationDelegate = nil;
    _web.UIDelegate = nil;
    [_web removeFromSuperview];
}
- (void)observeValueForKeyPath:(NSString *)kp ofObject:(id)o change:(NSDictionary *)ch context:(void *)ctx
{
    if ([kp isEqualToString:@"title"]) emit(@"t", _wid, @[ _web.title ?: @"" ]);
    else if ([kp isEqualToString:@"estimatedProgress"]) emit(@"p", _wid, @[ [NSString stringWithFormat:@"%d", (int)(_web.estimatedProgress * 100)] ]);
    else [self history];
}
- (void)history
{
    emit(@"h", _wid, @[ _web.canGoBack ? @"1" : @"0", _web.canGoForward ? @"1" : @"0", _web.URL.absoluteString ?: @"" ]);
}
- (int)park:(id)thing
{
    int call = g_next_call++;
    g_pending[@(call)] = thing;
    return call;
}

/* navigation */
- (void)webView:(WKWebView *)w decidePolicyForNavigationAction:(WKNavigationAction *)act decisionHandler:(void (^)(WKNavigationActionPolicy))decide
{
    NSURL *u = act.request.URL;
    NSString *scheme = u.scheme.lowercaseString ?: @"";
    bool main = act.targetFrame == nil || act.targetFrame.isMainFrame;
    if (!act.targetFrame) {                             /* target=_blank / window.open: Android loads it in this view */
        decide(WKNavigationActionPolicyCancel);
        [w loadRequest:act.request];
        return;
    }
    if (_apiLoad && main) { _apiLoad = false; decide(WKNavigationActionPolicyAllow); return; }
    bool web = [scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"] || [scheme isEqualToString:@"huskapp"] ||
               [scheme isEqualToString:@"about"] || [scheme isEqualToString:@"data"] || [scheme isEqualToString:@"blob"] || [scheme isEqualToString:@"file"];
    if (!main && web) { decide(WKNavigationActionPolicyAllow); return; }
    int call = [self park:[decide copy]];
    emit(@"o", _wid, @[ [NSString stringWithFormat:@"%d", call], u.absoluteString ?: @"", main ? @"1" : @"0",
                        act.navigationType == WKNavigationTypeLinkActivated ? @"1" : @"0", act.request.HTTPMethod ?: @"GET", web ? @"1" : @"0" ]);
}
- (void)webView:(WKWebView *)w didStartProvisionalNavigation:(WKNavigation *)n { emit(@"s", _wid, @[ w.URL.absoluteString ?: @"" ]); }
- (void)webView:(WKWebView *)w didCommitNavigation:(WKNavigation *)n { emit(@"m", _wid, @[ w.URL.absoluteString ?: @"" ]); [self history]; }
- (void)webView:(WKWebView *)w didFinishNavigation:(WKNavigation *)n { [self history]; emit(@"f", _wid, @[ w.URL.absoluteString ?: @"" ]); }
- (void)failed:(NSError *)e
{
    if ([e.domain isEqualToString:NSURLErrorDomain] && e.code == NSURLErrorCancelled) return;
    if ([e.domain isEqualToString:@"WebKitErrorDomain"] && e.code == 102) return;      /* frame load interrupted by a policy change */
    NSString *url = [e.userInfo[NSURLErrorFailingURLStringErrorKey] description] ?: _web.URL.absoluteString ?: @"";
    emit(@"E", _wid, @[ [NSString stringWithFormat:@"%ld", (long)e.code], e.localizedDescription ?: @"", url ]);
}
- (void)webView:(WKWebView *)w didFailNavigation:(WKNavigation *)n withError:(NSError *)e { [self failed:e]; }
- (void)webView:(WKWebView *)w didFailProvisionalNavigation:(WKNavigation *)n withError:(NSError *)e { [self failed:e]; }
- (void)webViewWebContentProcessDidTerminate:(WKWebView *)w { emit(@"g", _wid, @[]); }

/* dialogs and the bridge */
- (void)webView:(WKWebView *)w runJavaScriptAlertPanelWithMessage:(NSString *)msg initiatedByFrame:(WKFrameInfo *)f completionHandler:(void (^)(void))done
{
    int call = [self park:[done copy]];
    emit(@"A", _wid, @[ [NSString stringWithFormat:@"%d", call], f.request.URL.absoluteString ?: @"", msg ?: @"" ]);
}
- (void)webView:(WKWebView *)w runJavaScriptConfirmPanelWithMessage:(NSString *)msg initiatedByFrame:(WKFrameInfo *)f completionHandler:(void (^)(BOOL))done
{
    int call = [self park:[done copy]];
    emit(@"C", _wid, @[ [NSString stringWithFormat:@"%d", call], f.request.URL.absoluteString ?: @"", msg ?: @"" ]);
}
- (void)webView:(WKWebView *)w runJavaScriptTextInputPanelWithPrompt:(NSString *)msg defaultText:(NSString *)def initiatedByFrame:(WKFrameInfo *)f completionHandler:(void (^)(NSString *))done
{
    int call = [self park:[done copy]];
    if ([msg hasPrefix:@"husk:"]) emit(@"j", _wid, @[ [NSString stringWithFormat:@"%d", call], [msg substringFromIndex:5] ]);
    else emit(@"P", _wid, @[ [NSString stringWithFormat:@"%d", call], f.request.URL.absoluteString ?: @"", msg ?: @"", def ?: @"" ]);
}
- (WKWebView *)webView:(WKWebView *)w createWebViewWithConfiguration:(WKWebViewConfiguration *)c forNavigationAction:(WKNavigationAction *)act windowFeatures:(WKWindowFeatures *)wf
{
    [w loadRequest:act.request];
    return nil;
}
- (void)webViewDidClose:(WKWebView *)w { emit(@"x", _wid, @[]); }
- (void)userContentController:(WKUserContentController *)ucc didReceiveScriptMessage:(WKScriptMessage *)m
{
    NSDictionary *d = [m.body isKindOfClass:NSDictionary.class] ? m.body : @{};
    emit(@"c", _wid, @[ [NSString stringWithFormat:@"%@", d[@"l"] ?: @0], [NSString stringWithFormat:@"%@", d[@"m"] ?: @""],
                        [NSString stringWithFormat:@"%@", d[@"s"] ?: @""], [NSString stringWithFormat:@"%@", d[@"n"] ?: @0] ]);
}

/* the app's own content */
- (void)webView:(WKWebView *)w startURLSchemeTask:(id<WKURLSchemeTask>)task
{
    int call = [self park:task];
    NSURLRequest *r = task.request;
    NSMutableArray *f = [NSMutableArray arrayWithArray:@[ [NSString stringWithFormat:@"%d", call], r.URL.absoluteString ?: @"", r.HTTPMethod ?: @"GET",
                                                          r.mainDocumentURL && [r.mainDocumentURL isEqual:r.URL] ? @"1" : @"0" ]];
    [r.allHTTPHeaderFields enumerateKeysAndObjectsUsingBlock:^(NSString *k, NSString *v, BOOL *stop) { [f addObject:k]; [f addObject:v]; }];
    emit(@"r", _wid, f);
}
- (void)webView:(WKWebView *)w stopURLSchemeTask:(id<WKURLSchemeTask>)task
{
    for (NSNumber *k in g_pending.allKeys) if (g_pending[k] == task) [g_pending removeObjectForKey:k];
}
@end

/* ---- husk.Web's natives: the main thread does the work */

static NSString *nstr(jobj *s) { const char *u = s ? tl_jni_string(s) : NULL; return u ? [NSString stringWithUTF8String:u] : nil; }
static NSData *nbytes(jobj *b) { return b && b->kind == TL_K_PRIM_ARRAY ? [NSData dataWithBytes:b->arr.data length:b->arr.len] : nil; }
static NSArray<NSString *> *nstrs(jobj *arr)
{
    NSMutableArray *out = [NSMutableArray array];
    if (arr && arr->kind == TL_K_OBJ_ARRAY) for (uint32_t i = 0; i < arr->oarr.len; i++) [out addObject:nstr(arr->oarr.v[i]) ?: @""];
    return out;
}
static void on_main(dispatch_block_t b) { dispatch_async(dispatch_get_main_queue(), b); }
static HuskWeb *view_of(int wid) { return g_views[@(wid)]; }

static void update_hits(int wid, bool visible, bool touchable, CGRect r)
{
    pthread_mutex_lock(&g_hit_lock);
    int i = 0;
    while (i < g_nhits && g_hits[i].id != wid) i++;
    if (i == g_nhits && g_nhits < 16) g_nhits++;
    if (i < 16) g_hits[i] = (hit_rect){ wid, visible, touchable, (float)r.origin.x, (float)r.origin.y, (float)r.size.width, (float)r.size.height };
    int n = 0;
    for (int k = 0; k < g_nhits; k++) if (g_hits[k].visible) n++;
    pthread_mutex_unlock(&g_hit_lock);
    atomic_store(&g_visible_webs, n);
}
static void forget_hits(int wid)
{
    pthread_mutex_lock(&g_hit_lock);
    for (int i = 0; i < g_nhits; i++) if (g_hits[i].id == wid) { g_hits[i] = g_hits[--g_nhits]; break; }
    int n = 0;
    for (int k = 0; k < g_nhits; k++) if (g_hits[k].visible) n++;
    pthread_mutex_unlock(&g_hit_lock);
    atomic_store(&g_visible_webs, n);
}

NAT(W_create)
{
    (void)self; (void)ret;
    int wid = a[0].i;
    on_main(^{
        if (!g_views) { g_views = [NSMutableDictionary dictionary]; g_pending = [NSMutableDictionary dictionary]; }
        if (!g_views[@(wid)]) g_views[@(wid)] = [[HuskWeb alloc] initWithId:wid];
    });
    return true;
}
NAT(W_destroy)
{
    (void)self; (void)ret;
    int wid = a[0].i;
    forget_hits(wid);
    on_main(^{
        HuskWeb *h = view_of(wid);
        [h teardown];
        [g_views removeObjectForKey:@(wid)];
        if (atomic_load(&g_visible_webs) == 0) set_screen_opaque(true);
    });
    return true;
}
NAT(W_frame)
{
    (void)self; (void)ret;
    int wid = a[0].i;
    bool vis = a[5].i != 0, touchable = a[6].i != 0;
    CGRect r = CGRectMake(a[1].i / g_scale, a[2].i / g_scale, a[3].i / g_scale, a[4].i / g_scale);
    update_hits(wid, vis, touchable, r);
    on_main(^{
        HuskWeb *h = view_of(wid);
        if (!h) return;
        /* the screen may have had no superview when the web view was made */
        if (!g_container) ensure_container();
        if (g_container && !h.web.superview) [g_container addSubview:h.web];
#if TARGET_OS_IPHONE
        /* the back swipe from the screen's edge wins over the page's own scrolling */
        if (!h.edgeLinked) {
            for (UIGestureRecognizer *g in g_container.gestureRecognizers)
                if ([g isKindOfClass:UIScreenEdgePanGestureRecognizer.class]) { [h.web.scrollView.panGestureRecognizer requireGestureRecognizerToFail:g]; h.edgeLinked = true; }
        }
#endif
#if TARGET_OS_IPHONE
        if (g_screen && g_container && g_container.superview == g_screen.superview && !CGRectEqualToRect(g_container.frame, g_screen.frame)) g_container.frame = g_screen.frame;
#endif
        h.web.frame = r;
        h.web.hidden = !vis;
        set_screen_opaque(atomic_load(&g_visible_webs) == 0);
    });
    return true;
}
NAT(W_load)
{
    (void)self; (void)ret;
    int wid = a[0].i;
    NSString *url = nstr(a[1].l);
    NSArray *hdr = nstrs(a[2].l);
    NSData *body = nbytes(a[3].l);
    on_main(^{
        HuskWeb *h = view_of(wid);
        NSURL *u = url ? [NSURL URLWithString:url] : nil;
        if (!h || !u) { tl_log_line("web: cannot load %s", url.UTF8String ?: "(null)"); return; }
        NSMutableURLRequest *r = [NSMutableURLRequest requestWithURL:u];
        for (NSUInteger i = 0; i + 1 < hdr.count; i += 2) [r setValue:hdr[i + 1] forHTTPHeaderField:hdr[i]];
        if (body) { r.HTTPMethod = @"POST"; r.HTTPBody = body; }
        h.apiLoad = true;
        if (u.isFileURL) [h.web loadFileURL:u allowingReadAccessToURL:[NSURL fileURLWithPath:@"/"]];
        else [h.web loadRequest:r];
    });
    return true;
}
NAT(W_loadData)
{
    (void)self; (void)ret;
    int wid = a[0].i;
    NSData *data = nbytes(a[1].l);
    NSString *mime = nstr(a[2].l) ?: @"text/html", *enc = nstr(a[3].l) ?: @"utf-8", *base = nstr(a[4].l);
    on_main(^{
        HuskWeb *h = view_of(wid);
        if (!h) return;
        h.apiLoad = true;
        [h.web loadData:data ?: [NSData data] MIMEType:mime characterEncodingName:enc baseURL:(base ? [NSURL URLWithString:base] : nil) ?: [NSURL URLWithString:@"about:blank"]];
    });
    return true;
}
NAT(W_eval)
{
    (void)self; (void)ret;
    int wid = a[0].i, cb = a[2].i;
    NSString *js = nstr(a[1].l) ?: @"";
    on_main(^{
        HuskWeb *h = view_of(wid);
        if (!h) return;
        if (!cb) { [h.web evaluateJavaScript:js completionHandler:nil]; return; }
        /* Android hands the callback the result as JSON */
        NSData *q = [NSJSONSerialization dataWithJSONObject:@[ js ] options:0 error:nil];
        NSString *quoted = [[NSString alloc] initWithData:q encoding:NSUTF8StringEncoding];
        NSString *wrapped = [NSString stringWithFormat:@"(function(){var r=(0,eval)(%@[0]);try{var s=JSON.stringify(r);return s===undefined?'null':s}catch(e){return 'null'}})()", quoted];
        [h.web evaluateJavaScript:wrapped completionHandler:^(id result, NSError *err) {
            if (err) tl_log_line("web: evaluateJavascript: %s", err.localizedDescription.UTF8String);
            emit(@"v", wid, @[ [NSString stringWithFormat:@"%d", cb], [result isKindOfClass:NSString.class] ? result : @"null" ]);
        }];
    });
    return true;
}
NAT(W_userScript)
{
    (void)self; (void)ret;
    int wid = a[0].i;
    NSString *js = nstr(a[1].l) ?: @"";
    on_main(^{
        HuskWeb *h = view_of(wid);
        if (!h) return;
        [h.scripts addObject:js];
        [h.web.configuration.userContentController addUserScript:[[WKUserScript alloc] initWithSource:js injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]];
        [h.web evaluateJavaScript:js completionHandler:nil];      /* and the page already showing */
    });
    return true;
}
NAT(W_settings)
{
    (void)self; (void)ret;
    int wid = a[0].i, zoom = a[3].i;
    bool js = a[1].i != 0, gesture = a[4].i != 0;
    NSString *ua = nstr(a[2].l);
    on_main(^{
        HuskWeb *h = view_of(wid);
        if (!h) return;
        if (@available(iOS 14.0, macOS 11.0, *)) h.web.configuration.defaultWebpagePreferences.allowsContentJavaScript = js;
        else h.web.configuration.preferences.javaScriptEnabled = js;
        if (ua.length) h.web.customUserAgent = ua;
        if (@available(iOS 14.0, macOS 11.0, *)) h.web.pageZoom = zoom > 0 ? zoom / 100.0 : 1.0;
        (void)gesture;
    });
    return true;
}
NAT(W_go)
{
    (void)self; (void)ret;
    int wid = a[0].i, cmd = a[1].i;
    on_main(^{
        HuskWeb *h = view_of(wid);
        if (!h) return;
        switch (cmd) {
        case 0: [h.web goBack]; break;
        case 1: [h.web goForward]; break;
        case 2: [h.web reload]; break;
        case 3: [h.web stopLoading]; break;
        }
    });
    return true;
}
NAT(W_background)
{
    (void)self; (void)ret;
    int wid = a[0].i;
    uint32_t c = (uint32_t)a[1].i;
    on_main(^{
        HuskWeb *h = view_of(wid);
        if (!h) return;
#if TARGET_OS_IPHONE
        UIColor *col = [UIColor colorWithRed:((c >> 16) & 255) / 255.0 green:((c >> 8) & 255) / 255.0 blue:(c & 255) / 255.0 alpha:(c >> 24) / 255.0];
        h.web.backgroundColor = col;
        h.web.scrollView.backgroundColor = col;
        h.web.opaque = (c >> 24) == 255;
#else
        @try { [h.web setValue:@((c >> 24) == 255) forKey:@"drawsBackground"]; } @catch (NSException *e) {}
#endif
    });
    return true;
}
NAT(W_reply)
{
    (void)self; (void)ret;
    int call = a[0].i;
    NSString *r = nstr(a[1].l);
    bool isNull = a[1].l == NULL;
    on_main(^{
        id thing = g_pending[@(call)];
        if (!thing) return;
        [g_pending removeObjectForKey:@(call)];
        /* which kind of waiter this is shows in the block's type only to the compiler: the reply's form says it */
        if ([r isEqualToString:@"\x01nav-allow"]) ((void (^)(WKNavigationActionPolicy))thing)(WKNavigationActionPolicyAllow);
        else if ([r isEqualToString:@"\x01nav-cancel"]) ((void (^)(WKNavigationActionPolicy))thing)(WKNavigationActionPolicyCancel);
        else if ([r isEqualToString:@"\x01alert"]) ((void (^)(void))thing)();
        else if ([r isEqualToString:@"\x01yes"]) ((void (^)(BOOL))thing)(YES);
        else if ([r isEqualToString:@"\x01no"]) ((void (^)(BOOL))thing)(NO);
        else ((void (^)(NSString *))thing)(isNull ? nil : r);
    });
    return true;
}
NAT(W_respond)
{
    (void)self; (void)ret;
    int call = a[0].i, status = a[1].i;
    NSString *reason = nstr(a[2].l), *mime = nstr(a[3].l), *enc = nstr(a[4].l);
    NSArray *hdr = nstrs(a[5].l);
    NSData *body = nbytes(a[6].l);
    on_main(^{
        id<WKURLSchemeTask> task = g_pending[@(call)];
        if (!task) return;
        if (status == 0) {
            /* not the app's: fetch the real URL (reason) and hand that over */
            NSURL *real = reason ? [NSURL URLWithString:reason] : nil;
            if (!real) { [g_pending removeObjectForKey:@(call)]; [task didFailWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil]]; return; }
            NSMutableURLRequest *r = [task.request mutableCopy];
            r.URL = real;
            [[NSURLSession.sharedSession dataTaskWithRequest:r completionHandler:^(NSData *d, NSURLResponse *resp, NSError *err) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (!g_pending[@(call)]) return;
                    [g_pending removeObjectForKey:@(call)];
                    if (err) { [task didFailWithError:err]; return; }
                    NSHTTPURLResponse *hr = [resp isKindOfClass:NSHTTPURLResponse.class] ? (NSHTTPURLResponse *)resp : nil;
                    NSHTTPURLResponse *out = [[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:hr ? hr.statusCode : 200 HTTPVersion:@"HTTP/1.1"
                                                                      headerFields:hr ? hr.allHeaderFields : @{ @"Content-Type": resp.MIMEType ?: @"application/octet-stream" }];
                    [task didReceiveResponse:out];
                    if (d) [task didReceiveData:d];
                    [task didFinish];
                });
            }] resume];
            return;
        }
        [g_pending removeObjectForKey:@(call)];
        if (status < 0) { [task didFailWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil]]; return; }
        NSMutableDictionary *h = [NSMutableDictionary dictionary];
        for (NSUInteger i = 0; i + 1 < hdr.count; i += 2) h[hdr[i]] = hdr[i + 1];
        NSString *type = mime.length ? mime : @"application/octet-stream";
        if (enc.length && ![type containsString:@"charset"]) type = [NSString stringWithFormat:@"%@; charset=%@", type, enc];
        if (!h[@"Content-Type"] && !h[@"content-type"]) h[@"Content-Type"] = type;
        if (!h[@"Access-Control-Allow-Origin"]) h[@"Access-Control-Allow-Origin"] = @"*";
        NSHTTPURLResponse *resp = [[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:status HTTPVersion:@"HTTP/1.1" headerFields:h];
        [task didReceiveResponse:resp];
        if (body.length) [task didReceiveData:body];
        [task didFinish];
    });
    return true;
}
NAT(W_poll)
{
    (void)self; (void)a;
    pthread_mutex_lock(&g_ev_lock);
    int n = g_nev;
    if (!n) { pthread_mutex_unlock(&g_ev_lock); *ret = L(NULL); return true; }
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/String;");
    arr->refs = 1u << 30;
    for (int i = 0; i < n; i++) { arr->oarr.v[i] = dvm_new_string_utf8(g_ev[i]); free(g_ev[i]); }
    g_nev = 0;
    pthread_mutex_unlock(&g_ev_lock);
    *ret = L(arr);
    return true;
}

static const struct { const char *name, *sig; dvm_native_fn fn; } k_web[] = {
    { "create", "(I)V", W_create },
    { "destroy", "(I)V", W_destroy },
    { "frame", "(IIIIIZZ)V", W_frame },
    { "load", "(ILjava/lang/String;[Ljava/lang/String;[B)V", W_load },
    { "loadData", "(I[BLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", W_loadData },
    { "eval", "(ILjava/lang/String;I)V", W_eval },
    { "userScript", "(ILjava/lang/String;)V", W_userScript },
    { "settings", "(IZLjava/lang/String;IZ)V", W_settings },
    { "go", "(II)V", W_go },
    { "background", "(II)V", W_background },
    { "reply", "(ILjava/lang/String;)V", W_reply },
    { "respond", "(IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[B)V", W_respond },
    { "poll", "()[Ljava/lang/String;", W_poll },
    { NULL, NULL, NULL },
};
dvm_native_fn tl_web_native(const char *name, const char *sig);
dvm_native_fn tl_web_native(const char *name, const char *sig)
{
    for (int i = 0; k_web[i].name; i++) if (!strcmp(k_web[i].name, name) && !strcmp(k_web[i].sig, sig)) return k_web[i].fn;
    return NULL;
}

/* ---- the host's side */

void tl_web_attach(void *screen_view, void *container, float scale, bool fresh)
{
    HView *view = (__bridge HView *)screen_view, *box = (__bridge HView *)container;
    float s = scale > 0 ? scale : 1;
    dispatch_block_t b = ^{
        if (fresh) {
            /* a new app on a new screen: whatever the last one left goes */
            for (HuskWeb *h in g_views.allValues) [h teardown];
            [g_views removeAllObjects];
            [g_pending removeAllObjects];
            pthread_mutex_lock(&g_hit_lock);
            g_nhits = 0;
            pthread_mutex_unlock(&g_hit_lock);
            atomic_store(&g_visible_webs, 0);
        } else {
            /* the same app shown again on a new screen: its pages move over */
            for (HuskWeb *h in g_views.allValues) if (box) [box addSubview:h.web];
        }
        if (g_container != box) [g_container removeFromSuperview];
        g_container = box;
        g_screen = view;
        g_scale = s;
        set_screen_opaque(atomic_load(&g_visible_webs) == 0);
    };
    if ([NSThread isMainThread]) b(); else dispatch_sync(dispatch_get_main_queue(), b);
}

bool tl_web_hit(float x, float y)
{
    bool hit = false;
    pthread_mutex_lock(&g_hit_lock);
    for (int i = 0; i < g_nhits && !hit; i++) {
        const hit_rect *r = &g_hits[i];
        hit = r->id && r->visible && r->touchable && x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
    }
    pthread_mutex_unlock(&g_hit_lock);
    return hit;
}

bool tl_web_any_visible(void) { return atomic_load(&g_visible_webs) > 0; }

#if !TARGET_OS_IPHONE
/* The harness: a run loop on the main thread for WebKit, taps as clicks, and the web views' pixels for screenshots. */
void tl_web_harness_size(int w, int h, float scale)
{
    g_scale = scale > 0 ? scale : 1;
    dispatch_async(dispatch_get_main_queue(), ^{
        static NSView *screen;                          /* g_screen is weak: this keeps the stand-in alive */
        screen = [[HuskFlippedView alloc] initWithFrame:NSMakeRect(0, 0, w / g_scale, h / g_scale)];
        g_screen = screen;
    });
}
void tl_web_main_loop(void)
{
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp finishLaunching];
    CFRunLoopRun();
}
bool tl_web_tap(float px, float py)
{
    float x = px / g_scale, y = py / g_scale;
    int wid = 0;
    float ox = 0, oy = 0;
    pthread_mutex_lock(&g_hit_lock);
    for (int i = 0; i < g_nhits; i++) {
        const hit_rect *r = &g_hits[i];
        if (r->id && r->visible && r->touchable && x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h) { wid = r->id; ox = r->x; oy = r->y; break; }
    }
    pthread_mutex_unlock(&g_hit_lock);
    if (!wid) return false;
    float lx = x - ox, ly = y - oy;
    dispatch_async(dispatch_get_main_queue(), ^{
        HuskWeb *h = view_of(wid);
        NSString *js = [NSString stringWithFormat:@"(function(){var e=document.elementFromPoint(%f,%f);if(!e)return 'none';"
                        "['pointerdown','mousedown','pointerup','mouseup'].forEach(function(t){e.dispatchEvent(new MouseEvent(t,{bubbles:true,cancelable:true,clientX:%f,clientY:%f}))});"
                        "if(e.focus)e.focus();e.click();return e.tagName+'#'+(e.id||'')+'.'+(e.className||'');})()", lx, ly, lx, ly];
        [h.web evaluateJavaScript:js completionHandler:^(id r, NSError *err) { tl_log_line("web: tap %.0f,%.0f -> %s", lx, ly, [[r description] UTF8String] ?: "?"); }];
    });
    return true;
}
void tl_web_snapshot(const char *png)
{
    NSString *path = [NSString stringWithUTF8String:png];
    dispatch_async(dispatch_get_main_queue(), ^{
        for (HuskWeb *h in g_views.allValues) {
            if (h.web.hidden) continue;
            [h.web takeSnapshotWithConfiguration:nil completionHandler:^(NSImage *img, NSError *err) {
                if (!img) { tl_log_line("web: snapshot failed: %s", err.localizedDescription.UTF8String ?: "?"); return; }
                CGImageRef cg = [img CGImageForProposedRect:NULL context:nil hints:nil];
                NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cg];
                [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path atomically:YES];
                tl_log_line("web: snapshot of %d -> %s", h.wid, png);
            }];
            break;
        }
    });
}
void tl_web_eval_log(const char *js)
{
    NSString *s = [NSString stringWithUTF8String:js];
    dispatch_async(dispatch_get_main_queue(), ^{
        for (HuskWeb *h in g_views.allValues)
            [h.web evaluateJavaScript:s completionHandler:^(id r, NSError *err) { tl_log_line("web: eval -> %s", err ? err.localizedDescription.UTF8String : [[r description] UTF8String] ?: "null"); }];
    });
}
#endif
