/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#pragma once
#include "export.h"

UGSURFACE_NS_BEGIN
class PlatformSurface;
class UGSURFACE_API RenderLoop // TODO: move to core.renderloop
{
public:
    /*!
     * \brief RenderLoop
     * Create a platform window and start rendering thread internally. Rendering context will be automatically created from platform window
     * For some platforms like android, real window is created by app and platform window is just a dummy window,, x, y, w, h and title are unused.
     * You must call updateNativeWindow to active rendering context for such platforms.
     */
    RenderLoop(int x = 0, int y = 0, int w = 0, int h = 0);
    virtual ~RenderLoop();
    // must call updateNativeWindow(nullptr) before dtor if show() is not required, e.g. android
    // TODO: addNativeWindow(...)
    void updateNativeWindow(void* handle); // recreate window if native window handle changes. otherwise check geometry change
    void update(); // schedule onDraw
    /*!
     * \brief setFrameRate
     * \param fps
     * -1: vsync
     * 0: manually update()
     * +: auto update at fps
     */
    void setFrameRate(float fps = 0);
    void resize(int w, int h); // TODO: setGeometry. resize(PlatformSurface, ...)
    void show();
protected:
    virtual bool createRenderContext(PlatformSurface* surface) = 0;
    virtual bool destroyRenderContext(PlatformSurface* surface) = 0;
    virtual bool activeRenderContext(PlatformSurface* surface) = 0;
    virtual bool submitRenderContext(PlatformSurface* surface) = 0;
    /// the following functions are called on rendering thread
    virtual void onClose() {} // TODO: onClose(function), param: PlatformSurface ...
    /*!
     * \brief onResize
     */
    virtual void onResize(int w, int h) {} // TODO: onResize(function), param: PlatformSurface ...
    /*!
     * \brief onDraw
     * Do your opengl rendering here
     * \return true if frame is rendered
     */
    virtual bool onDraw() { return false;} // TODO: onDraw(function), param: PlatformSurface ...
private:
    void run();
    // TODO: frame advance service abstraction(clock, vsync, user)
    void waitForNext();
    void proceedToNext();
    class Private;
    Private* d;
};

UGSURFACE_NS_END
