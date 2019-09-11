#pragma once
#include "ugs/RenderLoop.h"
#include <d3d11.h>

using namespace UGS_NS;
class D3D11RenderLoop final : public RenderLoop
{
protected:
  void* createRenderContext(PlatformSurface* surface) override;
  bool destroyRenderContext(PlatformSurface* surface, void* ctx) override;
  bool activateRenderContext(PlatformSurface* surface, void* ctx) override;
  bool submitRenderContext(PlatformSurface* surface, void* ctx) override;
};

class ContextD3D11
{
public:
  virtual ~ContextD3D11() = default;
  virtual ID3D11DeviceContext* deviceContext() const = 0;
  virtual IDXGISwapChain* swapChain() const = 0;
};