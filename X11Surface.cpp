/*
 * Copyright (c) 2016-2018 WangBin <wbsecg1 at gmail.com>
 * Universal Graphics Surface
 * Source code: https://github.com/wang-bin/ugs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "ugs/PlatformSurface.h"
#include <cassert>
#include <iostream>

_Pragma("weak XOpenDisplay")
#pragma weak XDisplayName
#pragma weak XCloseDisplay
#pragma weak XCreateColormap
#pragma weak XCreateWindow
#pragma weak XDestroyWindow
#pragma weak XFlush
#pragma weak XFree
#pragma weak XGetWindowAttributes
#pragma weak XGetErrorText
#pragma weak XInternAtom
#pragma weak XMapWindow
#pragma weak XNextEvent
#pragma weak XPeekEvent
#pragma weak XPending
#pragma weak XResizeWindow
#pragma weak XSetErrorHandler
#pragma weak XSetWMProtocols
#pragma weak XStoreName
#pragma weak XSync
#pragma weak XVisualIDFromVisual
extern "C" {
#include<X11/Xlib.h>
}

UGS_NS_BEGIN

Display* open_x11_display() {
    if (!XDisplayName) {
        std::clog << "weak symbol XDisplayName is null. libX11 is not loaded? set LD_PRELOAD to load X11" << std::endl;
        return nullptr;
    }
    static const char* name = XDisplayName(nullptr);
    Display* d = XOpenDisplay(name);
    if (!d) {
        std::clog << "failed to open default display. try to open :0" << std::endl;
        static const char xdefault[] = ":0";
        name = xdefault;
        d = XOpenDisplay(name);
    }
    std::clog << "open x11 display: " << name << ", result: " << d << std::endl;
    return d;
}

void* ensure_x11_display() {
    static Display* d = open_x11_display();
    return d;
}

class X11Surface final : public PlatformSurface // TODO: XlibSurface
{
public:
    X11Surface();
    ~X11Surface() override {
        if (!display_)
            return;
        Window win = reinterpret_cast<Window>(nativeHandle());
        if (win)
            XDestroyWindow(display_, win);
    }
    Type type() const override { return Type::X11; }
    void* nativeResource() const override {
        return ensure_x11_display();
    }
    bool size(int *w, int *h) const override {
        Window win = reinterpret_cast<Window>(nativeHandle());
        if (!win)
            return false;
        XWindowAttributes wa;
        XGetWindowAttributes(display_, win, &wa);
        if (w)
            *w = wa.width;
        if (h)
            *h = wa.height;
        return true;
    }
    void resize(int w, int h) override {
        XResizeWindow(display_, (Window)nativeHandle(), w, h);
        //XFlush(display_);
         // angle: process events and wait for the size
        PlatformSurface::resize(w, h);
    }
    void processEvents() override;
private:
    Display *display_ = nullptr;
    int w_ = 1920;
    int h_ = 1080;
    Atom WM_DELETE_WINDOW = None;
    Atom WM_PROTOCOLS = None;
};

PlatformSurface* create_x11_surface() { return new X11Surface();}

X11Surface::X11Surface()
    : PlatformSurface()
{
    std::clog << "creating x11 window..." << std::endl;
    display_ = (Display*)nativeResource(); // TODO: open and return
    if (!display_) {
        std::clog << "failed to get x11 display" << std::endl;
        return;
    }
#if 1
    Window root = DefaultRootWindow(display_);
    XSetWindowAttributes swa;
    swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask | StructureNotifyMask;
    // CWColormap: invalid Colormap parameter if setNativeHandleChangeCallback() is called
    Window win = XCreateWindow(display_, root, 0, 0, w_, h_, 0, CopyFromParent, InputOutput, CopyFromParent, /*CWColormap | */CWEventMask, &swa);
#if 0
    XSetWindowAttributes xattr;
    xattr.override_redirect = False;
    XChangeWindowAttributes(display_, win, CWOverrideRedirect, &xattr);
#endif
#else
    int s = DefaultScreen(display_);
    Window win = XCreateSimpleWindow(display_, RootWindow(display_, s), 10, 10, 100, 100, 1, BlackPixel(display_, s), WhitePixel(display_, s));
#endif
    // let wm notify us when close window is requested by user(close button)
    WM_DELETE_WINDOW = XInternAtom(display_, "WM_DELETE_WINDOW", False);
    WM_PROTOCOLS = XInternAtom(display_, "WM_PROTOCOLS", False);
    if (WM_DELETE_WINDOW != None && WM_PROTOCOLS != None) {
        XSetWMProtocols(display_, win, &WM_DELETE_WINDOW, 1);
    }
    XMapWindow(display_, win);
    XStoreName(display_, win, "X11Surface");
    //XSelectInput(display_, win, ExposureMask | KeyPressMask);
    resetNativeHandle(reinterpret_cast<void*>(win));
}

void X11Surface::processEvents() {
    int n = XPending(display_);
    while (n--) {
        XEvent xev;
        XNextEvent(display_, &xev);
        if (xev.type == ConfigureNotify) {
            if (xev.xconfigure.window != reinterpret_cast<Window>(nativeHandle()))
                return;
            if (xev.xconfigure.width == w_ && xev.xconfigure.height == h_)
                return;
            w_ = xev.xconfigure.width;
            h_ = xev.xconfigure.height;
            PlatformSurface::resize(w_, h_);
        } else if (xev.type == DestroyNotify) {

        } else if (xev.type == ClientMessage) {
            if (xev.xclient.message_type == WM_PROTOCOLS && static_cast<Atom>(xev.xclient.data.l[0]) == WM_DELETE_WINDOW) {
                PlatformSurface::close();
            }
        }
    }
}
UGS_NS_END
