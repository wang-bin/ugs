/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#include "ugs/PlatformSurface.h"
#include <string>
#include <windows.h>

UGS_NS_BEGIN
class Win32Surface final: public PlatformSurface
{
public:
    Win32Surface(int x = 0, int y = 0, int w = 0, int h = 0);
    void resize(int w, int h) override { doResize(w, h);}
    void close() override;
    void processEvents() override;
private:
    static LRESULT CALLBACK WinProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);
    void processEvent(UINT message, WPARAM wParam, LPARAM lParam);
    void doResize(int w, int h);
};

PlatformSurface* create_win32_surface() {
    return new Win32Surface();
}

LRESULT CALLBACK Win32Surface::WinProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    Win32Surface* surface = handle ? reinterpret_cast<Win32Surface*>(GetWindowLongPtr(handle, GWLP_USERDATA)) : nullptr;
    if (surface) {
        surface->processEvent(message, wParam, lParam);
    }
    return DefWindowProcW(handle, message, wParam, lParam);
}

Win32Surface::Win32Surface(int x, int y, int w, int h)
    : PlatformSurface()
{
    static const wchar_t kClassName[] = L"UGS_Window";
    WNDCLASSW win_class{};
    win_class.style         = CS_OWNDC; // CS_OWNDC: required by wgl? but 0 seems ok: https://www.opengl.org/wiki/Creating_an_OpenGL_Context_(WGL)
    win_class.lpfnWndProc   = &Win32Surface::WinProc;
    win_class.hInstance     = GetModuleHandleW(nullptr);
    win_class.lpszClassName = kClassName;
    RegisterClassW(&win_class);

    const DWORD win32Style = WS_VISIBLE | WS_CAPTION | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_SYSMENU;
    RECT rect = {0, 0, w, h};
    AdjustWindowRect(&rect, win32Style, FALSE);
    const LONG W  = rect.right - rect.left;
    const LONG H = rect.bottom - rect.top;

    resetNativeHandle(CreateWindowW(kClassName, L"Win32Surface", win32Style, x, y, W, H, nullptr, nullptr, GetModuleHandle(nullptr), this));
    HWND hwnd = (HWND)nativeHandle();
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    // fits desktop size by default
    doResize(w, h);

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

void Win32Surface::processEvent(UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND h = (HWND)nativeHandle();
    if (!h)
        return;
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
        break;
    }
}
UGS_NS_END
