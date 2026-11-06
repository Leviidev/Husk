/* SPDX-License-Identifier: GPL-2.0-or-later */
#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>

#include "husk-tl-http.h"
#include "husk-tl-session.h"

/* Answering a redirect with nil makes the task finish with the 3xx response itself. */
@interface TLHttpNoRedirect : NSObject <NSURLSessionTaskDelegate>
@end
@implementation TLHttpNoRedirect
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task willPerformHTTPRedirection:(NSHTTPURLResponse *)response
        newRequest:(NSURLRequest *)request completionHandler:(void (^)(NSURLRequest *_Nullable))completionHandler
{
    completionHandler(nil);
}
@end

static NSURLSession *shared_session(void)
{
    static NSURLSession *session;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration defaultSessionConfiguration];
        cfg.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        cfg.URLCache = nil;
        session = [NSURLSession sessionWithConfiguration:cfg delegate:[TLHttpNoRedirect new] delegateQueue:nil];
    });
    return session;
}

static void put_message(tl_http_response *out, int error, NSString *text)
{
    out->error = error;
    snprintf(out->message, sizeof(out->message), "%s", text.UTF8String ?: "");
}

static void cancel_request(void *context) { [(__bridge NSURLSessionTask *)context cancel]; }

static bool perform_once(const tl_http_request *req, tl_http_response *out)
{
    memset(out, 0, sizeof(*out));
    @autoreleasepool {
        NSURL *url = req->url ? [NSURL URLWithString:@(req->url)] : nil;
        if (!url || !url.scheme) { put_message(out, TL_HTTP_MALFORMED_URL, [NSString stringWithFormat:@"Malformed URL: %s", req->url ?: ""]); return false; }
        NSMutableURLRequest *r = [NSMutableURLRequest requestWithURL:url];
        r.HTTPMethod = req->method && req->method[0] ? @(req->method) : @"GET";
        r.timeoutInterval = req->timeout_ms > 0 ? req->timeout_ms / 1000.0 : 300.0;
        r.HTTPShouldHandleCookies = YES;
        for (int i = 0; i < req->nheaders; i++) {
            if (!req->header_names[i] || !req->header_values[i]) continue;
            [r addValue:@(req->header_values[i]) forHTTPHeaderField:@(req->header_names[i])];
        }
        if (req->body_len > 0) r.HTTPBody = [NSData dataWithBytes:req->body length:req->body_len];

        __block NSData *data; __block NSURLResponse *resp; __block NSError *err;
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        NSURLSessionDataTask *task = [shared_session() dataTaskWithRequest:r completionHandler:^(NSData *d, NSURLResponse *rs, NSError *e) {
            data = d; resp = rs; err = e;
            dispatch_semaphore_signal(done);
        }];
        tl_session_wait wait;
        bool registered = tl_session_wait_begin(&wait, cancel_request, (__bridge void *)task);
        if (!registered) {
            [task cancel]; put_message(out, TL_HTTP_READ, @"APK session stopped"); return false;
        }
        [task resume];
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
        tl_session_wait_end(&wait);

        if (err || ![resp isKindOfClass:[NSHTTPURLResponse class]]) {
            NSInteger code = err.code;
            int kind = TL_HTTP_SDK;
            if (code == NSURLErrorTimedOut) kind = TL_HTTP_TIMEOUT;
            else if (code == NSURLErrorCannotFindHost || code == NSURLErrorDNSLookupFailed) kind = TL_HTTP_UNKNOWN_HOST;
            else if (code == NSURLErrorServerCertificateUntrusted || code == NSURLErrorServerCertificateHasBadDate || code == NSURLErrorServerCertificateHasUnknownRoot
                     || code == NSURLErrorServerCertificateNotYetValid) kind = TL_HTTP_SSL_UNTRUSTED;
            else if (code == NSURLErrorSecureConnectionFailed || code == NSURLErrorClientCertificateRejected) kind = TL_HTTP_SSL;
            else if (code == NSURLErrorNetworkConnectionLost || code == NSURLErrorCannotConnectToHost || code == NSURLErrorNotConnectedToInternet) kind = TL_HTTP_READ;
            put_message(out, kind, err ? err.localizedDescription : @"not an HTTP response");
            return false;
        }
        NSHTTPURLResponse *http = (NSHTTPURLResponse *)resp;
        out->status = (int)http.statusCode;
        NSDictionary *fields = http.allHeaderFields;
        out->header_names = calloc(fields.count + 1, sizeof(char *));
        out->header_values = calloc(fields.count + 1, sizeof(char *));
        out->header_names[0] = strdup("Status");
        out->header_values[0] = strdup([NSString stringWithFormat:@"HTTP/1.1 %d %@", out->status, [NSHTTPURLResponse localizedStringForStatusCode:http.statusCode]].UTF8String);
        out->nheaders = 1;
        for (id key in fields) {
            NSString *k = [key description], *v = [fields[key] description];
            out->header_names[out->nheaders] = strdup(k.UTF8String ?: "");
            out->header_values[out->nheaders] = strdup(v.UTF8String ?: "");
            out->nheaders++;
        }
        if (data.length) { out->body = malloc(data.length); memcpy(out->body, data.bytes, data.length); out->body_len = data.length; }
        return true;
    }
}

/* 301/302/303 turn a request into a GET without a body; 307/308 repeat it as it was. */
bool tl_http_perform(const tl_http_request *req, tl_http_response *out)
{
    tl_http_request cur = *req;
    char *owned_url = NULL, *owned_method = NULL;
    bool ok = false;
    for (int hops = 0; ; hops++) {
        if (hops) tl_http_response_free(out);            /* the answer to the redirect it is about to follow */
        ok = perform_once(&cur, out);
        if (!ok || !req->follow_redirects || hops >= 10) break;
        int st = out->status;
        if (st != 301 && st != 302 && st != 303 && st != 307 && st != 308) break;
        const char *location = NULL;
        for (int i = 1; i < out->nheaders; i++) if (!strcasecmp(out->header_names[i], "location")) location = out->header_values[i];
        if (!location) break;
        @autoreleasepool {
            NSURL *base = [NSURL URLWithString:@(cur.url)];
            NSURL *next = [NSURL URLWithString:@(location) relativeToURL:base].absoluteURL;
            if (!next) break;
            free(owned_url); owned_url = strdup(next.absoluteString.UTF8String);
            cur.url = owned_url;
        }
        if (st != 307 && st != 308) {
            if (strcasecmp(cur.method ? cur.method : "GET", "HEAD")) { free(owned_method); owned_method = strdup("GET"); cur.method = owned_method; }
            cur.body = NULL; cur.body_len = 0;
        }
    }
    free(owned_url); free(owned_method);
    return ok;
}

void tl_http_response_free(tl_http_response *r)
{
    for (int i = 0; i < r->nheaders; i++) { free(r->header_names[i]); free(r->header_values[i]); }
    free(r->header_names); free(r->header_values); free(r->body);
    memset(r, 0, sizeof(*r));
}
