/*
 * Copyright (c) 2024 WangBin <wbsecg1 at gmail.com>
 */
#pragma once

#include <cstdint>
#include <inspectable.h> // IInspectable
#include <dxgi.h>

/*
  winrt/impl: abi, uuid, class name and interface is in *.0.h. void* is used for abi
*/
namespace WinUI { // winrt::Microsoft::UI::Xaml
    typedef struct event_token
    {
        int64_t value;
    } event_token;
// https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.controls.swapchainpanel?view=windows-app-sdk-1.5
    MIDL_INTERFACE("08844F85-AA1B-540D-BEF2-B2BB7B257F8C")
    ISwapChainPanel : public IInspectable // impl/Microsoft.UI.Xaml.Controls.0.h template <> struct abi<winrt::Microsoft::UI::Xaml::Controls::ISwapChainPanel>
    {
        virtual int32_t __stdcall get_CompositionScaleX(float*) noexcept = 0;
        virtual int32_t __stdcall get_CompositionScaleY(float*) noexcept = 0;
        virtual int32_t __stdcall add_CompositionScaleChanged(void*, event_token*) noexcept = 0;
        virtual int32_t __stdcall remove_CompositionScaleChanged(event_token) noexcept = 0;
        virtual int32_t __stdcall CreateCoreIndependentInputSource(uint32_t, void**) noexcept = 0;
    };

    enum class DispatcherQueuePriority : int32_t
    {
        Low = -10,
        Normal = 0,
        High = 10,
    };

    MIDL_INTERFACE("2E0872A9-4E29-5F14-B688-FB96D5F9D5F8") // winrt::Microsoft::UI::Dispatching::DispatcherQueueHandler
    IDispatcherQueueHandler : IUnknown
    {
        virtual int32_t __stdcall Invoke() noexcept = 0;
    };

    MIDL_INTERFACE("F6EBF8FA-BE1C-5BF6-A467-73DA28738AE8")
    IDispatcherQueue : IInspectable
    {
        virtual int32_t __stdcall CreateTimer(void**) noexcept = 0;
        virtual int32_t __stdcall TryEnqueue(void*, bool*) noexcept = 0; // IDispatcherQueueHandler
        virtual int32_t __stdcall TryEnqueueWithPriority(int32_t, void*, bool*) noexcept = 0;
        virtual int32_t __stdcall add_ShutdownStarting(void*, event_token*) noexcept = 0;
        virtual int32_t __stdcall remove_ShutdownStarting(event_token) noexcept = 0;
        virtual int32_t __stdcall add_ShutdownCompleted(void*, event_token*) noexcept = 0;
        virtual int32_t __stdcall remove_ShutdownCompleted(event_token) noexcept = 0;
    };

    MIDL_INTERFACE("0CF48751-F1AC-59B8-BA52-6CE7A1444D6F")
    IDispatcherQueue2 : IInspectable
    {
        virtual int32_t __stdcall get_HasThreadAccess(bool*) noexcept = 0;
    };

    MIDL_INTERFACE("E7BEAEE7-160E-50F7-8789-D63463F979FA") // winrt::Microsoft::UI::Xaml::IDependencyObject
    IDependencyObject : public IInspectable
    {
        virtual int32_t __stdcall GetValue(void*, void**) noexcept = 0;
        virtual int32_t __stdcall SetValue(void*, void*) noexcept = 0;
        virtual int32_t __stdcall ClearValue(void*) noexcept = 0;
        virtual int32_t __stdcall ReadLocalValue(void*, void**) noexcept = 0;
        virtual int32_t __stdcall GetAnimationBaseValue(void*, void**) noexcept = 0;
        virtual int32_t __stdcall RegisterPropertyChangedCallback(void*, void*, int64_t*) noexcept = 0;
        virtual int32_t __stdcall UnregisterPropertyChangedCallback(void*, int64_t) noexcept = 0;
        virtual int32_t __stdcall get_Dispatcher(void**) noexcept = 0;
        virtual int32_t __stdcall get_DispatcherQueue(void**) noexcept = 0;
    };
    //MIDL_INTERFACE("FE08F13D-DC6A-5495-AD44-C2D8D21863B0")
    //IFrameworkElement

    MIDL_INTERFACE("63aad0b8-7c24-40ff-85a8-640d944cc325")
    ISwapChainPanelNative : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE SetSwapChain(
            /* [annotation][in] */
            _In_  IDXGISwapChain *swapChain) = 0;

    };
} // namespace WinUI
