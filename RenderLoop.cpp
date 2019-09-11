/*
 * Copyright (c) 2016-2019 WangBin <wbsecg1 at gmail.com>
 * This file is part of UGS (Universal Graphics Surface)
 * Source code: https://github.com/wang-bin/ugs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "ugs/RenderLoop.h"
#include "ugs/PlatformSurface.h"
#include "base/BlockingQueue.h"
#include <cassert>
#include <list>
#include <mutex>
#include <thread>
#include <iostream>

//// TODO: frame rendering can be driven by frame push, master clock, vsync, subtitle(hi fps sub, but must < vsync fps), any of above conditions triggered, post update event in render loop
UGS_NS_BEGIN
using namespace std;

class RenderLoop::SurfaceContext {
public:
    shared_ptr<PlatformSurface> surface;
    RenderContext ctx;
};
class RenderLoop::Private
{
public:
    ~Private() {
        assert(!render_thread.joinable() && "rendering thread can not be joinable in dtor. MUST call stop() & waitForStopped() first");
    }
    void schedule(std::function<void()>&& task) {
        tasks.push(std::move(task));
    }

    void run() {
        running = true;
        std::clog << this << " start RenderLoop" << std::endl;
        while (true) { // TODO: lock free? semaphore?
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
    bool stop_on_last_close = true;
    mutex mtx;
    thread render_thread;

    list<RenderLoop::SurfaceContext*> surfaces;
    function<void(PlatformSurface*,int,int)> resize_cb = nullptr;
    function<bool(PlatformSurface*, RenderContext)> draw_cb = nullptr;
    function<void(PlatformSurface*)> close_cb = nullptr;
    function<void(PlatformSurface*, RenderContext)> ctx_created_cb = nullptr;
    function<void(PlatformSurface*, RenderContext)> ctx_destroy_cb = nullptr;
    BlockingQueue<function<void()>> tasks; // TODO: unbounded_blocking_fifo w/ or w/o semaphore (depending on on draw call cost(profile))
};

RenderLoop::RenderLoop()
    : d(new Private())
{
}

RenderLoop::~RenderLoop()
{
    // can not stop and waitForStopped() in dtor to avoid calling virtual functions. user MUST manually cal them
    delete d;
}

bool RenderLoop::start(bool stop_on_last_surface_closed)
{
    if (d->running)
        return true;
    d->running = true;
    d->stop_on_last_close = stop_on_last_surface_closed;
    if (d->use_thread) { // check joinable?
        d->render_thread = std::thread([this]{
            std::clog << "Rendering thread @" << std::this_thread::get_id() << std::endl;
            d->run();
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

void RenderLoop::waitForStopped()
{
    if (!d->use_thread) {
        d->run();
        return;
    }
    while (d->running) {
        {
            unique_lock<mutex> lock(d->mtx);
            // surfaces.erase() on close.
            // TODO: lock free, schedule a task so in the same thread as ease(still has mutext in task queue)? main thread?
            // cow?
            for (auto sp : d->surfaces) {
                sp->surface->processEvents(); // MPSC
            }
        }
        this_thread::sleep_for(chrono::milliseconds(10)); // TODO: poll native events?
    }
    if (d->render_thread.joinable())
        d->render_thread.join();
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

RenderLoop& RenderLoop::onResize(function<void(PlatformSurface*,int,int)> cb)
{
    d->resize_cb = cb;
    return *this;
}

RenderLoop& RenderLoop::onDraw(function<bool(PlatformSurface*, RenderContext)> cb)
{
    d->draw_cb = cb;
    return *this;
}

RenderLoop& RenderLoop::onContextCreated(function<void(PlatformSurface*,void*)> cb)
{
    d->ctx_created_cb = cb;
    return *this;
}

RenderLoop& RenderLoop::onDestroyContext(function<void(PlatformSurface*,void*)> cb)
{
    d->ctx_destroy_cb = cb;
    return *this;
}

RenderLoop& RenderLoop::onClose(function<void(PlatformSurface*)> cb)
{
    d->close_cb = cb;
    return *this;
}

weak_ptr<PlatformSurface> RenderLoop::add(PlatformSurface *surface)
{
    shared_ptr<PlatformSurface> ss(surface);
    auto sp = new SurfaceContext{ss, nullptr};
    d->surfaces.push_back(sp);
    surface->setEventCallback([=]{ // TODO: void(Event e)
        d->schedule([=]{
            if (!process(sp)) {
                clog << "surface removed by event callback..." << endl;
            }
        });
    });
    d->schedule([=]{
        if (!process(sp)) { // create=>resize=>close event in 1 process()
            clog << "deleting surface scheduled by surface add callback..." << endl;
            auto it = find(d->surfaces.begin(), d->surfaces.end(), sp);
            delete *it;
            d->surfaces.erase(it); // gcc4.8 can not erase a const_iterator
        }
    });
    return ss;
}

PlatformSurface* RenderLoop::process(SurfaceContext *sp)
{
    PlatformSurface* surface = sp->surface.get();
    void* ctx = sp->ctx;
    if (!surface->acquire())
        return surface;
    activateRenderContext(surface, ctx);
    PlatformSurface::Event e{};
    while (surface->popEvent(e)) {
        if (e.type == PlatformSurface::Event::Close) {
            std::clog << surface << "->PlatformSurface::Event::Close" << std::endl;
            if (ctx) {
                if (d->ctx_destroy_cb)
                    d->ctx_destroy_cb(surface, ctx);
                destroyRenderContext(surface, ctx);
            }
            if (d->close_cb)
                d->close_cb(surface);
            surface->release();
            auto it = find(d->surfaces.begin(), d->surfaces.end(), sp);
            unique_lock<mutex> lock(d->mtx);
            clog << "removing closed surface..." << endl;
            d->surfaces.erase(it);
            delete sp;
            if (d->stop_on_last_close && d->surfaces.empty())
                d->stop_requested = true;
            return nullptr; // FIXME
        } else if (e.type == PlatformSurface::Event::Resize) {
            std::clog << "PlatformSurface::Event::Resize" << std::endl;
            if (d->resize_cb)
                d->resize_cb(surface, e.size.width, e.size.height);
#if defined(__ANDROID__) || defined(ANDROID)
            submitRenderContext(surface, ctx);
            surface->submit();
            // workaround for android wrong display rect. also force iOS resize rbo because makeCurrent is not always called in current implementation
            // if (d->draw_cb && d->draw_cb(surface, ctx)) // for all platforms? // for iOS, render in a correct viewport before swapBuffers
#endif
        } else if (e.type == PlatformSurface::Event::NativeHandle) {
            std::clog << surface << "->PlatformSurface::Event::NativeHandle: " << e.handle.before << ">>>" << e.handle.after << std::endl;
            if (e.handle.before && ctx) {
                if (d->ctx_destroy_cb)
                    d->ctx_destroy_cb(surface, ctx);
                destroyRenderContext(surface, ctx);
            }
            sp->ctx = nullptr;
            surface->release(); // assume createRenderContext() will not call any gl command, so surface->acquire() is not required.
            if (!e.handle.after)
                return surface;
            ctx = createRenderContext(surface);
            if (!ctx) { // ss will be destroyed if not pushed to list
                std::clog << "ERROR! Failed to create rendering context! platform surface handle: " << surface->nativeHandle() << std::endl;
                return surface; // already release(). FIXME
            }
            if (d->ctx_created_cb)
                d->ctx_created_cb(surface, ctx);
            sp->ctx = ctx;
            surface->setEventCallback([=]{ // TODO: void(Event e)
                d->schedule([=]{
                    if (!process(sp)) {
                        clog << "surface removed by event callback..." << endl;
                    }
                });
            });
            if (!surface->acquire())
                return surface;
            activateRenderContext(surface, ctx);
        }
    }
    if (!ctx) {
        std::clog << "no gfx context. skip rendering..." << std::endl;
        surface->release();
        return surface;
    }
    if (d->draw_cb && d->draw_cb(surface, ctx)) { // not onDraw(surface) with surface is ok, because context is current
        submitRenderContext(surface, ctx);
        surface->submit();
    }
    surface->release();
    return surface;
}
UGS_NS_END
