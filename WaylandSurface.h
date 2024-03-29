/*
 * Copyright (c) 2017-2024 WangBin <wbsecg1 at gmail.com>
 * This file is part of UGS (Universal Graphics Surface)
 * Source code: https://github.com/wang-bin/ugs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include "ugs/PlatformSurface.h"
extern "C" {
#include <wayland-client.h>
}
// wl_proxy_marshal_flags is introduced in wayland 1.21, so must generate xdg-shell.h by old wayland-scanner(1.18 on ubuntu20.04) to be more compatible with old systems(wl_proxy_marshal_constructor in wayland 1.4, ubuntu14.04)
#include "xdg-shell.h"

UGS_NS_BEGIN
class WaylandSurface : public PlatformSurface
{
public:
    WaylandSurface();
    ~WaylandSurface() override;
    void* nativeResource() const override { return display_;}
    void processEvents() override;

protected:
    wl_display *display_ = nullptr;
    wl_surface *surface_ = nullptr;

private:
    void init_wl_shell();
    void init_xdg_shell();

    static void registry_add_object(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version);
    static void registry_remove_object(void *data, struct wl_registry *reg, uint32_t name);

    static void shell_surface_ping(void *data, struct wl_shell_surface *s, uint32_t serial);
    static void shell_surface_configure(void *data, struct wl_shell_surface *s, uint32_t edges, int32_t w, int32_t h);
    static void shell_surface_popup_done(void *data, struct wl_shell_surface *s);

    wl_compositor* compositor_ = nullptr;
    wl_seat* seat_ = nullptr;

    union {
// wl_shell is deprecated
        struct {
            wl_shell* shell;
            wl_shell_surface *surface;
        } wl;
        struct {
            xdg_wm_base *wm;
            xdg_surface *surface;
            xdg_toplevel *toplevel;
        } xdg;
    } shell_{};
};
UGS_NS_END
