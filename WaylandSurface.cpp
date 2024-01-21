/*
 * Copyright (c) 2017-2024 WangBin <wbsecg1 at gmail.com>
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

using namespace std;
UGS_NS_BEGIN
WaylandSurface::WaylandSurface() : PlatformSurface(Type::Wayland)
{
    display_ = wl_display_connect(nullptr); // TODO: set by user
    if (!display_) {
        std::cerr << "failed to connect to wayland display" << std::endl;
        return;
    }
    wl_registry* reg = wl_display_get_registry(display_);
    const wl_registry_listener reg_listener = {&registry_add_object, &registry_remove_object};
    wl_registry_add_listener(reg, &reg_listener, this);
    wl_display_roundtrip(display_); // ensure compositor is ready

    surface_ = wl_compositor_create_surface(compositor_);

    if (shell_) {
        shell_surface_ = wl_shell_get_shell_surface(shell_, surface_);
        const wl_shell_surface_listener ss_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};
        wl_shell_surface_add_listener(shell_surface_, &ss_listener, this);
        wl_shell_surface_set_toplevel(shell_surface_);
        wl_shell_surface_set_class(shell_surface_, "WaylandSurface");
        return;
    }

    {
        static const xdg_wm_base_listener listener = {
            .ping = [](void *data, xdg_wm_base *wm_base, uint32_t serial) {
                //clog << "wayland xdg ping " << serial << endl;
                xdg_wm_base_pong(wm_base, serial);
            },
        };
        xdg_wm_base_add_listener(wm_base_, &listener, this);
    }
    xdg_surface_ = xdg_wm_base_get_xdg_surface(wm_base_, surface_);
    {
        static const xdg_surface_listener listener = {
            .configure = [](void *data, xdg_surface *surface, uint32_t serial) {
                //clog << "wayland xdg_surface_listener " << serial << endl;
                xdg_surface_ack_configure(surface, serial);
            },
        };
        xdg_surface_add_listener(xdg_surface_, &listener, this);
    }
    xdg_toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
    {
        static const xdg_toplevel_listener listener = {
            .configure = [](void *data, xdg_toplevel *toplevel, int32_t width, int32_t height, wl_array *states) {
                //clog << "wayland xdg_toplevel_listener.configure " << width << "x" << height << endl;
            },
            .close = [](void *data, xdg_toplevel *toplevel) {
                //clog << "wayland xdg_toplevel_listener.close " << endl;
                auto ww = static_cast<WaylandSurface*>(data);
                ww->close();
            },
            .configure_bounds = [](void *data, xdg_toplevel *toplevel, int32_t width, int32_t height) {
                //clog << "wayland xdg_toplevel_listener.configure_bounds " << width << "x" << height << endl;
                auto ww = static_cast<WaylandSurface*>(data);
        //        wl_egl_window_resize(static_cast<wl_egl_window*>(ww->nativeHandle()), width, height, 0, 0);
                ww->resize(width, height);
            },
            .wm_capabilities = [](void *data, xdg_toplevel *toplevel, wl_array *capabilities) {
            },
        };
// listener functions can not be null!(abort listener function for opcode 3 of xdg_toplevel is NULL)
        xdg_toplevel_add_listener(xdg_toplevel_, &listener, this);
    }
}

WaylandSurface::~WaylandSurface()
{
    if (shell_surface_)
        wl_shell_surface_destroy(shell_surface_);
    if (xdg_toplevel_)
        xdg_toplevel_destroy(xdg_toplevel_);
    if (xdg_surface_)
        xdg_surface_destroy(xdg_surface_);
    if (wm_base_)
        xdg_wm_base_destroy(wm_base_);
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
    std::clog << "WaylandSurface::registry_add_object: " << interface << " version " << version << std::endl;
    WaylandSurface* ww = static_cast<WaylandSurface*>(data);
    if (!strcmp(interface, wl_compositor_interface.name)) {//"wl_compositor"
        ww->compositor_ = static_cast<wl_compositor*>(wl_registry_bind(reg, name, &wl_compositor_interface, version));
        ww->surface_ = wl_compositor_create_surface(ww->compositor_);
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        ww->wm_base_ = (xdg_wm_base*)wl_registry_bind(reg, name, &xdg_wm_base_interface, version);
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
