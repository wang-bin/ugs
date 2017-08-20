/*
 * Copyright (c) 2017 WangBin <wbsecg1 at gmail.com>
 */
#include "WaylandSurface.h"
UGSURFACE_NS_BEGIN

class WaylandEGLSurface final : public WaylandSurface
{
public:
    WaylandEGLSurface() : WaylandSurface() {
        if (!display_)
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

PlatformSurface* create_wayland_surface() { return new WaylandEGLSurface(); }
UGSURFACE_NS_END
