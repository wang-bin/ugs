
#pragma once

#include <cstdint>
#include <inspectable.h> // IInspectable
#include <dxgi.h>

namespace WinUI { // winrt::Microsoft::UI::Xaml
    typedef struct event_token
    {
        int64_t value;
    } event_token;
    MIDL_INTERFACE("08844F85-AA1B-540D-BEF2-B2BB7B257F8C")
    ISwapChainPanel : public IInspectable // Microsoft.UI.Xaml.Controls.0.h template <> struct abi<winrt::Microsoft::UI::Xaml::Controls::ISwapChainPanel>
    {
        virtual int32_t __stdcall get_CompositionScaleX(float*) noexcept = 0;
        virtual int32_t __stdcall get_CompositionScaleY(float*) noexcept = 0;
        virtual int32_t __stdcall add_CompositionScaleChanged(void*, event_token*) noexcept = 0;
        virtual int32_t __stdcall remove_CompositionScaleChanged(event_token) noexcept = 0;
        virtual int32_t __stdcall CreateCoreIndependentInputSource(uint32_t, void**) noexcept = 0;
    };
    MIDL_INTERFACE("E7BEAEE7-160E-50F7-8789-D63463F979FA")
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
