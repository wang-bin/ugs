/*
 * Copyright (c) 2020-2026 WangBin <wbsecg1 at gmail.com>
 * AI participated
 * Universal Graphics Surface
 * Source code: https://github.com/wang-bin/ugs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "ugs/PlatformSurface.h"
#import <QuartzCore/CALayer.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#define USE_CFNOTIFICATION
UGS_NS_BEGIN
// Surface events use backing pixels; NSView geometry and CALayer frames use points.
static CGSize backing_size(NSView* view)
{
    auto size = [view convertSizeToBacking:view.bounds.size];
    if (size.width <= 0 || size.height <= 0) {
        auto scale = view.window ? view.window.backingScaleFactor : view.layer.contentsScale;
        if (scale < 1)
            scale = [NSScreen mainScreen].backingScaleFactor;
        size = CGSizeMake(view.bounds.size.width * scale, view.bounds.size.height * scale);
    }
    return size;
}

static void resize_surface(PlatformSurface* surface, NSView* view)
{
    const auto size = backing_size(view);
    surface->resize(static_cast<int>(size.width), static_cast<int>(size.height));
}

class CocoaSurface final: public PlatformSurface
{
public:
    CocoaSurface() : PlatformSurface() {
        setNativeHandleChangeCallback([this](void* old){
            auto getView = [](NSObject* obj){
                if ([obj isKindOfClass:[CALayer class]])
                    return (NSView*)nil;
                if ([obj isKindOfClass:[NSWindow class]]) {
                    auto win = (NSWindow*)obj;
                    return (NSView*)win.contentView;
                }
                return (NSView*)obj;
            };
            if (old) { // updateNativeWindow(nullptr) called outside or resetNativeHandle(nullptr) in dtor
                auto view = getView((__bridge NSObject*)old);
                if (view) {
#ifdef USE_CFNOTIFICATION
                    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this, CFSTR("NSViewFrameDidChangeNotification"), nullptr);
#endif
                }
                if (observer_)
                    [[NSNotificationCenter defaultCenter] removeObserver:observer_];
                //CFRelease((CFTypeRef)view);
            }
            auto obj = (__bridge NSObject*)nativeHandle();
            if (!obj)
                return;
            view_ = getView(obj);
            if (!view_)
                return;
#ifdef USE_CFNOTIFICATION // TODO: move to ugs surface like UIKitSurface
            CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this
                , [](CFNotificationCenterRef center, void *observer, CFNotificationName name, const void *object, CFDictionaryRef userInfo){
                    auto s = static_cast<CocoaSurface*>(observer);
                    auto view = (__bridge NSView*)object;
                    resize_surface(s, view);
                }
                , CFSTR("NSViewFrameDidChangeNotification")
                , (const void*)view_
                , CFNotificationSuspensionBehaviorDeliverImmediately
            );
#else
            observer_ = [[NSNotificationCenter defaultCenter] addObserverForName:NSViewFrameDidChangeNotification
                object:view_
                queue:[NSOperationQueue mainQueue] // or nil
                usingBlock:^(NSNotification *note) {
                    resize_surface(this, view_);
            }];
#endif
        });
    }

    ~CocoaSurface() override {
        //if (layer_)
          //  CFRelease((CFTypeRef)layer_);
        resetNativeHandle(nullptr);
#ifdef USE_CFNOTIFICATION
        CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this, CFSTR("NSViewFrameDidChangeNotification"), nullptr);
#else
        [[NSNotificationCenter defaultCenter] removeObserver:observer_];
# if !__has_feature(objc_arc)
        [observer_ release];
# endif
#endif
    }
    bool size(int *w, int *h) const override {
        if (!view_)
            return false;
        const auto size = backing_size(view_);
        printf("%p~~~~~~view size: %fx%f, .layer: %p, layer size: %fx%f @%f~~~~~~\n", this, view_.frame.size.width, view_.frame.size.height, view_.layer, view_.layer.bounds.size.width, view_.layer.bounds.size.height, view_.layer.contentsScale);
        if (w)
            *w = (int)size.width;
        if (h)
            *h = (int)size.height;
        return true;
    }
private:
    NSView *view_ = nil;
    NSObject *observer_ = nullptr;
};

PlatformSurface* create_cocoa_surface() { return new CocoaSurface();}
UGS_NS_END
