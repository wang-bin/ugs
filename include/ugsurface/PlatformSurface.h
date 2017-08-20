/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#pragma once
#include <limits>
#include "export.h"
#include <functional>

UGSURFACE_NS_BEGIN

/*!
 * \brief javaVM
 * Set/Get current java vm
 * \param vm null to get current vm
 * \return current vm
 */
UGSURFACE_API void* javaVM(void* vm = nullptr);

class UGSURFACE_API PlatformSurface {
public:
    class Event {
    public:
        enum Type : int8_t {
            Close,
            Resize,
        };
        Type type;

        struct ResizeEvent {
            int width;
            int height;
        };
        union {
            ResizeEvent size;
        };
    };

    enum class Type : int8_t {
        Default, // platform default window/surface type if only 1 type is supported
        X11,
        Wayland,
        GBM,
    };
    /*!
     * If OS and window system supports to create multiple kinds of window/surface, Type must be specified to create the desired one.
     */
    static PlatformSurface* create(Type type = Type::Default); // TODO: param existing NativeWindow? for android, ios etc. if not null, create a wrapper. if null, create internal window/surface
    virtual ~PlatformSurface();
    void setEventCallback(std::function<void()> cb);
    void resetNativeHandle(void* h);
    void* nativeHandle() const;
    /*!
      \brief nativeHandleForGL
      used to create GL context. It can be different with nativeHandle, for example android nativeHandle() is a Surface ptr, while nativeHandleForGL() is ANativeWindow.
      TODO: HDC for WGL, Visual for GLX?
     */
    virtual void* nativeHandleForGL() const { return nativeHandle();}
    virtual Type type() const {return Type::Default;}
    virtual void* nativeResource() const {return nullptr;} // extra resource required by gfx context, e.g. wayland and x11 display
    virtual void submit() {}
    virtual void resize(int w, int h);
    virtual void close();
    virtual void processEvents() {}
    /*!
     * \brief popEvent
     * \return false if no event
     */
    bool popEvent(Event& e, int64_t timeout = (std::numeric_limits<int64_t>::max)());
protected:
    void pushEvent(const Event& e);
    // call in your ctor if want to observe the changes. the callback is called resetNativeHandle() parameter is a different handle.
    // can not use virtual method in resetNativeHandle() because it may be called in ctor
    // NOTE: it's recommended to call resize(w, h) in the callback
    void setNativeHandleChangeCallback(std::function<void(void* old)> cb);

    PlatformSurface();
private:
    void onEvent();

    class Private;
    Private* d;
};

UGSURFACE_NS_END
