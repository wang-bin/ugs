#include "D3D11RenderLoop.h"
#include "ugs/PlatformSurface.h"
#include <iostream>

int main(int argc, char* argv[])
{
  D3D11RenderLoop loop;
  loop.onDraw([](PlatformSurface* s, RenderContext ctx) {
    float r = float(intptr_t(s) % 1000) / 1000.0f;
    auto d3d11ctx = reinterpret_cast<ContextD3D11*>(ctx);
    ID3D11RenderTargetView* rtv = nullptr;
    d3d11ctx->deviceContext()->OMGetRenderTargets(1, &rtv, nullptr);
    const FLOAT color[] = { r, 1.0f, 0, 1.0f };
    d3d11ctx->deviceContext()->ClearRenderTargetView(rtv, color);
    rtv->Release();
    return true;
  }).onResize([](PlatformSurface*, int w, int h) {
      std::clog << "onResize" << std::endl;
  }).onContextCreated([](PlatformSurface*, RenderContext) {
  std::clog << "onContextCreated" << std::endl;
  }).onDestroyContext([](PlatformSurface*, RenderContext) {
    std::clog << "onDestroyContext" << std::endl;
  });

  auto surface = loop.add(PlatformSurface::create()).lock();
  surface->resize(640, 480);
  surface = loop.add(PlatformSurface::create()).lock();
  surface->resize(480, 320);
  loop.start();
  loop.update();
  loop.waitForStopped();
  return 0;
}