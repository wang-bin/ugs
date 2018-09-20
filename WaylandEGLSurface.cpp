/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * Universal Graphics Surface
 * Source code: https://github.com/wang-bin/ugs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "WaylandSurface.h"
#include <dlfcn.h>
#include <wayland-egl-core.h>

_Pragma("weak wl_egl_window_create")
_Pragma("weak wl_egl_window_destroy")
_Pragma("weak wl_egl_window_resize")

UGS_NS_BEGIN

class WaylandEGLSurface final : public WaylandSurface
{
public:
    WaylandEGLSurface() : WaylandSurface() {
        if (!display_)
            return;
        if (!wl_egl_window_create) {
            dso_ = dlopen("libwayland-egl.so.1", RTLD_LAZY|RTLD_GLOBAL);
            if (!dso_ || !wl_egl_window_create)
                return;
        }
        wl_egl_window* eglwin = wl_egl_window_create(surface_, w_, h_);
        resetNativeHandle(reinterpret_cast<void*>(eglwin));
    }
    ~WaylandEGLSurface() {
        wl_egl_window *win = static_cast<wl_egl_window*>(nativeHandle());
        if (win)
            wl_egl_window_destroy(win);
        if (dso_)
            dlclose(dso_);
    }

    void resize(int w, int h) override {
        wl_egl_window_resize(static_cast<wl_egl_window*>(nativeHandle()), w, h, 0, 0);
        WaylandSurface::resize(w, h);
    }
private:
    int w_ = 1820, h_ = 1080;
    void* dso_ = nullptr;
};

PlatformSurface* create_wayland_surface(void*) { return new WaylandEGLSurface(); }
UGS_NS_END
