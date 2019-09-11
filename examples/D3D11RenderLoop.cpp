#include "D3D11RenderLoop.h"
#include "ugs/PlatformSurface.h"
#include <iostream>
#include <system_error>
#include <d3d11_1.h>
#include <windows.h>
#ifdef WINAPI_FAMILY
# include <winapifamily.h>
# if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   define MS_API_DESKTOP 1
# else
#   define MS_WINRT 1
#   define MS_API_APP 1
# endif
#else
# define MS_API_DESKTOP 1
#endif //WINAPI_FAMILY

// vista is required to build with wrl, but xp runtime works
#pragma push_macro("_WIN32_WINNT")
#if _WIN32_WINNT < _WIN32_WINNT_VISTA
# undef _WIN32_WINNT
# define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif
#include <wrl/client.h>
#if (_MSC_VER + 0) // missing headers in mingw
#include <wrl/implements.h> // RuntimeClass
#endif
#pragma pop_macro("_WIN32_WINNT")
using namespace Microsoft::WRL; //ComPtr

// TODO: define MS_ERROR_STR before including this header to handle module specific errors?
#define MS_ENSURE(f, ...) MS_CHECK(f, return __VA_ARGS__;)
#define MS_WARN(f) MS_CHECK(f)
#define MS_CHECK(f, ...)  do { \
        while (FAILED(GetLastError())) {} \
        HRESULT __ms_hr__ = f; \
        if (FAILED(__ms_hr__)) { \
            std::clog << #f "  ERROR@" << __LINE__ << __FUNCTION__ << ": (" << std::hex << __ms_hr__ << std::dec << ") " << std::error_code(__ms_hr__, std::system_category()).message() << std::endl << std::flush; \
            __VA_ARGS__ \
        } \
    } while (false)

class ContextD3D11Impl final : public ContextD3D11
{
public:
  ~ContextD3D11Impl() override {}

  ID3D11DeviceContext* deviceContext() const override {
    return immediate_ctx_.Get();
  }

  IDXGISwapChain* swapChain() const override {
    return swapchain_.Get();
  }

  bool initDevice()
  {
    UINT createDeviceFlags = 0;
//#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
//#endif
    D3D_FEATURE_LEVEL fl;
    /*!
    Note  If the Direct3D 11.1 runtime is present on the computer and pFeatureLevels is set to NULL, this function won't create a D3D_FEATURE_LEVEL_11_1 device. To create a D3D_FEATURE_LEVEL_11_1 device, you must explicitly provide a D3D_FEATURE_LEVEL array that includes D3D_FEATURE_LEVEL_11_1. If you provide a D3D_FEATURE_LEVEL array that contains D3D_FEATURE_LEVEL_11_1 on a computer that doesn't have the Direct3D 11.1 runtime installed, this function immediately fails with E_INVALIDARG.
    */
    D3D_FEATURE_LEVEL fls[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    if (D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, fls, std::size(fls),
      D3D11_SDK_VERSION, &dev_, &fl, &immediate_ctx_) == E_INVALIDARG)
      MS_ENSURE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, &fls[1], std::size(fls) -1,
        D3D11_SDK_VERSION, &dev_, &fl, &immediate_ctx_), false);
    ComPtr<ID3D10Multithread> mt;
    if (SUCCEEDED(dev_.As(&mt)))
      mt->SetMultithreadProtected(TRUE);

    return true;
  }

  bool initSwapChain(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;
    // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
    ComPtr<IDXGIDevice> dxgiDev;
    MS_ENSURE(dev_.As(&dxgiDev), false);
    ComPtr<IDXGIAdapter> adapter;
    MS_ENSURE(dxgiDev->GetAdapter(&adapter), false);
    ComPtr<IDXGIFactory1> factory1;
    MS_ENSURE(adapter->GetParent(IID_PPV_ARGS(&factory1)), false);

    ComPtr<IDXGIFactory2> factory2;
    if (SUCCEEDED(factory1.As(&factory2)) && factory2.Get()) {
      // DirectX 11.1 or later
      DXGI_SWAP_CHAIN_DESC1 sd = {};
      sd.Width = width;
      sd.Height = height;
      sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      sd.SampleDesc.Count = 1;
      sd.SampleDesc.Quality = 0;
      sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      sd.BufferCount = 1;
      //sd.Flags = DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
      ComPtr<IDXGISwapChain1> sc1;
      MS_ENSURE(factory2->CreateSwapChainForHwnd(dev_.Get(), hWnd, &sd, nullptr, nullptr, &sc1), false);
      MS_ENSURE(sc1.As(&swapchain_), false);
    } else {
      // DirectX 11.0 systems
      DXGI_SWAP_CHAIN_DESC sd = {};
      sd.BufferCount = 1;
      sd.BufferDesc.Width = width;
      sd.BufferDesc.Height = height;
      sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      sd.BufferDesc.RefreshRate.Numerator = 60;
      sd.BufferDesc.RefreshRate.Denominator = 1;
      sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      sd.OutputWindow = hWnd;
      sd.SampleDesc.Count = 1;
      sd.SampleDesc.Quality = 0;
      sd.Windowed = TRUE;

      MS_ENSURE(factory1->CreateSwapChain(dev_.Get(), &sd, &swapchain_), false);
    }
    // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
    factory1->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

    // Create a render target view
    ComPtr<ID3D11Texture2D> bb;
    MS_ENSURE(swapchain_->GetBuffer(0, IID_PPV_ARGS(&bb)), false);
    ComPtr<ID3D11RenderTargetView> rtv;
    MS_ENSURE(dev_->CreateRenderTargetView(bb.Get(), nullptr, &rtv), false);
    immediate_ctx_->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr); // why need GetAddressOf()? operator& wrong overload?
    rtv_ = rtv;
    ID3D11RenderTargetView* v = nullptr;
    immediate_ctx_->OMGetRenderTargets(1, &v, nullptr);
    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    immediate_ctx_->RSSetViewports(1, &vp);
    return true;
  }

private:
  ComPtr<ID3D11Device> dev_;
  ComPtr<ID3D11DeviceContext> immediate_ctx_; // GetImmediateContext
  ComPtr<ID3D11RenderTargetView> rtv_;
  ComPtr<IDXGISwapChain> swapchain_;
};

void* D3D11RenderLoop::createRenderContext(PlatformSurface* surface)
{
  if (!surface->nativeHandle())
    return nullptr;
  std::clog << "createRenderContext from surface " << surface << " with extra native res " << surface->nativeResource() << std::endl;
  auto d3d11ctx = new ContextD3D11Impl();
  if (!d3d11ctx->initDevice())
    return false;
  if (!d3d11ctx->initSwapChain(HWND(surface->nativeHandle()))) // TODO: resize
    return false;
  return d3d11ctx;
}

bool D3D11RenderLoop::destroyRenderContext(PlatformSurface* surface, void* ctx)
{
  auto d3d11ctx = static_cast<ContextD3D11*>(ctx);
  std::clog << "destroyRenderContext: " << d3d11ctx << std::endl;
  delete d3d11ctx;
  return true;
}

bool D3D11RenderLoop::activateRenderContext(PlatformSurface* surface, void* ctx)
{
  if (!ctx)
    return false;
  return true;
}

bool D3D11RenderLoop::submitRenderContext(PlatformSurface* surface, void* ctx)
{
  if (!ctx)
    return false;
  static_cast<ContextD3D11*>(ctx)->swapChain()->Present(0, 0);
  return true;
}