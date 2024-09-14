/*
 * Copyright (c) 2016-2024 WangBin <wbsecg1 at gmail.com>
 * This file is part of UGS (Universal Graphics Surface)
 * Source code: https://github.com/wang-bin/ugs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include "export.h"
#include <functional>
#include <memory>

UGS_NS_BEGIN
class PlatformSurface;
using RenderContext = void*;
class UGS_API RenderLoop
{
public:
    RenderLoop();
    virtual ~RenderLoop();
    bool start(bool stop_on_last_surface_closed = true); // start render loop if surface is ready
    // close all surfaces, invoke onClose() callbacks, destroy gfx contexts and surfaces, then exit render loop
    void stop();
    /// MUST call stop() && waitForStopped() manually before destroying RenderLoop
    void waitForStopped();  // not required for foreign surface handles, run event loop and observe resize using gui kit api
    bool isRunning() const;
    void update(); // schedule onDraw for all surfaces
    /*!
     * \brief setFrameRate
     * \param fps
     * -1: vsync
     * 0: manually update()
     * +: auto update at fps
     */
    void setFrameRate(float fps = 0);

    // takes the ownership. but surface ptr can be accessed before close. To remove surface, call surface->close()
    std::weak_ptr<PlatformSurface> add(PlatformSurface* surface);
    /// the following functions are called in rendering thread
    RenderLoop& onResize(const std::function<void(PlatformSurface*, int w, int h, RenderContext)>& cb);
    // draw callback requres RenderContext parameter for d3d11, vulkan. opengl does not need it because context is implicit
    RenderLoop& onDraw(const std::function<bool(PlatformSurface*, RenderContext)>& cb);
    // callback after context is created
    RenderLoop& onContextCreated(const std::function<void(PlatformSurface*, RenderContext)>& cb);
    // callback before destroying context. For example, happens when resetNativeHandle(nullptr)=>close()
    RenderLoop& onDestroyContext(const std::function<void(PlatformSurface*, RenderContext)>& cb); // called when gfx context on the surface is about to be destroyed. User can destroy gfx resources in the callback
    // callback before surface close after context is destroyed
    RenderLoop& onClose(const std::function<void(PlatformSurface*)>& cb); // called when surface is about to be destroyed
protected:
    virtual void* createRenderContext(PlatformSurface* surface) = 0;
    virtual bool destroyRenderContext(PlatformSurface* surface, void* ctx) = 0;
    virtual bool activateRenderContext(PlatformSurface* surface, void* ctx) = 0;
    // changes: 1 if new format wanted, will invoke resize callback
    virtual bool submitRenderContext(PlatformSurface* surface, void* ctx, int* changes = nullptr) = 0;
private:
    class SurfaceContext;
    // process surface events and do rendering. return input surface, or null if surface is no longer used, e.g. closed
    PlatformSurface* process(SurfaceContext* sp);
    // TODO: frame advance service abstraction(clock, vsync, user)
    class Private;
    Private* d;
};
UGS_NS_END
