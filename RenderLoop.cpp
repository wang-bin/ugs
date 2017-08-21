/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#include "ugsurface/RenderLoop.h"
#include "ugsurface/PlatformSurface.h"
#include "base/BlockingQueue.h"
#include "base/Semaphore.h"
#include <cassert>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <iostream>

/*!
 * TODO: support global rendering thread and event loop instead of per surface ones
 */
UGSURFACE_NS_BEGIN
using namespace std;
class RenderLoop::Private
{
public:
    ~Private() {
        //updateNativeSurface(nullptr); // ensure not joinable
        if (render_thread.joinable()) // already not joinable by updateNativeSurface(nullptr) or show() return
            std::cerr << "rendering thread should not be joinable" << std::endl << std::flush;
        assert(!render_thread.joinable() && "rendering thread should not be joinable");
        delete psurface;
    }
    void schedule(std::function<void()> task) {
        tasks.push(std::move(task));
    }

    void run() {
        printf("%p start RenderLoop\n", this);
        while (true) {
            function<void()> task;
            tasks.pop(task);
            if (task)
                task();
            if (stop_requested && surfaces.empty())
                break;
        }
        sem.release();
    }

    bool task_model = false;
    bool use_thread = true;
    bool stop_requested = false;
    bool running = false;
    bool dirty_native_surface = false; // if current native surface is reset by updateNativeSurface(), a new render loop will start
    void* native_surface = nullptr; // just record previous surface handle
    PlatformSurface* psurface = nullptr; // always create one to unify code
    std::condition_variable cv;
    std::thread render_thread;
    Semaphore sem;
    Semaphore render_sem;

    typedef struct SurfaceProcessor {
        shared_ptr<PlatformSurface> surface;
        function<void()> use;
    } SurfaceProcessor;
    list<SurfaceProcessor*> surfaces;
    std::function<void(PlatformSurface*,int,int)> resize_cb = nullptr;
    std::function<bool(PlatformSurface*)> draw_cb = nullptr;
    std::function<void(PlatformSurface*)> close_cb = nullptr;
    BlockingQueue<function<void()>> tasks;
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
    d->stop_requested = false;
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

void RenderLoop::stop()
{
    d->stop_requested = true;
    for (auto sp : d->surfaces) {
        sp->surface->close();
    }
}

bool RenderLoop::isRunning() const
{
    return d->running;
}

void RenderLoop::update()
{
    if (!d->task_model) {
        proceedToNext();
        return;
    }
    d->schedule([=]{
        for (auto sp : d->surfaces) {
            if (sp->use)
                sp->use();
            process(sp->surface.get());
        }
    });
}

void RenderLoop::onResize(function<void(PlatformSurface*,int,int)> cb)
{
    d->resize_cb = cb;
}

void RenderLoop::onDraw(function<bool(PlatformSurface*)> cb)
{
    d->draw_cb = cb;
}

void RenderLoop::onClose(function<void(PlatformSurface*)> cb)
{
    d->close_cb = cb;
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

weak_ptr<PlatformSurface> RenderLoop::add(PlatformSurface *surface)
{
    shared_ptr<PlatformSurface> ss(surface);
    d->schedule([=]{
        function<void()> cb = nullptr;
        if (!createRenderContext(ss.get(), &cb)) { // ss will be destroyed if not pushed to list
            printf("Failed to create rendering context! platform surface handle: %p\n", surface->nativeHandle());
            return;
        }
        auto sp = new Private::SurfaceProcessor{ss, cb};
        d->surfaces.push_back(sp);
        surface->setEventCallback([=]{ // TODO: void(Event e)
            d->schedule([=]{
                if (cb)
                    cb();
                if (!process(ss.get())) {
                    d->surfaces.erase(find(d->surfaces.begin(), d->surfaces.end(), sp));
                    delete sp;
                }
            });
        });
    });
    return ss;
}

PlatformSurface* RenderLoop::process(PlatformSurface* surface)
{
    activateRenderContext(surface);

    PlatformSurface::Event e{};
    while (surface->popEvent(e, 0)) { // do no always try pop
        if (e.type == PlatformSurface::Event::Close) {
            std::cout << surface << "->PlatformSurface::Event::Close" << std::endl;
            if (d->close_cb)
                d->close_cb(surface);
            destroyRenderContext(surface);
            return nullptr;
        } else if (e.type == PlatformSurface::Event::Resize) {
            if (d->resize_cb)
                d->resize_cb(surface, e.size.width, e.size.height);
#if defined(__ANDROID__) || defined(ANDROID)
            submitRenderContext(surface);
            surface->submit();
            // workaround for android wrong display rect. also force iOS resize rbo because makeCurrent is not always called in current implementation
            // if (d->draw_cb && d->draw_cb(surface)) // for all platforms? // for iOS, render in a correct viewport before swapBuffers
#endif
        }
    }
    // FIXME: check null for ios background?
    if (d->draw_cb && d->draw_cb(surface)) { // not onDraw(surface) is ok, because context is current
        submitRenderContext(surface);
        surface->submit();
    }
    return surface;
}

void RenderLoop::run()
{
    //set_current_thread_name(UGSURFACE_STRINGIFY(UGSURFACE_NS) ".video.render");
    printf("%p start renderLoop thread\n", this);
    std::cout << "rendering thread: " << std::this_thread::get_id() << std::endl << std::flush;
    while (true) {
        // TODO: surface create task
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
        // TODO: dispatch to a certain surface, but not for (surface : ...) {}
        while (d->psurface->popEvent(e, 0)) { // do no always try pop
            if (e.type == PlatformSurface::Event::Close) {
                std::cout << "PlatformSurface::Event::Close" << std::endl << std::flush;
                // TODO: continue until dtor is called?
                goto end;
            } else if (e.type == PlatformSurface::Event::Resize) {
                onResize(e.size.width, e.size.height);
#if defined(__ANDROID__) || defined(ANDROID)
                submitRenderContext(d->psurface);
                d->psurface->submit();
                // workaround for android wrong display rect. also force iOS resize rbo because makeCurrent is not always called in current implementation
                // if (onDraw()) // for all platforms? // for iOS, render in a correct viewport before swapBuffers
#endif
            }
        }
        // FIXME: check null for ios background?
        if (onDraw()) {
            submitRenderContext(d->psurface);
            d->psurface->submit();
        }
    }
end:
    onClose();
    destroyRenderContext(d->psurface);
    d->sem.release();
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
    if (d->render_thread.joinable())
        d->render_thread.join();
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
