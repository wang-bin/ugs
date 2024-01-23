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

    if (!is_xdg_ && shell_.wl.shell) {
        shell_.wl.surface = wl_shell_get_shell_surface(shell_.wl.shell, surface_);
        const wl_shell_surface_listener ss_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};
        wl_shell_surface_add_listener(shell_.wl.surface, &ss_listener, this);
        wl_shell_surface_set_toplevel(shell_.wl.surface);
        wl_shell_surface_set_class(shell_.wl.surface, "WaylandSurface");
        wl_display_roundtrip(display_);
        return;
    }

    {
        static const xdg_wm_base_listener listener = {
            .ping = [](void *data, xdg_wm_base *wm_base, uint32_t serial) {
                //clog << "wayland xdg ping " << serial << endl;
                xdg_wm_base_pong(wm_base, serial);
            },
        };
        xdg_wm_base_add_listener(shell_.xdg.wm, &listener, this);
    }
    shell_.xdg.surface = xdg_wm_base_get_xdg_surface(shell_.xdg.wm, surface_);
    {
        static const xdg_surface_listener listener = {
            .configure = [](void *data, xdg_surface *surface, uint32_t serial) {
                //clog << "wayland xdg_surface_listener " << serial << endl;
                xdg_surface_ack_configure(surface, serial);
            },
        };
        xdg_surface_add_listener(shell_.xdg.surface, &listener, this);
    }
    shell_.xdg.toplevel = xdg_surface_get_toplevel(shell_.xdg.surface);
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
        xdg_toplevel_add_listener(shell_.xdg.toplevel, &listener, this);
    }
    wl_display_roundtrip(display_);
}

WaylandSurface::~WaylandSurface()
{
    if (shell_.wl.surface)
        wl_shell_surface_destroy(shell_.wl.surface);
    if (shell_.xdg.toplevel)
        xdg_toplevel_destroy(shell_.xdg.toplevel);
    if (shell_.xdg.surface)
        xdg_surface_destroy(shell_.xdg.surface);
    if (shell_.xdg.wm)
        xdg_wm_base_destroy(shell_.xdg.wm);
    if (surface_)
        wl_surface_destroy(surface_);
    if (display_)
        wl_display_disconnect(display_);
}

void WaylandSurface::processEvents()
{
    wl_display_dispatch_pending(display_);
    wl_display_flush(display_);
#if 0
    while (wl_display_prepare_read(display_)) {
        wl_display_dispatch_pending(display_);
    }
    wl_display_flush(display_);
    // poll(), wl_display_read_events(display);
    wl_display_cancel_read(display_);
    wl_display_dispatch_pending(display_);
#endif
}

void WaylandSurface::registry_add_object(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version)
{
    std::clog << "WaylandSurface::registry_add_object: " << interface << " version " << version << std::endl;
    WaylandSurface* ww = static_cast<WaylandSurface*>(data);
    if (!strcmp(interface, wl_compositor_interface.name)) {//"wl_compositor"
        ww->compositor_ = static_cast<wl_compositor*>(wl_registry_bind(reg, name, &wl_compositor_interface, version));
        ww->surface_ = wl_compositor_create_surface(ww->compositor_);
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        ww->is_xdg_ = true;
        ww->shell_.xdg.wm = (xdg_wm_base*)wl_registry_bind(reg, name, &xdg_wm_base_interface, version);
    } else if (!strcmp(interface, wl_shell_interface.name)) {// "wl_shell"
        ww->is_xdg_ = false;
        ww->shell_.wl.shell = static_cast<wl_shell*>(wl_registry_bind(reg, name, &wl_shell_interface, version));
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
