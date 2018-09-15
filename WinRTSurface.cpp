/*
 * Copyright (c) 2017-2018 WangBin <wbsecg1 at gmail.com>
 */
// EGLNativeWindowType IInspectable can be: ICoreWindow, ISwapChainPanel, IPropertySet([EGLNativeWindowTypeProperty] = ICoreWindow)
#include "ugs/PlatformSurface.h"
#include <future>
#include <iostream>
#include <string>
#include <system_error>
#include <windows.h>
#include <wrl.h> //
#include <wrl/client.h>
#include <windows.foundation.h>
#include <windows.system.threading.h>
#include <windows.ui.core.h>
#include <windows.ui.xaml.h>
#include <windows.applicationmodel.core.h>
#include <windows.graphics.display.h>

using namespace Microsoft::WRL; //ComPtr
using namespace Microsoft::WRL::Wrappers; //HString
using namespace ABI::Windows::System::Threading;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::ApplicationModel::Core;

#define MS_ENSURE(f, ...) MS_CHECK(f, return __VA_ARGS__;)
#define MS_WARN(f) MS_CHECK(f)
#define MS_CHECK(f, ...)  do { \
        while (FAILED(GetLastError())) {} \
        HRESULT hr = f; \
        if (FAILED(hr)) { \
            std::clog << #f "  ERROR@" << __LINE__ << __FUNCTION__ << ": (" << std::hex << hr << std::dec << ") " << std::error_code(hr, std::system_category()).message() << std::endl; \
            __VA_ARGS__ \
        } \
    } while (false)

UGS_NS_BEGIN

class WinRTSurface final : public PlatformSurface
{
public:
    WinRTSurface() : PlatformSurface() {
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
    EventRegistrationToken size_change_token_;
};

static float GetLogicalDpi()
{
    const float kDefaultDPI = 96.0f;
#if _WIN32_WINNT < 0x0A00
    ComPtr<ABI::Windows::Graphics::Display::IDisplayPropertiesStatics> info;
    MS_ENSURE(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(), &info), kDefaultDPI);
#else
    ComPtr<ABI::Windows::Graphics::Display::IDisplayInformationStatics> st;
    MS_ENSURE(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayInformation).Get(), &st), kDefaultDPI);
    ComPtr<ABI::Windows::Graphics::Display::IDisplayInformation> info;
    MS_ENSURE(st->GetForCurrentView(&info), kDefaultDPI);
#endif
    float dpi = kDefaultDPI;
    MS_ENSURE(info->get_LogicalDpi(&dpi), kDefaultDPI);
    return dpi;
}

float DPIsToPixels(float dips)
{
    static const float dipsPerInch = 96.0f;
    return dips * GetLogicalDpi() / dipsPerInch;
}

bool WinRTSurface::size(int* w, int *h) const
{
    if (!inspectable_)
        return false;
    if (win_) {
        ABI::Windows::Foundation::Rect bounds;
        MS_ENSURE(win_->get_Bounds(&bounds), false);
        if (w)
            *w = DPIsToPixels(bounds.Width);
        if (h)
            *h = DPIsToPixels(bounds.Height);
        return true;
    }
    if (panel_) {
        ComPtr<ABI::Windows::UI::Xaml::IFrameworkElement> fe;
        MS_ENSURE(panel_.As(&fe), false);
        DOUBLE v;
        if (w) {
            MS_ENSURE(fe->get_Width(&v), false);
            *w = int(v);
        }
        if (h) {
            MS_ENSURE(fe->get_Width(&v), false);
            *h = int(v);
        }
        return true;
    }
    return false;
}

static ICoreDispatcher* GetDispatcher()
{
    static __declspec(thread) ICoreDispatcher *dispatcher = nullptr;
    if (dispatcher)
        return dispatcher;
    ComPtr<ICoreImmersiveApplication> application;
    MS_ENSURE(RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication).Get(),
                                IID_PPV_ARGS(&application)), nullptr);
    ComPtr<ICoreApplicationView> view;
    MS_ENSURE(application->get_MainView(&view), nullptr);
    if (!view)
        return nullptr;
    ComPtr<ICoreWindow> window;
    MS_ENSURE(view->get_CoreWindow(&window), nullptr);
    if (window)
        MS_ENSURE(window->get_Dispatcher(&dispatcher), nullptr);
    // In case the application is launched via activation there might not be a main view (eg ShareTarget).
    // Hence iterate through the available views and try to find a dispatcher in there
    ComPtr<ABI::Windows::Foundation::Collections::IVectorView<CoreApplicationView*>> appViews;
    MS_ENSURE(application->get_Views(&appViews), nullptr);
    unsigned int count = 0;
    MS_ENSURE(appViews->get_Size(&count), nullptr);
    for (unsigned int i = 0; i < count; ++i) {
        MS_ENSURE(appViews->GetAt(i, &view), nullptr);
        MS_ENSURE(view->get_CoreWindow(&window), nullptr);
        if (!window)
            continue;
        MS_ENSURE(window->get_Dispatcher(&dispatcher), nullptr);
        if (dispatcher)
            break;
    }
    return dispatcher;
}

class AgileDispatchedHandler : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>, IDispatchedHandler, IAgileObject>
{
public:
    AgileDispatchedHandler(std::function<void()> f, std::promise<bool>&& p) : f_(f), p_(std::move(p)) {}
    HRESULT __stdcall Invoke() override {
        f_();
        p_.set_value(true);
        return S_OK;
    }
private:
    std::function<void()> f_;
    std::promise<bool> p_;
};

bool RunOnUIThread(std::function<void()> f, bool wait = true)
{
    ICoreDispatcher *dispatcher = GetDispatcher();
    if (!dispatcher) // no ui
        return false;
    boolean isUI;
    MS_ENSURE(dispatcher->get_HasThreadAccess(&isUI), false);
    if (isUI) {
        f();
        return true;
    }
    std::promise<bool> p;
    std::future<bool> pf = p.get_future();
    ComPtr<IAsyncAction> op;
    // Callback<AddFtmBase<IDispatchedHandler>::Type>(...);
    MS_ENSURE(dispatcher->RunAsync(CoreDispatcherPriority_Normal, Make<AgileDispatchedHandler>(f, std::move(p)).Get(), &op), false);
    if (wait)
        return pf.get();
    return true;
}

class SwapChainPanelSizeChangedHandler : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, ABI::Windows::UI::Xaml::ISizeChangedEventHandler>
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
        // The size of the ISwapChainPanel control is returned in DIPs, the same unit as viewports.
        // XAML Clients of the ISwapChainPanel are required to use dips to define their layout sizes as well.
        ABI::Windows::Foundation::Size newSize;
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
        const Size windowSizeInPixels = {DPIsToPixels(windowSize.Width), DPIsToPixels(windowSize.Height)};
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
        ComPtr<ABI::Windows::UI::Xaml::IFrameworkElement> fe;
        MS_ENSURE(inspectable_.As(&fe), false);
        return RunOnUIThread([=]{
            MS_ENSURE(fe->add_SizeChanged(h.Get(), &size_change_token_), false);
            return true;
        }, false); // usually successed, and return value is not used, so no wait
    }
    ComPtr<ABI::Windows::Foundation::Collections::IPropertySet> ps;
    if (SUCCEEDED(inspectable_.As(&ps)) && ps) {
        ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>> propMap;
        if (SUCCEEDED(ps.As(&propMap))) {
            boolean hasKey = false;
            const auto key = HStringReference(L"EGLNativeWindowTypeProperty");
            if (SUCCEEDED(propMap->HasKey(key.Get(), &hasKey)) && hasKey)
                MS_ENSURE(propMap->Lookup(key.Get(), &win_), false);
        }
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
        ComPtr<ABI::Windows::UI::Xaml::IFrameworkElement> fe;
        MS_ENSURE(panel_.As(&fe));
        RunOnUIThread([=]{
            fe->remove_SizeChanged(size_change_token_);
            size_change_token_.value = 0;
        }, false);
    }
}

PlatformSurface* create_winrt_surface() { return new WinRTSurface();}
UGS_NS_END
