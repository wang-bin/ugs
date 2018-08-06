/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 * This file is part of UGS (Universal Graphics Surface)
 * Source code: https://github.com/wang-bin/ugs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "ugs/PlatformSurface.h"
extern "C" {
#include <wayland-client.h>
#include <wayland-egl.h>
}

UGS_NS_BEGIN
class WaylandSurface : public PlatformSurface
{
public:
    WaylandSurface();
    ~WaylandSurface() override;
    void* nativeResource() const override { return display_;}
    void processEvents() override;

protected:
    static void registry_add_object(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version);
    static void registry_remove_object(void *data, struct wl_registry *reg, uint32_t name);

    static void shell_surface_ping(void *data, struct wl_shell_surface *s, uint32_t serial);
    static void shell_surface_configure(void *data, struct wl_shell_surface *s, uint32_t edges, int32_t w, int32_t h);
    static void shell_surface_popup_done(void *data, struct wl_shell_surface *s);

    wl_compositor* compositor_ = nullptr;
    wl_shell* shell_ = nullptr;
    wl_display *display_ = nullptr;
    wl_surface *surface_ = nullptr;
    wl_shell_surface *shell_surface_ = nullptr;
};
UGS_NS_END
