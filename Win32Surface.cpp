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
#include <string>
#include <windows.h>

UGS_NS_BEGIN
class Win32Surface final: public PlatformSurface
{
public:
    Win32Surface(void* handle = nullptr, int x = 0, int y = 0, int w = 0, int h = 0);
    ~Win32Surface();
    bool size(int* w, int *h) const override;
    void resize(int w, int h) override { doResize(w, h);}
    void close() override;
    void processEvents() override;
    // TODO: natvieHandleForGL() const { HDC}
    static LRESULT CALLBACK WinProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);
private:
    bool processEvent(UINT message, WPARAM wParam, LPARAM lParam);
    void doResize(int w, int h);

    HWND win_ = nullptr; // own window
    WNDPROC orig_proc_ = nullptr;
};

PlatformSurface* create_win32_surface(void* handle) {
    return new Win32Surface(handle);
}

LRESULT CALLBACK Win32Surface::WinProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    Win32Surface* surface = handle ? reinterpret_cast<Win32Surface*>(GetWindowLongPtr(handle, GWLP_USERDATA)) : nullptr;
    if (surface) {
        if (!surface->processEvent(message, wParam, lParam)
             && surface->orig_proc_)
            return CallWindowProc(surface->orig_proc_, handle, message, wParam, lParam);
    }
    return DefWindowProcW(handle, message, wParam, lParam);
}

static const wchar_t kClassName[] = L"UGS_Window";
Win32Surface::Win32Surface(void* handle, int x, int y, int w, int h)
    : PlatformSurface()
{
    setNativeHandleChangeCallback([this](void* old){
        HWND hwnd = (HWND)nativeHandle();
        if (!hwnd)
            return;
        // replace WndProc with our implementation to ensure events can be handled. always call DefWindowProc for original WndProc
        if (hwnd != win_)
            orig_proc_ = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Win32Surface::WinProc));
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    });
    if (handle)
        return;
    // call once? for current dll(module)
    WNDCLASSW win_class{};
    win_class.style         = CS_OWNDC; // CS_OWNDC: required by wgl? but 0 seems ok: https://www.opengl.org/wiki/Creating_an_OpenGL_Context_(WGL)
    win_class.lpfnWndProc   = &Win32Surface::WinProc;
    win_class.hInstance     = GetModuleHandleW(nullptr);//HINST_THISCOMPONENT;//
    win_class.lpszClassName = kClassName;
    RegisterClassW(&win_class);

    DWORD win32Style = WS_VISIBLE | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_SYSMENU;
    RECT rect = {0, 0, w, h};
    AdjustWindowRect(&rect, win32Style, FALSE);
    const LONG W  = rect.right - rect.left;
    const LONG H = rect.bottom - rect.top;
    win_ = CreateWindowExW(0, kClassName, L"Win32Surface", win32Style, x, y, W, H, nullptr, nullptr, GetModuleHandle(nullptr), this);
    resetNativeHandle(win_);
    // fits desktop size by default
    doResize(w, h);
}

Win32Surface::~Win32Surface()
{
    if (win_)
        DestroyWindow(win_);
    HWND hwnd = (HWND)nativeHandle();
    if (hwnd && orig_proc_)
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, LONG_PTR(orig_proc_));
}

bool Win32Surface::size(int* w, int* h) const
{
    HWND hwnd = (HWND)nativeHandle();
    if (!hwnd)
        return false;
    RECT rect{};
    GetClientRect(hwnd, &rect);
    if (w)
        *w = rect.right - rect.left;
    if (h)
        *h = rect.bottom - rect.top;
    return true;
}

void Win32Surface::doResize(int w, int h)
{
    HWND hwnd = (HWND)nativeHandle();
    RECT rect{0, 0, w, h};
    AdjustWindowRect(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE);
    w  = rect.right - rect.left;
    h = rect.bottom - rect.top;
    SetWindowPos(hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
}

void Win32Surface::close()
{
    HWND h = (HWND)nativeHandle();
    if (!h)
        return;
    CloseWindow(h);
    DestroyWindow(h);
}

void Win32Surface::processEvents()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool Win32Surface::processEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h = (HWND)nativeHandle();
    if (!h)
        return false;
    switch (message) {
    case WM_CREATE:
        break;
    case WM_DESTROY:
    case WM_CLOSE: {
        PlatformSurface::close();
    }
        break;
    case WM_SIZE: {
        RECT rect;
        GetClientRect(h, &rect);
        PlatformSurface::resize(rect.right - rect.left, rect.bottom - rect.top);
    }
        break;
    default:
        return false;
    }
    return false; // return false to always CallWindowProc? 
}
UGS_NS_END
