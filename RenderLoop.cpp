/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#include "ugsurface/RenderLoop.h"
#include "ugsurface/PlatformSurface.h"
#include "base/Semaphore.h"
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>

/*!
 * TODO: support global rendering thread and event loop instead of per surface ones
 */
UGSURFACE_NS_BEGIN
class RenderLoop::Private
{
public:
    ~Private() {
        //updateNativeSurface(nullptr); // ensure not joinable
        if (render_thread.joinable())
            std::cerr << "rendering thread should not be joinable" << std::endl << std::flush;
        assert(!render_thread.joinable() && "rendering thread should not be joinable");
        delete psurface;
    }

    bool use_thread = true;
    bool running = false;
    bool dirty_native_surface = false; // if current native surface is reset by updateNativeSurface(), a new render loop will start
    void* native_surface = nullptr; // just record previous surface handle
    PlatformSurface* psurface = nullptr; // always create one to unify code
    std::condition_variable cv;
    std::thread render_thread;
    Semaphore sem;
    Semaphore render_sem;
};

RenderLoop::RenderLoop(int x, int y, int w, int h)
    : d(new Private())
{
    d->psurface = PlatformSurface::create(); // create surface in ui thread, not rendering thread
    assert(d->psurface);
    d->psurface->setEventCallback([this]{
        proceedToNext();
    });
    if (w > 0 && h > 0) // use default size to avoid manually resize
        resize(w, h); // after setEventCallback()
    if (!d->psurface->nativeHandle() || !d->use_thread)
        return;
    d->dirty_native_surface = true;
}

RenderLoop::~RenderLoop()
{
    delete d;
}

bool RenderLoop::start()
{
    // TODO: start even if nativeHandle is null
    if (!d->psurface->nativeHandle())
        return false;
    if (d->running)
        return true;
    d->running = true;
    if (d->use_thread) { // check joinable?
        d->render_thread = std::thread([this]{
            run();
        });
    } else {
        run();
    }
    return true;
}

bool RenderLoop::isRunning() const
{
    return d->running;
}

void RenderLoop::update()
{
    proceedToNext();
}

void RenderLoop::updateNativeSurface(void *handle)
{
    printf("%p->updateNativeSurface: %p=>%p\n", this, d->native_surface, handle);
    if (d->native_surface == handle) {
        // TODO: resize
        return;
    }

    // ensure onClose() is called before dtor
    if (d->native_surface) // render thread not started, close will quit render thread immediately when started
        d->psurface->close();
    if (d->render_thread.joinable())
        d->render_thread.join();
    d->running = false;
    // exit old thread before resetting native handle to ensure new events are posted to new thread
    d->psurface->resetNativeHandle(handle);
    d->dirty_native_surface = true;
    d->native_surface = handle;
}

void RenderLoop::run()
{
    //set_current_thread_name(UGSURFACE_STRINGIFY(UGSURFACE_NS) ".video.render");
    printf("%p start renderLoop thread\n", this);
    std::cout << "rendering thread: " << std::this_thread::get_id() << std::endl << std::flush;
    while (true) {
        if (d->dirty_native_surface) {// TODO: for(auto surface: dirty_surface) {...}
            d->dirty_native_surface = false; // FIXME: not safe. atomic<int>?
            if (!createRenderContext(d->psurface)) {
                printf("Failed to create rendering context! platform surface handle: %p\n", d->psurface->nativeHandle());
                continue;
            }
        }
        waitForNext();
        activateRenderContext(d->psurface);

        PlatformSurface::Event e{};
        // TODO: dispatch to a certain surface
        while (d->psurface->popEvent(e, 0)) { // do no always try pop
            if (e.type == PlatformSurface::Event::Close) {
                std::cout << "PlatformSurface::Event::Close" << std::endl << std::flush;
                onClose();
                d->sem.release();
                // TODO: continue until dtor is called?
                goto end;
            } else if (e.type == PlatformSurface::Event::Resize) {
                onResize(e.size.width, e.size.height);
#if defined(__ANDROID__) || defined(ANDROID)
                submitRenderContext(d->psurface);
                    // workaround for android wrong display rect. also force iOS resize rbo because makeCurrent is not always called in current implementation
                    // if (onDraw()) // for all platforms? // for iOS, render in a correct viewport before swapBuffers
#endif
            }
        }
        // FIXME: check null for ios background?
        if (onDraw())
            submitRenderContext(d->psurface);
    }
end:
    //onClose();
    destroyRenderContext(d->psurface);
    //d->sem.release();
}

void RenderLoop::resize(int w, int h)
{
    d->psurface->resize(w, h); // w, h < 0: platform surface check the size
}

void RenderLoop::show()
{
    if (!d->use_thread) {
        run();
        return;
    }
    while (!d->sem.tryAcquire(1, 10)) {
        d->psurface->processEvents();
    }
}

void RenderLoop::waitForNext()
{
    d->render_sem.acquire(1);
}

void RenderLoop::proceedToNext()
{
    d->render_sem.release(1);
}

UGSURFACE_NS_END
