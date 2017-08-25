/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#pragma once
#include "export.h"
#include <functional>
#include <memory>

UGSURFACE_NS_BEGIN
class PlatformSurface;
class UGSURFACE_API RenderLoop
{
public:
    RenderLoop();
    virtual ~RenderLoop();
    bool start(); // start render loop if surface is ready
    // close all surfaces, invoke onClose() callbacks, destroy gfx contexts and surfaces, then exit render loop
    void stop();
    /// MUST call stop() && waitForStopped() manually before destroying RenderLoop
    void waitForStopped();
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
    void onResize(std::function<void(PlatformSurface*, int w, int h)> cb);
    void onDraw(std::function<bool(PlatformSurface*)> cb);
    void onClose(std::function<void(PlatformSurface*)> cb); // destroy gfx resources in the callback
protected:
    virtual void* createRenderContext(PlatformSurface* surface) = 0;
    virtual bool destroyRenderContext(PlatformSurface* surface, void* ctx) = 0;
    virtual bool activateRenderContext(PlatformSurface* surface, void* ctx) = 0;
    virtual bool submitRenderContext(PlatformSurface* surface, void* ctx) = 0;
private:
    class SurfaceContext;
    // process surface events and do rendering. return input surface, or null if surface is no longer used, e.g. closed
    PlatformSurface* process(SurfaceContext* sp);
    // TODO: frame advance service abstraction(clock, vsync, user)
    class Private;
    Private* d;
};
UGSURFACE_NS_END
