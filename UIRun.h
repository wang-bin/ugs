/*
 * Copyright (c) 2018-2024 WangBin <wbsecg1 at gmail.com>
 */
#pragma once
#include <functional>

/*
  https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.dependencyobject?view=winrt-22621#dependencyobject-and-threading
  DependencyObject.Dispatcher is the only instance API of DependencyObject or any of its subclasses(UIElement, Geometry, FrameworkTemplate, Style, Window..)
  that can be accessed from a non-UI thread without throwing a cross-thread exception
 */
// run f on UI thread and wait to finish. return false if no UI thread or an error occurs. return a future object?
/*!
  \brief RunOnUIThread
  \param dispatcher ICoreDispatcher or WinUI3 IDispatcherQueue
*/
bool RunOnUIThread(void* dispatcher, std::function<void()>&& f);

template<typename F>
auto GetOnUIThread(void* dispatcher, F&& f) // && can't forward.
{
    decltype(f()) r;
    RunOnUIThread(dispatcher, [f = std::forward<F>(f), &r]{
        r = f();
    });
    return r;
}

#define MS_ENSURE(f, ...) MS_CHECK(f, return __VA_ARGS__;)
#define MS_WARN(f) MS_CHECK(f)
#define MS_CHECK(f, ...)  do { \
        while (FAILED(GetLastError())) {} \
        const HRESULT hr = f; \
        if (FAILED(hr)) { \
            std::clog << #f "  ERROR@" << __LINE__ << __FUNCTION__ << ": (" << std::hex << hr << std::dec << ") " << std::error_code(hr, std::system_category()).message() << std::endl; \
            __VA_ARGS__ \
        } \
    } while (false)
