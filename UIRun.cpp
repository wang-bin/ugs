/*
 * Copyright (c) 2018-2024 WangBin <wbsecg1 at gmail.com>
 */

#include "UIRun.h"
#include <wrl/client.h>
#if (_MSC_VER + 0) // missing headers in mingw
#include <wrl/implements.h> // RuntimeClass
#endif
#ifdef GetCurrentTime // defined in WinBase.h, conflicts with windows.ui.xaml
# undef GetCurrentTime
#endif
#include <windows.foundation.h>
#include <windows.ui.xaml.h>
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Core;
using namespace Microsoft::WRL; //ComPtr

#include <future>
#include <iostream>
#include <type_traits>
#include "winui.h"
using namespace std;

// https://github.com/microsoft/microsoft-ui-xaml/blob/winui3/release/1.5-preview1/dxaml/xcp/components/base/inc/wrlhelper.h
// RuntimeClass: desktop targeting win < 8.1 MUST ensure _WIN32_WINNT/NTDDI_VERSION < 8.1 to avoid using ::RoOriginateError
template<typename TDelegateInterface, typename TCallback>
class HandlerDelegate final: public RuntimeClass<RuntimeClassFlags<ClassicCom>, TDelegateInterface>, private std::remove_reference_t<TCallback>
{
    using TReturn = std::invoke_result_t<decltype(&TDelegateInterface::Invoke),TDelegateInterface>;
public:
    HandlerDelegate(TCallback&& callback) : std::remove_reference_t<TCallback>(std::forward<TCallback>(callback)) {}
    TReturn __stdcall Invoke() noexcept(std::is_nothrow_invocable_v<decltype(&TDelegateInterface::Invoke), TDelegateInterface>) override {
        (*this)();
        return S_OK;
    }
};

template<typename TDelegateInterface, typename TCallback>
ComPtr<TDelegateInterface> MakeCallback(TCallback&& callback)
{
    return Make<HandlerDelegate<TDelegateInterface, TCallback>>(std::forward<TCallback>(callback));
}

template<typename TDelegateInterface, typename TCallback>
class AgileHandlerDelegate final: public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, TDelegateInterface, IAgileObject>, private std::remove_reference_t<TCallback>
{
    using TReturn = std::invoke_result_t<decltype(&TDelegateInterface::Invoke),TDelegateInterface>;
public:
    AgileHandlerDelegate(TCallback&& callback) : std::remove_reference_t<TCallback>(std::forward<TCallback>(callback)) {}
    TReturn __stdcall Invoke() noexcept(std::is_nothrow_invocable_v<decltype(&TDelegateInterface::Invoke), TDelegateInterface>) override {
        (*this)();
        return S_OK;
    }
};

template<typename TDelegateInterface, typename TCallback>
ComPtr<TDelegateInterface> MakeAgileCallback(TCallback&& callback)
{
    return Make<AgileHandlerDelegate<TDelegateInterface, TCallback>>(std::forward<TCallback>(callback));
}

// https://github.com/google/angle/blob/69f5e9ca60cd6adfecd9eb8c969beeec30a4813d/src/libANGLE/renderer/d3d/d3d11/winrt/SwapChainPanelNativeWindow.cpp#L39
bool RunOnUIThread(ICoreDispatcher* dispatcher, function<void()>&& f)
{
    boolean isUI;
    MS_ENSURE(dispatcher->get_HasThreadAccess(&isUI), false);
    if (isUI) {
        f();
        return true;
    }
    auto p = make_shared<promise<bool>>();
    ComPtr<IAsyncAction> op;
    MS_ENSURE(dispatcher->RunAsync(CoreDispatcherPriority_Normal
        , MakeAgileCallback<IDispatchedHandler>([f = std::move(f), p]() mutable {
            f();
            p->set_value(true);
        }).Get(), &op), false);
    //MS_ENSURE(op->GetResults(), false); // wait
    return p->get_future().get();
}

bool RunOnUIThread(WinUI::IDispatcherQueue* dispatcher, function<void()>&& f)
{
    ComPtr<WinUI::IDispatcherQueue2> dispatcher2;
    MS_ENSURE(dispatcher->QueryInterface(IID_PPV_ARGS(&dispatcher2)), false);
    bool isUI;
    MS_ENSURE(dispatcher2->get_HasThreadAccess(&isUI), false);
    if (isUI) {
        f();
        return true;
    }
    auto p = make_shared<promise<bool>>();
    bool enqueued = false;
    // Callback<Implements<RuntimeClassFlags<ClassicCom>, WinUI::IDispatcherQueueHandler>>(cb)
    // handler must be agile, otherwise RO_E_MUST_BE_AGILE
    MS_ENSURE(dispatcher->TryEnqueueWithPriority((int32_t)WinUI::DispatcherQueuePriority::Normal
        , MakeAgileCallback<WinUI::IDispatcherQueueHandler>([f = std::move(f), p]() mutable {
            f();
            p->set_value(true);
        }).Get(), &enqueued), false);
    if (!enqueued)
        return false;
    return p->get_future().get();
}

bool RunOnUIThread(void* d, function<void()>&& f)
{
    ComPtr<IInspectable> i((IInspectable*)d);
    ComPtr<ICoreDispatcher> cd;
    ComPtr<WinUI::IDispatcherQueue> dq;
    if (SUCCEEDED(i.As(&cd)) && cd) {
        return RunOnUIThread(cd.Get(), std::move(f));
    } else if (SUCCEEDED(i.As(&dq)) && dq) {
        return RunOnUIThread(dq.Get(), std::move(f));
    }
    return false;
}
