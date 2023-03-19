/* input: surface object. ANativeWindow can be variant for a surface, but ANW func ptrs are the same,
 they are hook_XXX() with the same Surface parameter
 */
/*
 * Copyright (c) 2017-2023 WangBin <wbsecg1 at gmail.com>
 */
#include "ugs/PlatformSurface.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include "jmi/jmi.h"

UGS_NS_BEGIN
void* javaVM(void* vm)
{
    return jmi::javaVM(static_cast<JavaVM*>(vm));
}

class AndroidSurface final : public PlatformSurface
{
public:
    AndroidSurface() : PlatformSurface() {
        setNativeHandleChangeCallback([this](void* old){
            jobject s = static_cast<jobject>(nativeHandle());
            if (old != s) { // updateNativeWindow(nullptr) called outside or resetNativeHandle(nullptr) in dtor
                if (anw_)
                    ANativeWindow_release(anw_);
            }
            anw_ = nullptr;
            if (!s)
                return;
            anw_ = ANativeWindow_fromSurface(jmi::getEnv(), s);
        });
    }
    ~AndroidSurface() override {
        if (anw_)
            ANativeWindow_release(anw_);
        anw_ = nullptr;
    }

    void* nativeHandleForVulkan() const override {
        return anw_;
    }

    void* nativeHandleForGL() const override {
        return anw_;
    }

    bool size(int* w, int *h) const override {
        if (!anw_)
            return false;
        if (w)
            *w = ANativeWindow_getWidth(anw_);
        if (h)
            *h = ANativeWindow_getHeight(anw_);
        return true;
    }
    void resize(int w, int h) override {
        if (!anw_)
            return;
        const int w0 = ANativeWindow_getWidth(anw_);
        const int h0 = ANativeWindow_getHeight(anw_);
        if (w <= 0 || h <= 0) {
            w = w0;
            h = h0;
        }
        // surfacetexture: 1x1
        PlatformSurface::resize(w, h);
        if (w0 == w && h0 == h)
            return;
        ANativeWindow_setBuffersGeometry(anw_, w, h, WINDOW_FORMAT_RGBA_8888);
    }
private:
    ANativeWindow* anw_ = nullptr;
};

PlatformSurface* create_android_surface() { return new AndroidSurface();}
UGS_NS_END
