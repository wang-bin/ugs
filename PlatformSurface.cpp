/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#include "ugsurface/PlatformSurface.h"
#include "base/BlockingQueue.h"
#include <chrono>

UGSURFACE_NS_BEGIN
extern PlatformSurface* create_android_surface();
extern PlatformSurface* create_uikit_surface();
extern PlatformSurface* create_wfc();
extern PlatformSurface* create_rpi_surface();
extern PlatformSurface* create_x11_surface();
extern PlatformSurface* create_win32_surface();
extern PlatformSurface* create_wayland_surface();
extern PlatformSurface* create_gbm_surface();
typedef PlatformSurface* (*surface_creator)();

PlatformSurface* PlatformSurface::create(Type type)
{
#ifdef __ANDROID__
    return create_android_surface();
#endif
#ifdef HAVE_WAYLAND
    if (type == Type::Wayland)
        return create_wayland_surface();
#endif
#ifdef HAVE_X11
    if (type == Type::X11)
        return create_x11_surface();
#endif
#ifdef HAVE_GBM
    if (type == Type::GBM)
        return create_gbm_surface();
#endif
    // fallback to platform default surface
    for (auto create_win : std::initializer_list<surface_creator>{ // vs2013 does not support {}, it requires explicit std::initializer_list<surface_creator>{}
#if defined(_WIN32)
        create_win32_surface,
#elif defined(OS_RPI)
        //create_wfc,
        create_rpi_surface,
#elif defined(__APPLE__)
# if !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
        create_uikit_surface,
# endif
#endif
// APPLE, Cygwin can support X11
#if (HAVE_X11+0) // defined(__gnu_linux__) && !defined(ANDROID)
        create_x11_surface,
#endif
#if (HAVE_WAYLAND+0)
        create_wayland_surface,
#endif
#if (HAVE_GBM+0)
        create_gbm_surface,
#endif
    }) { // TODO: how to avoid crash if create error?
        PlatformSurface* pw = create_win();
        if (pw && pw->nativeHandle())
            return pw;
        delete pw;
    }
    return new PlatformSurface();
}

class PlatformSurface::Private
{
public:
    void* native_handle = nullptr;
    std::function<void(void*)> handle_cb = nullptr;
    std::function<void()> cb = nullptr;
    BlockingQueue<PlatformSurface::Event> events;
};

PlatformSurface::PlatformSurface()
    : d(new Private())
{
}

PlatformSurface::~PlatformSurface()
{
    delete d;
}

void PlatformSurface::resetNativeHandle(void* handle)
{
    if (d->native_handle == handle) // TODO: no check, used to check resize in callback
        return;
    auto old = d->native_handle;
    d->native_handle = handle;
    if (d->handle_cb)
        d->handle_cb(old);
}

void PlatformSurface::setNativeHandleChangeCallback(std::function<void(void*)> cb)
{
    d->handle_cb = cb;
}

void* PlatformSurface::nativeHandle() const
{
    return d->native_handle;
}

void PlatformSurface::setEventCallback(std::function<void()> cb)
{
    d->cb = cb;
}

void PlatformSurface::resize(int w, int h)
{
    Event e;
    e.type = Event::Resize;
    e.size = {w, h};
    pushEvent(e);
}

void PlatformSurface::close()
{
    Event e;
    e.type = Event::Close;
    pushEvent(e);
}

bool PlatformSurface::popEvent(Event &e, int64_t timeout)
{
    using namespace std::chrono;
    if (d->events.size() == 0) {
        processEvents();
        if (timeout <= 0)
            return false;
    }
    auto t0 = steady_clock::now();
    while (true) {
        if (d->events.tryPop(e, 1) > 0)
            return true;
        processEvents();
        // if timeout is 0, dt always <0
        if (timeout - duration_cast<milliseconds>(steady_clock::now() - t0).count() < -1)
            break;
    }
    return false;
}

void PlatformSurface::pushEvent(const Event &e)
{
    d->events.push(e);
    onEvent();
}

void PlatformSurface::onEvent()
{
    if (d->cb)
        d->cb();
}
UGSURFACE_NS_END
