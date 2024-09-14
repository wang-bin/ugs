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
#include <cinttypes> //vs2013
#include <limits>
#include "export.h"
#include <functional>

UGS_NS_BEGIN
/*!
 * \brief javaVM
 * Set/Get current java vm
 * \param vm null to get current vm
 * \return current vm
 */
UGS_API void* javaVM(void* vm = nullptr);

class UGS_API PlatformSurface {
public:
    class Event {
    public:
        enum Type : int8_t {
            Close,
            Resize,
            NativeHandle,
        };
        Type type;

        struct ResizeEvent {
            int width;
            int height;
        };
        struct NativeHandleEvent {
            void* before;
            void* after;
        };
        union {
            ResizeEvent size;
            NativeHandleEvent handle;
        };
    };

    enum class Type : int8_t {
        Default, // platform default window/surface type if only 1 type is supported
        X11,
        Xlib = X11,
        Wayland,
        GBM,
        XCB,
    };
    /*!
     * If OS and window system supports to create multiple kinds of window/surface, Type must be specified to create the desired one.
     */
    static PlatformSurface* create(Type type = Type::Default) {return create(nullptr, type);}
    // param handle is exsiting native window handle. for android, ios etc..
    // TODO: if handle is not null, create a wrapper(call resetNativeHandle?). if null, create internal window/surface, e.g. x11, win32 surface
    static PlatformSurface* create(void* handle, Type type = Type::Default);
    virtual ~PlatformSurface();
    Type type() const;
    void setEventCallback(const std::function<void()>& cb); // TODO: void(Event) as callback and remove event queue which can be implemented externally
    //
    void resetNativeHandle(void* h);
    void* nativeHandle() const;
    /*!
      \brief nativeHandleForGL
      used to create GL context. It can be different with nativeHandle, for example android nativeHandle() is a Surface ptr, while nativeHandleForGL() is ANativeWindow.
      TODO: HDC for WGL, Visual for GLX?
     */
    virtual void* nativeHandleForVulkan() const { return nativeHandle();}
    virtual void* nativeHandleForGL() const { return nativeHandle();}
    virtual void* nativeResource() const {return nullptr;} // extra resource required by gfx context, e.g. wayland and x11 display
    virtual void submit() {}
    virtual bool size(int *w, int *h) const {return false;}
    virtual void resize(int w, int h);
    virtual void close();
    virtual void processEvents() {}
    virtual bool acquire() { return true;}
    virtual void release() {}
    /*!
     * \brief popEvent
     * \return false if no event
     */
    bool popEvent(Event& e);
protected:
    // call in your ctor if want to observe the changes. the callback is called resetNativeHandle() parameter is a different handle.
    // can not use virtual method in resetNativeHandle() because it may be called in ctor
    // NOTE: it's recommended to call resize(w, h) in the callback
    void setNativeHandleChangeCallback(const std::function<void(void* old)>& cb);

    PlatformSurface(Type type = Type::Default);
private:
    void pushEvent(Event&& e);
    class Private;
    Private* d; // TODO: unique_ptr
};

UGS_NS_END
