/*
 * Copyright (c) 2017 WangBin <wbsecg1 at gmail.com>
 */
#include "ugsurface/PlatformSurface.h"
#import <UIKit/UIView.h>
#include <iostream>

@interface PropertyObserver : NSObject
- (void) observeGeometryChange:(std::function<void(float,float)>)cb; //(void(^)())
@end

@implementation PropertyObserver {
    std::function<void(float,float)> update_cb_;
}

- (void) observeGeometryChange:(std::function<void(float,float)>)cb
{
    update_cb_ = cb;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (!update_cb_)
        return;
    if([keyPath isEqualToString:@"bounds"] || [keyPath isEqualToString:@"frame"]) {
        CGRect rect0, rect;
        if([change objectForKey:@"old"] != [NSNull null])
            rect0 = [[change objectForKey:@"old"] CGRectValue];
        if([object valueForKeyPath:keyPath] != [NSNull null])
            rect = [[object valueForKeyPath:keyPath] CGRectValue];
        const NSString *msg = [NSString stringWithFormat:@"%@ geometry changes from %fx%f to %fx%f", object, rect0.size.width, rect0.size.height, rect.size.width, rect.size.height];
        std::cout << msg.UTF8String << std::endl;
        update_cb_(rect.size.width, rect.size.height);
    }
}
@end

UGSURFACE_NS_BEGIN
class UIKitSurface final: public PlatformSurface
{
public:
    UIKitSurface() : PlatformSurface() {
        setNativeHandleChangeCallback([this](void* old){
            auto getLayer = [](NSObject* obj){
                if ([obj isKindOfClass:[CALayer class]])
                    return (CALayer*)obj;
                UIView *view = (UIView*)obj;
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
            resize(layer_.bounds.size.width, layer_.bounds.size.height); // post resize event
        });
        observer_ = [PropertyObserver alloc];
        [observer_ observeGeometryChange:[this](float w, float h){ resize(w, h);}];
    }
    ~UIKitSurface() {
        resetNativeHandle(nullptr);
#if !__has_feature(objc_arc)
        if (observer_)
            [observer_ release];
#endif
    }
private:
    PropertyObserver *observer_ = nil;
    CALayer *layer_ = nil;
};

PlatformSurface* create_ios_surface() { return new UIKitSurface();}
UGSURFACE_NS_END
