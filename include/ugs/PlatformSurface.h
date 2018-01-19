/*
 * Copyright (c) 2016-2018 WangBin <wbsecg1 at gmail.com>
 * Universal Graphics Surface
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
        Wayland,
        GBM,
    };
    /*!
     * If OS and window system supports to create multiple kinds of window/surface, Type must be specified to create the desired one.
     */
    // TODO: from an existing native handle, and reset later
    static PlatformSurface* create(Type type = Type::Default); // TODO: param existing NativeWindow? for android, ios etc. if not null, create a wrapper. if null, create internal window/surface
    virtual ~PlatformSurface();
    void setEventCallback(std::function<void()> cb); // TODO: void(Event) as callback and remove event queue which can be implemented externally
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
    void setNativeHandleChangeCallback(std::function<void(void* old)> cb);

    PlatformSurface();
private:
    void pushEvent(Event&& e);
    class Private;
    Private* d; // TODO: unique_ptr
};

UGS_NS_END
