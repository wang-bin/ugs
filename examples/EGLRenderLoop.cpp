/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 */
#include "EGLRenderLoop.h"
#include "ugs/PlatformSurface.h"
#include <iostream>

#define EGL_ENSURE(x, ...) EGL_RUN_CHECK(x, return __VA_ARGS__)
#define EGL_WARN(x, ...) EGL_RUN_CHECK(x)
#define EGL_RUN_CHECK(x, ...) do { \
        while (eglGetError() != EGL_SUCCESS) {} \
        (x); \
        EGLint err = eglGetError(); \
        if (err != EGL_SUCCESS) { \
            std::clog << #x " EGL ERROR (" << std::hex << err << std::dec << ") @" << __LINE__ << __FUNCTION__ << std::endl; \
            __VA_ARGS__; \
        } \
    } while(false)

class ContextEGL
{
public:
  ContextEGL(EGLNativeWindowType window, void* extra_res) {
    EGL_ENSURE(display_ = eglGetDisplay((EGLNativeDisplayType)intptr_t(extra_res)));
    int ver[2]{};
    EGL_ENSURE(eglInitialize(display_, &ver[0], &ver[1]));

    EGLint attr[] = {
      EGL_BUFFER_SIZE, 32,
      // EGL_OPENGL_ES2_BIT|EGL_OPENGL_BIT may result in EGL_BAD_ATTRIBUTE error on android. On linux the results of EGL_OPENGL_ES2_BIT may have both ES2 and OPENGL bit
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,  // EGL_DONT_CARE for surfaceless(EGL_KHR+GL_OES) in chrome
      EGL_NONE
    };
    EGLint nb_cfgs; // eglGetConfigs
    EGLBoolean ret = EGL_FALSE;
    EGL_WARN(ret = eglChooseConfig(display_, attr, &config_, 1, &nb_cfgs));
    if (ret == EGL_FALSE || nb_cfgs < 1) { // no error and return success even if pbuffer config is not found
      std::clog << "no config if surface type is set. try EGL_DONT_CARE" << std::endl;
      attr[std::extent<decltype(attr)>::value - 2] = EGL_DONT_CARE;
      EGL_ENSURE(ret = eglChooseConfig(display_, attr, &config_, 1, &nb_cfgs));
    }
    if (nb_cfgs < 1) // fallback to renderable type es2 if current is es3?
      return;
    EGL_ENSURE(surface_ = eglCreateWindowSurface(display_, config_, window, nullptr));

    EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE,
    };
    EGL_ENSURE(ctx_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, &attribs[0]));
  }

  ~ContextEGL() {
    if (display_ == EGL_NO_DISPLAY)
      return;
    EGL_WARN(eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    if (ctx_ != EGL_NO_CONTEXT)
      EGL_WARN(eglDestroyContext(display_, ctx_));
    if (surface_ != EGL_NO_SURFACE)
      EGL_WARN(eglDestroySurface(display_, surface_));
    EGL_WARN(eglReleaseThread());
    EGL_WARN(eglTerminate(display_)); // 
    display_ = EGL_NO_DISPLAY;
  }

  void swapBuffers() {
    EGL_ENSURE(eglSwapBuffers(display_, surface_));
  }

  bool makeCurrent() {
    //thread_local ContextEGL* currentCtx = nullptr;
    //if (this == currentCtx)
    //  return true;
    EGLBoolean ok = EGL_FALSE;
    EGL_ENSURE(ok = eglMakeCurrent(display_, surface_, surface_, ctx_), false);
    if (ok != EGL_TRUE)
      return false;
    //currentCtx = this;
    return true;
  }

private:
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLContext ctx_ = EGL_NO_CONTEXT;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLConfig config_ = nullptr;
};

void* EGLRenderLoop::createRenderContext(PlatformSurface* surface)
{
  if (!surface->nativeHandleForGL())
    return nullptr;
  std::clog << "createRenderContext from surface " << surface << " with extra native res " << surface->nativeResource() << std::endl;
  return new ContextEGL(reinterpret_cast<EGLNativeWindowType>(surface->nativeHandleForGL()), surface->nativeResource());
}

bool EGLRenderLoop::destroyRenderContext(PlatformSurface* surface, void* ctx)
{
  auto glctx = static_cast<ContextEGL*>(ctx);
  std::clog << "destroyRenderContext: " << glctx << std::endl;
  delete glctx;
  return true;
}

bool EGLRenderLoop::activateRenderContext(PlatformSurface* surface, void* ctx)
{
  if (!ctx)
    return false;
  return static_cast<ContextEGL*>(ctx)->makeCurrent();
}

bool EGLRenderLoop::submitRenderContext(PlatformSurface* surface, void* ctx)
{
  if (!ctx)
    return false;
  static_cast<ContextEGL*>(ctx)->swapBuffers();
  return true;
}
