/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#include "ugsurface/RenderLoop.h"
#include "ugsurface/PlatformSurface.h"
#include "base/BlockingQueue.h"
#include <cassert>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <iostream>

UGSURFACE_NS_BEGIN
using namespace std;

class RenderLoop::SurfaceContext {
public:
    shared_ptr<PlatformSurface> surface;
    void* ctx;
};
class RenderLoop::Private
{
public:
    ~Private() {
        //updateNativeSurface(nullptr); // ensure not joinable
        if (render_thread.joinable()) {// already not joinable by updateNativeSurface(nullptr) or show() return
            std::cerr << "rendering thread should not be joinable" << std::endl << std::flush;
            render_thread.join();
        }
        assert(!render_thread.joinable() && "rendering thread should not be joinable");
    }
    void schedule(std::function<void()> task) {
        tasks.push(std::move(task));
    }

    void run() {
        running = true;
        printf("%p start RenderLoop\n", this);
        while (true) {
            function<void()> task;
            tasks.pop(task);
            if (task)
                task();
            if (stop_requested && surfaces.empty())
                break;
        }
        running = false;
    }

    bool use_thread = true;
    bool stop_requested = false;
    bool running = false;
    mutex mtx;
    condition_variable cv;
    thread render_thread;

    list<RenderLoop::SurfaceContext*> surfaces;
    function<void(PlatformSurface*,int,int)> resize_cb = nullptr;
    function<bool(PlatformSurface*)> draw_cb = nullptr;
    function<void(PlatformSurface*)> close_cb = nullptr;
    BlockingQueue<function<void()>> tasks;
};

RenderLoop::RenderLoop()
    : d(new Private())
{
}

RenderLoop::~RenderLoop()
{
    delete d;
}

bool RenderLoop::start()
{
    if (d->running)
        return true;
    d->running = true;
    if (d->use_thread) { // check joinable?
        d->render_thread = std::thread([this]{
            d->run();
            notify_all_at_thread_exit(d->cv, unique_lock<mutex>(d->mtx));
        });
    }
    return true;
}

void RenderLoop::stop()
{
    if (!isRunning())
        return;
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
    d->schedule([=]{
        for (auto sp : d->surfaces) {
            process(sp);
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

weak_ptr<PlatformSurface> RenderLoop::add(PlatformSurface *surface)
{
    shared_ptr<PlatformSurface> ss(surface);
    auto sp = new SurfaceContext{ss, nullptr};
    d->surfaces.push_back(sp);
    d->schedule([=]{
        if (!process(sp)) { // create=>resize=>close event in 1 process()
            cout << "deleting surface scheduled by surface add callback..." << endl;
            auto it = find(d->surfaces.begin(), d->surfaces.end(), sp);
            d->surfaces.erase(it);
            delete *it;
        }
    });
    return ss;
}

PlatformSurface* RenderLoop::process(SurfaceContext *sp)
{
    PlatformSurface* surface = sp->surface.get();
    void* ctx = sp->ctx;
    activateRenderContext(surface, ctx);
    PlatformSurface::Event e{};
    while (surface->popEvent(e, 0)) { // do no always try pop
        if (e.type == PlatformSurface::Event::Close) {
            std::cout << surface << "->PlatformSurface::Event::Close" << std::endl;
            if (d->close_cb)
                d->close_cb(surface);
            destroyRenderContext(surface, ctx);
            return nullptr; // FIXME
        } else if (e.type == PlatformSurface::Event::Resize) {
            std::cout << "PlatformSurface::Event::Resize" << std::endl;
            if (d->resize_cb)
                d->resize_cb(surface, e.size.width, e.size.height);
#if defined(__ANDROID__) || defined(ANDROID)
            submitRenderContext(surface, ctx);
            surface->submit();
            // workaround for android wrong display rect. also force iOS resize rbo because makeCurrent is not always called in current implementation
            // if (d->draw_cb && d->draw_cb(surface)) // for all platforms? // for iOS, render in a correct viewport before swapBuffers
#endif
        } else if (e.type == PlatformSurface::Event::NativeHandle) {
            std::cout << surface << "->PlatformSurface::Event::NativeHandle: " << e.handle.before << ">>>" << e.handle.after << std::endl;
            if (e.handle.before)
                destroyRenderContext(surface, ctx);
            sp->ctx = nullptr;
            if (!e.handle.after)
                return surface;
            ctx = createRenderContext(surface);
            if (!ctx) { // ss will be destroyed if not pushed to list
                printf("Failed to create rendering context! platform surface handle: %p\n", surface->nativeHandle());
                return surface; // FIXME
            }
            sp->ctx = ctx;
            activateRenderContext(surface, ctx);
            surface->setEventCallback([=]{ // TODO: void(Event e)
                d->schedule([=]{
                    if (!process(sp)) {
                        cout << "deleting surface scheduled by event callback..." << endl;
                        auto it = find(d->surfaces.begin(), d->surfaces.end(), sp);
                        d->surfaces.erase(it);
                        delete *it;
                    }
                });
            });
        }
    }
    if (!ctx) {
        std::cout << "no gl context. skip rendering..." << std::endl;
        return surface;
    }
    // FIXME: check null for ios background?
    if (d->draw_cb && d->draw_cb(surface)) { // not onDraw(surface) is ok, because context is current
        submitRenderContext(surface, ctx);
        surface->submit();
    }
    return surface;
}

void RenderLoop::waitForStopped()
{
    if (!d->use_thread) {
        d->run();
        return;
    }
    while (d->running) {
        for (auto sp : d->surfaces) {
            sp->surface->processEvents();
        }
        unique_lock<mutex> lock(d->mtx);
        if (!d->running)
            break;
        d->cv.wait_for(lock, chrono::milliseconds(10));
    }
    if (d->render_thread.joinable())
        d->render_thread.join();
}
UGSURFACE_NS_END
