/*
 * Copyright (c) 2017-2024 WangBin <wbsecg1 at gmail.com>
 */
// EGLNativeWindowType IInspectable can be: ICoreWindow, ISwapChainPanel, IPropertySet([EGLNativeWindowTypeProperty] = ICoreWindow)
# include <winapifamily.h>
# pragma push_macro("_WIN32_WINNT")
// RuntimeClass: desktop targeting win < 8.1 MUST ensure _WIN32_WINNT/NTDDI_VERSION < 8.1 to avoid using ::RoOriginateError() in GetRuntimeClassName()
// To test win<8.1 build, redefine _WIN32_WINNT, and disable pop_macro
# if _WIN32_WINNT >= 0x0603 && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
//#   undef _WIN32_WINNT
//#   define _WIN32_WINNT 0x0600
# endif
#include "ugs/PlatformSurface.h"
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <windows.h>
#include <wrl/client.h>
#if (_MSC_VER + 0) // missing headers in mingw
#include <wrl/implements.h> // RuntimeClass
#endif
#include <wrl\wrappers\corewrappers.h>
#ifdef GetCurrentTime // defined in WinBase.h, conflicts with windows.ui.xaml
# undef GetCurrentTime
#endif
#include <windows.foundation.h>
#include <windows.system.threading.h>
#include <windows.ui.core.h>
#include <windows.ui.xaml.h>
#include <windows.applicationmodel.core.h>
#include <windows.graphics.display.h>
# pragma pop_macro("_WIN32_WINNT")

#include "UIRun.h"
#include "winui.h"
using namespace Microsoft::WRL; //ComPtr
using namespace Microsoft::WRL::Wrappers; //HString
using namespace ABI::Windows::System::Threading;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::ApplicationModel::Core;

UGS_NS_BEGIN

class WinRTSurface final : public PlatformSurface
{
public:
    WinRTSurface() : PlatformSurface() {
        size_change_token_.value = 0;
        setNativeHandleChangeCallback([this](void* old){
            ComPtr<IInspectable> s(static_cast<IInspectable*>(nativeHandle()));
            if (old != s.Get()) { // updateNativeWindow(nullptr) called outside or resetNativeHandle(nullptr) in dtor
                if (inspectable_)
                    unregisterSizeChange();
            }
            inspectable_ = s;
            if (!s)
                return;
            registerSizeChange();
        });
    }
    ~WinRTSurface() {
        unregisterSizeChange();
    }

    void* nativeHandleForGL() const override {
        return inspectable_.Get();
    }
    bool size(int* w, int *h) const override;
private:
    bool registerSizeChange();
    void unregisterSizeChange();

    ComPtr<IInspectable> inspectable_;
    ComPtr<ABI::Windows::UI::Core::ICoreWindow> win_;
    ComPtr<ABI::Windows::UI::Xaml::Controls::ISwapChainPanel> panel_;
    ComPtr<ABI::Windows::UI::Xaml::IFrameworkElement> fe_;
    ComPtr<ICoreDispatcher> dispatcher_;
    struct {
        ComPtr<WinUI::ISwapChainPanel> panel_;
        ComPtr<WinUI::IFrameworkElement> fe_;
        ComPtr<WinUI::IDispatcherQueue> dispatcher_;
    } winui;
    union {
        EventRegistrationToken size_change_token_;
        WinUI::event_token token_;
    };
};

static float GetLogicalDpi()
{
    const float kDefaultDPI = 96.0f;
    float dpi = kDefaultDPI;
// dpi change: https://learn.microsoft.com/en-us/archive/msdn-magazine/2013/october/windows-with-c-rendering-for-the-windows-runtime
// TODO: runtime version check
#if _WIN32_WINNT >= 0x0602 // GetActivationFactory/HString requires win8(OneCore.lib/WindowsApp.lib). TODO: resolve from combase.dll?
# if _WIN32_WINNT < 0x0A00
    ComPtr<ABI::Windows::Graphics::Display::IDisplayPropertiesStatics> info;
    MS_ENSURE(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(), &info), kDefaultDPI);
# else
    ComPtr<ABI::Windows::Graphics::Display::IDisplayInformationStatics> st;
    MS_ENSURE(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayInformation).Get(), &st), kDefaultDPI);
    ComPtr<ABI::Windows::Graphics::Display::IDisplayInformation> info;
    MS_ENSURE(st->GetForCurrentView(&info), kDefaultDPI);
# endif
    MS_ENSURE(info->get_LogicalDpi(&dpi), kDefaultDPI);
#endif
    return dpi;
}

// TODO: ISwapChainPanel.get_CompositionScaleX/Y
float DIPstoPixels(float dips)
{
    static const float dipsPerInch = 96.0f;
    return std::floor(dips * GetLogicalDpi() / dipsPerInch + 0.5f); // round to nearest int
}

bool WinRTSurface::size(int* w, int *h) const
{
    if (!inspectable_)
        return false;
    if (win_) {
        ABI::Windows::Foundation::Rect bounds;
        MS_ENSURE(win_->get_Bounds(&bounds), false);
        if (w)
            *w = DIPstoPixels(bounds.Width);
        if (h)
            *h = DIPstoPixels(bounds.Height);
        return true;
    }
    if (panel_) {
        ComPtr<ABI::Windows::UI::Xaml::IFrameworkElement> fe;
        MS_ENSURE(panel_.As(&fe), false);
        DOUBLE v;
        if (w) { // get_Width/Height is -inf, get_ActualWidth/Height is rendered size in dips(device independent unit)
            MS_ENSURE(fe->get_ActualWidth(&v), false);
            *w = int(v);
        }
        if (h) {
            MS_ENSURE(fe->get_ActualHeight(&v), false);
            *h = int(v);
        }
        return true;
    }
    return false;
}

class SwapChainPanelSizeChangedHandler : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ABI::Windows::UI::Xaml::ISizeChangedEventHandler, WinUI::ISizeChangedEventHandler>
{
public:
    HRESULT RuntimeClassInitialize(WinRTSurface* surface) {
        if (!surface)
            return E_INVALIDARG;
        surface_ = surface;
        return S_OK;
    }
    // ISizeChangedEventHandler
    IFACEMETHOD(Invoke)(IInspectable *sender, ABI::Windows::UI::Xaml::ISizeChangedEventArgs *sizeChangedEventArgs) override {
        // from angle: The size of the ISwapChainPanel control is returned in DIPs, the same unit as viewports.
        // XAML Clients of the ISwapChainPanel are required to use dips to define their layout sizes as well.
        ABI::Windows::Foundation::Size newSize;
        MS_ENSURE(sizeChangedEventArgs->get_NewSize(&newSize), S_OK);
        surface_->resize((int)newSize.Width, (int)newSize.Height);
        return S_OK;
    }
    int32_t __stdcall Invoke(IInspectable *sender, WinUI::ISizeChangedEventArgs *sizeChangedEventArgs) noexcept override {
        // from angle: The size of the ISwapChainPanel control is returned in DIPs, the same unit as viewports.
        // XAML Clients of the ISwapChainPanel are required to use dips to define their layout sizes as well.
        WinUI::Size newSize;
        MS_ENSURE(sizeChangedEventArgs->get_NewSize(&newSize), S_OK);
        surface_->resize((int)newSize.Width, (int)newSize.Height);
        return S_OK;
    }
private:
    WinRTSurface* surface_ = nullptr;
};

typedef ABI::Windows::Foundation::__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CWindowSizeChangedEventArgs_t IWindowSizeChangedEventHandler;
class CoreWindowSizeChangedHandler : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IWindowSizeChangedEventHandler>
{
public:
    HRESULT RuntimeClassInitialize(WinRTSurface* surface) {
        if (!surface)
            return E_INVALIDARG;
        surface_ = surface;
        return S_OK;
    }
    // IWindowSizeChangedEventHandler
    IFACEMETHOD(Invoke)(ABI::Windows::UI::Core::ICoreWindow *sender, ABI::Windows::UI::Core::IWindowSizeChangedEventArgs *sizeChangedEventArgs) {
        ABI::Windows::Foundation::Size windowSize;
        MS_ENSURE(sizeChangedEventArgs->get_Size(&windowSize), S_OK);
        const Size windowSizeInPixels = {DIPstoPixels(windowSize.Width), DIPstoPixels(windowSize.Height)};
        surface_->resize((int)windowSizeInPixels.Width, (int)windowSizeInPixels.Height);
        return S_OK;
    }
private:
    WinRTSurface* surface_ = nullptr;
};

bool WinRTSurface::registerSizeChange()
{
    if (SUCCEEDED(inspectable_.As(&panel_)) && panel_) {
        ComPtr<ABI::Windows::UI::Xaml::ISizeChangedEventHandler> h;
        MS_ENSURE(Microsoft::WRL::MakeAndInitialize<SwapChainPanelSizeChangedHandler>(&h, this), false);
        MS_ENSURE(inspectable_.As(&fe_), false);
        ComPtr<ABI::Windows::UI::Xaml::IDependencyObject> dep;
        MS_ENSURE(panel_.As(&dep), false);
        MS_ENSURE(dep->get_Dispatcher(&dispatcher_), false);
        return RunOnUIThread(dispatcher_.Get(), [this, h]{
            MS_ENSURE(fe_->add_SizeChanged(h.Get(), &size_change_token_), false);
            return true;
        }); // usually successed, and return value is not used, so no wait
    } else if (SUCCEEDED(inspectable_.As(&winui.panel_)) && winui.panel_) {
        ComPtr<WinUI::ISizeChangedEventHandler> h;
        MS_ENSURE(Microsoft::WRL::MakeAndInitialize<SwapChainPanelSizeChangedHandler>(&h, this), false);
        MS_ENSURE(inspectable_.As(&winui.fe_), false);
        ComPtr<WinUI::IDependencyObject> dep;
        MS_ENSURE(winui.panel_.As(&dep), false);
        MS_ENSURE(dep->get_DispatcherQueue(&winui.dispatcher_), false);
        return RunOnUIThread(winui.dispatcher_.Get(), [this, h]{
            MS_ENSURE(winui.fe_->add_SizeChanged(h.Get(), &token_), false);
            return true;
        });
        return true;
    }
    ComPtr<ABI::Windows::Foundation::Collections::IPropertySet> ps;
    if (SUCCEEDED(inspectable_.As(&ps)) && ps) {
#if _WIN32_WINNT >= 0x0602
        ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>> propMap;
        if (SUCCEEDED(ps.As(&propMap))) {
            boolean hasKey = false;
            const auto key = HStringReference(L"EGLNativeWindowTypeProperty");
            if (SUCCEEDED(propMap->HasKey(key.Get(), &hasKey)) && hasKey)
                MS_ENSURE(propMap->Lookup(key.Get(), &win_), false);
        }
#endif // _WIN32_WINNT >= 0x0602
    } else {
        MS_ENSURE(inspectable_.As(&win_), false);
    }
    if (win_) {
        ComPtr<IWindowSizeChangedEventHandler> h;
        MS_ENSURE(Microsoft::WRL::MakeAndInitialize<CoreWindowSizeChangedHandler>(&h, this/*wp?*/), false);
        MS_ENSURE(win_->add_SizeChanged(h.Get(), &size_change_token_), false);
        // or use unregister function
        return true;
    }
    return false;
}

void WinRTSurface::unregisterSizeChange()
{
    if (!size_change_token_.value)
        return;
    if (win_) {
        win_->remove_SizeChanged(size_change_token_);
        size_change_token_.value = 0;
    }
    if (panel_) {
        RunOnUIThread(dispatcher_.Get(), [this]{
            fe_->remove_SizeChanged(size_change_token_);
            size_change_token_.value = 0;
        });
    } else if (winui.panel_) {
        RunOnUIThread(winui.dispatcher_.Get(), [this]{
            winui.fe_->remove_SizeChanged(token_);
            token_.value = 0;
        });
    }
}

PlatformSurface* create_winrt_surface() { return new WinRTSurface();}
UGS_NS_END
