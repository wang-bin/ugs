/*
 * Copyright (c) 2017-2020 WangBin <wbsecg1 at gmail.com>
 * Universal Graphics Surface
 * Source code: https://github.com/wang-bin/ugs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "ugs/PlatformSurface.h"
#include <TargetConditionals.h>
#if (TARGET_OS_IPHONE+0) // ios or catalyst
#import <UIKit/UIApplication.h>
#import <UIKit/UIView.h>
#include <iostream>

static const NSNotificationName kAppNotifications[] = {
    UIApplicationWillEnterForegroundNotification,
    UIApplicationDidBecomeActiveNotification,
    UIApplicationWillResignActiveNotification,
    UIApplicationDidEnterBackgroundNotification,
    UIApplicationWillTerminateNotification
};

@interface PropertyObserver : NSObject
- (void) observeGeometryChange:(std::function<void(float,float)>)cb; //(void(^)())
- (void) observeAppStateChange:(std::function<void(bool)>)cb; //(void(^)())
@end

@implementation PropertyObserver {
    std::function<void(float,float)> geo_cb_;
    std::function<void(bool)> app_state_cb_;
}

- (void) observeGeometryChange:(std::function<void(float,float)>)cb
{
    geo_cb_ = cb;
}

- (void) observeAppStateChange:(std::function<void(bool)>)cb
{
    app_state_cb_ = cb;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (!geo_cb_)
        return;
    if([keyPath isEqualToString:@"bounds"] || [keyPath isEqualToString:@"frame"]) {
        CGRect rect0, rect;
        if([change objectForKey:@"old"] != [NSNull null])
            rect0 = [[change objectForKey:@"old"] CGRectValue];
        if([object valueForKeyPath:keyPath] != [NSNull null])
            rect = [[object valueForKeyPath:keyPath] CGRectValue];
        const NSString *msg = [NSString stringWithFormat:@"%@ geometry changes from %fx%f to %fx%f", object, rect0.size.width, rect0.size.height, rect.size.width, rect.size.height];
        std::clog << msg.UTF8String << std::endl;
        geo_cb_(rect.size.width, rect.size.height);
    }
}

- (void)appStateChanged:(NSNotification *)notification
{
    NSLog(@"notification %@: %d", [notification name], (int)[UIApplication sharedApplication].applicationState);
    for (auto n : {UIApplicationWillResignActiveNotification, UIApplicationDidEnterBackgroundNotification, UIApplicationWillTerminateNotification}) {
        if ([[notification name] isEqualToString:n]) {
            app_state_cb_(false);
            return;
        }
    }
    app_state_cb_(true);
}
@end

UGS_NS_BEGIN
class UIKitSurface final: public PlatformSurface
{
public:
    UIKitSurface() : PlatformSurface() {
        setNativeHandleChangeCallback([this](void* old){
            auto getLayer = [](NSObject* obj){
                if ([obj isKindOfClass:[CALayer class]])
                    return (CALayer*)obj;
                auto view = (UIView*)obj;
                return view.layer;
            };
            if (old) { // updateNativeWindow(nullptr) called outside or resetNativeHandle(nullptr) in dtor
                CALayer* layer = getLayer((__bridge NSObject*)old);
                [layer removeObserver:observer_ forKeyPath:@"bounds" context:nil];
                CFRelease((CFTypeRef)layer);
            }
            NSObject *obj = (__bridge NSObject*)nativeHandle();
            if (!obj)
                return;
            layer_ = getLayer(obj);
            CFRetain((CFTypeRef)layer_); // what if already retained by passing (__bridge_retained void*)view as parameter?
            // observe view.frame or layer.bounds
            // http://stackoverflow.com/questions/4874288/use-key-value-observing-to-get-a-kvo-callback-on-a-uiviews-frame?noredirect=1&lq=1
            [layer_ addObserver:observer_ forKeyPath:@"bounds" options:NSKeyValueObservingOptionOld context:nil];
        });
        observer_ = [[PropertyObserver alloc] init];
        [observer_ observeGeometryChange:[this](float w, float h){ resize(w, h);}];
        [observer_ observeAppStateChange:[this](bool allowed){
            mtx_.lock();
            gfx_allowed_ = allowed;
            mtx_.unlock();
        }];

        for (auto name : kAppNotifications) {
            [[NSNotificationCenter defaultCenter] addObserver:observer_ selector:@selector(appStateChanged:) name:name object:nil];
        }
    }
    ~UIKitSurface() override {
        if (layer_)
            CFRelease((CFTypeRef)layer_);
        resetNativeHandle(nullptr);
#if !__has_feature(objc_arc)
        if (observer_)
            [observer_ release];
#endif
        for (auto name : kAppNotifications) {
            [[NSNotificationCenter defaultCenter] removeObserver:observer_ name:name object:nil];
        }
    }
    bool size(int *w, int *h) const override {
        if (!layer_)
            return false;
        if (w)
            *w = layer_.bounds.size.width;
        if (h)
            *h = layer_.bounds.size.height;
        return true;
    }

    bool acquire() override {
        mtx_.lock();
        if (!gfx_allowed_)
            mtx_.unlock();
        //printf("gfx_allowed_: %d\n", gfx_allowed_);
        return gfx_allowed_;
    }

    void release() override {
        mtx_.unlock();
    }
private:
    PropertyObserver *observer_ = nil;
    CALayer *layer_ = nil;
    bool gfx_allowed_ = true;
    std::mutex mtx_;
};

PlatformSurface* create_uikit_surface() { return new UIKitSurface();}
UGS_NS_END
#endif // (TARGET_OS_IPHONE+0)