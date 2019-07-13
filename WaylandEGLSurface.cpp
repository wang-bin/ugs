/*
 * Copyright (c) 2017-2019 WangBin <wbsecg1 at gmail.com>
 * Universal Graphics Surface
 * Source code: https://github.com/wang-bin/ugs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "WaylandSurface.h"
#include <dlfcn.h>
//#include <wayland-egl-core.h>
#include <cassert>
#include <iostream>

UGS_NS_BEGIN
// how to get current function result type easily?
#define DEFINE_T_V(R, NAME, ARG_T, ARG_T_V, ARG_V) \
    R NAME ARG_T_V { \
        static auto fp = (decltype(&NAME))dlsym(load_once(), #NAME); \
        assert(fp && "SYMBOL NOT FOUND: " #NAME); \
        return fp ARG_V; \
    }

#define CAPI_ARG0() (), (), ()
#define CAPI_ARG1(P1) (P1), (P1 p1), (p1)
#define CAPI_ARG2(P1, P2) (P1, P2), (P1 p1, P2 p2), (p1, p2)
#define CAPI_ARG3(P1, P2, P3) (P1, P2, P3), (P1 p1, P2 p2, P3 p3), (p1, p2, p3)
#define CAPI_ARG4(P1, P2, P3, P4) (P1, P2, P3, P4), (P1 p1, P2 p2, P3 p3, P4 p4), (p1, p2, p3, p4)
#define CAPI_ARG5(P1, P2, P3, P4, P5) (P1, P2, P3, P4, P5), (P1 p1, P2 p2, P3 p3, P4 p4, P5 p5), (p1, p2, p3, p4, p5)

#define CAPI_EXPAND(expr) expr
#define CAPI_DEFINE(R, name, ...) CAPI_EXPAND(DEFINE_T_V(R, name, __VA_ARGS__)) /* not ##__VA_ARGS__ !*/

static auto load_once()->decltype(dlopen(nullptr, RTLD_LAZY))
{
    static auto dll = dlopen("libwayland-egl.so.1", RTLD_LAZY|RTLD_LOCAL);
    if (!dll)
        std::clog << "failed to load libwayland-egl.so.1" << std::endl;
    return dll;
}

#ifndef WAYLAND_EGL_CORE_H
struct wl_egl_window;
CAPI_DEFINE(wl_egl_window*, wl_egl_window_create, CAPI_ARG3(wl_surface*, int, int));
CAPI_DEFINE(void, wl_egl_window_destroy, CAPI_ARG1(wl_egl_window*));
CAPI_DEFINE(void, wl_egl_window_resize, CAPI_ARG5(wl_egl_window*, int, int, int, int));
#endif

class WaylandEGLSurface final : public WaylandSurface
{
public:
    WaylandEGLSurface() : WaylandSurface() {
        if (!display_)
            return;
        if (!load_once())
            return;
        wl_egl_window* eglwin = wl_egl_window_create(surface_, w_, h_);
        resetNativeHandle(reinterpret_cast<void*>(eglwin));
    }
    ~WaylandEGLSurface() {
        wl_egl_window *win = static_cast<wl_egl_window*>(nativeHandle());
        if (win)
            wl_egl_window_destroy(win);
    }

    void resize(int w, int h) override {
        wl_egl_window_resize(static_cast<wl_egl_window*>(nativeHandle()), w, h, 0, 0);
        WaylandSurface::resize(w, h);
    }
private:
    int w_ = 1820, h_ = 1080;
};

PlatformSurface* create_wayland_surface(void*) { return new WaylandEGLSurface(); }
UGS_NS_END
