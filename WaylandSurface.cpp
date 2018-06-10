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
#include <cassert>
#include <cstring>
#include <iostream>

// symbols does not exist on raspian 7
_Pragma("weak wl_proxy_marshal_constructor")
_Pragma("weak wl_proxy_marshal_constructor_versioned")

UGS_NS_BEGIN
WaylandSurface::WaylandSurface() : PlatformSurface()
{
    display_ = wl_display_connect(nullptr);
    if (!display_) {
        std::cerr << "failed to connect to wayland display" << std::endl;
        return;
    }
    wl_registry* reg = wl_display_get_registry(display_);
    const wl_registry_listener reg_listener = {&registry_add_object, &registry_remove_object};
    wl_registry_add_listener(reg, &reg_listener, this);
    wl_display_roundtrip(display_); // ensure compositor is ready

    surface_ = wl_compositor_create_surface(compositor_);
    shell_surface_ = wl_shell_get_shell_surface(shell_, surface_);

    const wl_shell_surface_listener ss_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};
    wl_shell_surface_add_listener(shell_surface_, &ss_listener, this);
    wl_shell_surface_set_toplevel(shell_surface_);
    wl_shell_surface_set_class(shell_surface_, "WaylandSurface");
}

WaylandSurface::~WaylandSurface()
{
    if (shell_surface_)
        wl_shell_surface_destroy(shell_surface_);
    if (surface_)
        wl_surface_destroy(surface_);
    if (display_)
        wl_display_disconnect(display_);
}

void WaylandSurface::processEvents()
{
    //wl_display_dispatch_pending(display_);
    //wl_display_flush(display_);
}

void WaylandSurface::registry_add_object(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version)
{
    printf("WaylandSurface::registry_add_object: %s\n", interface);
    WaylandSurface* ww = static_cast<WaylandSurface*>(data);
    if (!strcmp(interface, wl_compositor_interface.name)) {//"wl_compositor"
        ww->compositor_ = static_cast<wl_compositor*>(wl_registry_bind(reg, name, &wl_compositor_interface, version));
        ww->surface_ = wl_compositor_create_surface(ww->compositor_);
    } else if (!strcmp(interface, wl_shell_interface.name)) {// "wl_shell"
        ww->shell_ = static_cast<wl_shell*>(wl_registry_bind(reg, name, &wl_shell_interface, version));
    }
}

void WaylandSurface::registry_remove_object(void *data, struct wl_registry *reg, uint32_t name)
{
}

void WaylandSurface::shell_surface_ping(void *data, struct wl_shell_surface *shell_surface, uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
}

void WaylandSurface::shell_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height)
{
    WaylandSurface* ww = static_cast<WaylandSurface*>(data);
//        wl_egl_window_resize(static_cast<wl_egl_window*>(ww->nativeHandle()), width, height, 0, 0);
    ww->resize(width, height);
}

void WaylandSurface::shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface)
{    
}
UGS_NS_END
