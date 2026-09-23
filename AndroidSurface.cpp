/* input: surface object. ANativeWindow can be variant for a surface, but ANW func ptrs are the same,
 they are hook_XXX() with the same Surface parameter
 */
/*
 * Copyright (c) 2017-2026 WangBin <wbsecg1 at gmail.com>
 * AI participated
 */
#include "ugs/PlatformSurface.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include "jmi/jmi.h"
#include <memory>
#include <mutex>

UGS_NS_BEGIN
using namespace std;
void* javaVM(void* vm)
{
    return jmi::javaVM(static_cast<JavaVM*>(vm));
}

class AndroidSurface final : public PlatformSurface
{
public:
    AndroidSurface() : PlatformSurface() {
        setNativeHandleChangeCallback([this](void* old){
            const auto s = static_cast<jobject>(nativeHandle());
            if (old != s) { // updateNativeWindow(nullptr) called outside or resetNativeHandle(nullptr) in dtor
                shared_ptr<ANativeWindow> next;
                if (s) {
                    if (auto anw = ANativeWindow_fromSurface(jmi::getEnv(), s))
                        next = {anw, ANativeWindow_release};
                }
                const scoped_lock lock(mtx_);
                anw_.swap(next);
            }
        });
    }

    void* nativeHandleForVulkan() const override {
        const scoped_lock lock(mtx_);
        return anw_.get();
    }

    void* nativeHandleForGL() const override {
        const scoped_lock lock(mtx_);
        return anw_.get();
    }

    shared_ptr<void> retainNativeHandleForVulkan() const override {
        return window();
    }

    shared_ptr<void> retainNativeHandleForGL() const override {
        return window();
    }

    bool size(int* w, int *h) const override {
        const auto anw = window();
        if (!anw)
            return false;
        if (w)
            *w = ANativeWindow_getWidth(anw.get());
        if (h)
            *h = ANativeWindow_getHeight(anw.get());
        return true;
    }
    void resize(int w, int h) override {
        const auto anw = window();
        if (!anw)
            return;
        const int w0 = ANativeWindow_getWidth(anw.get());
        const int h0 = ANativeWindow_getHeight(anw.get());
        if (w <= 0 || h <= 0) {
            w = w0;
            h = h0;
        }
        // surfacetexture: 1x1
        PlatformSurface::resize(w, h);
        if (w0 == w && h0 == h)
            return;
        const auto fmt = ANativeWindow_getFormat(anw.get());
// no error but format won't change, only surface holder can set. default is rgb565
        ANativeWindow_setBuffersGeometry(anw.get(), w, h, fmt); // prevent tearing
    }
private:
    shared_ptr<ANativeWindow> window() const {
        const scoped_lock lock(mtx_);
        return anw_;
    }

    mutable mutex mtx_;
    shared_ptr<ANativeWindow> anw_;
};

PlatformSurface* create_android_surface() { return new AndroidSurface();}
UGS_NS_END
