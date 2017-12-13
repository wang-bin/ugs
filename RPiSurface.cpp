/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#include "ugs/PlatformSurface.h"
#include <cstdio>
#include <algorithm>
#include <iostream>
#include <bcm_host.h>
#include <EGL/egl.h>

UGS_NS_BEGIN
static int getDisplayId() { // vc_dispmanx_types.h: DISPMANX_ID_MAIN_LCD 0, DISPMANX_ID_AUX_LCD 1, DISPMANX_ID_HDMI 2, DISPMANX_ID_SDTV 3, DISPMANX_ID_FORCE_LCD 4, DISPMANX_ID_FORCE_TV 5
    static int id = -1;
    if (id >= 0)
        return id;
    const char* e = std::getenv("DISPMANX_ID");
    if (e)
        id = std::atoi(e);
    id = std::min(std::max(id, DISPMANX_ID_MAIN_LCD), DISPMANX_ID_FORCE_TV);
    std::clog << "dispmanx id: " << id << std::endl;
    return id;
}

class RPiSurface final : public PlatformSurface
{
public:
    RPiSurface(): PlatformSurface() {
        bcm_host_init(); // required to create egl context
        display_ = vc_dispmanx_display_open(getDisplayId()); // vc4 returns null
        if (!display_)
            return;
        // Let the window be fullscreen to simplify geometry change logic.
        // If the background is transparent and OpenGL viewport is not fullscreen, the result looks like a normal window
        resetNativeHandle(createFullscreenWindow(display_)); // virtual onNativeHandleChanged()!!!
    }

    ~RPiSurface() {
        if (!display_)
            return;
        EGL_DISPMANX_WINDOW_T *win = static_cast<EGL_DISPMANX_WINDOW_T*>(nativeHandle());
        DISPMANX_UPDATE_HANDLE_T hUpdate = vc_dispmanx_update_start(0);
        vc_dispmanx_element_remove(hUpdate, win->element);
        vc_dispmanx_update_submit_sync(hUpdate);
        delete win;

        vc_dispmanx_display_close(display_);
        bcm_host_deinit();
    }
private:
    EGL_DISPMANX_WINDOW_T* createFullscreenWindow(DISPMANX_DISPLAY_HANDLE_T disp);

    DISPMANX_DISPLAY_HANDLE_T display_ = 0;
};

PlatformSurface* create_rpi_surface() { return new RPiSurface();}

EGL_DISPMANX_WINDOW_T* RPiSurface::createFullscreenWindow(DISPMANX_DISPLAY_HANDLE_T disp)
{
    int32_t w = 0, h = 0;
    int32_t ret = graphics_get_display_size(getDisplayId(), (uint32_t*)&w, (uint32_t*)&h);
    if (ret < 0)
        return nullptr;
    resize(w, h); // resize event
    std::clog << "creating dispmanx fullscreen window" << w << "x" << h << std::endl;
    const VC_RECT_T dst = {0, 0, w, h};
    const VC_RECT_T src = {0, 0, w << 16, h << 16};

    VC_DISPMANX_ALPHA_T alpha;
    //alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS; // fill background
    alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE; // keep the background outside gl viewport
    alpha.opacity = 0xFF;
    alpha.mask = 0;

    const int z = 1;
    const DISPMANX_RESOURCE_HANDLE_T hSrc = 0;
    const DISPMANX_UPDATE_HANDLE_T hUpdate = vc_dispmanx_update_start(0);
    const DISPMANX_ELEMENT_HANDLE_T e = vc_dispmanx_element_add(hUpdate, disp, z, &dst, hSrc, &src, DISPMANX_PROTECTION_NONE, &alpha, nullptr, DISPMANX_NO_ROTATE);
    vc_dispmanx_update_submit_sync(hUpdate);

    EGL_DISPMANX_WINDOW_T *win = new EGL_DISPMANX_WINDOW_T();
    win->element = e;
    win->width = w;
    win->height = h;
    return win;
}

UGS_NS_END
