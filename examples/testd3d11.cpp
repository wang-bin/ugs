#include "D3D11RenderLoop.h"
#include "ugs/PlatformSurface.h"
#include <iostream>
#include <unordered_map>
#include <d3dcompiler.h>
#include <directxcolors.h>
#include <wrl/client.h>
using namespace Microsoft::WRL; //ComPtr
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


const char kVS[] = R"(
float4 VS( float4 Pos : POSITION ) : SV_POSITION
{
    return Pos;
}
)";

const char kPS[] = R"(
float4 PS( float4 Pos : SV_POSITION ) : SV_Target
{
    return float4( 1.0f, 1.0f, 0.0f, 1.0f );    // Yellow, with Alpha = 1
})";

ComPtr<ID3DBlob> CompileShader(const char* data, size_t size, LPCSTR szEntryPoint, LPCSTR szShaderModel)
{
  DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
  dwShaderFlags |= D3DCOMPILE_DEBUG;
  dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
  ComPtr<ID3DBlob> b;
  ComPtr<ID3DBlob> eb;
  MS_WARN(D3DCompile(data, size, nullptr, nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, &b, &eb));
  if (eb.Get())
    OutputDebugStringA(reinterpret_cast<const char*>(eb->GetBufferPointer()));
  return b;
}

bool build_shader(ID3D11DeviceContext* ctx, ID3D11VertexShader** vs, ID3D11PixelShader** ps)
{
  ComPtr<ID3D11Device> dev;
  ctx->GetDevice(&dev);
  auto blob = CompileShader(kVS, sizeof(kVS), "VS", "vs_4_0");
  MS_ENSURE(dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, vs), false);

  D3D11_INPUT_ELEMENT_DESC layout[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };
  ComPtr<ID3D11InputLayout> vl;
  MS_ENSURE(dev->CreateInputLayout(layout, std::extent_v<decltype(layout)>, blob->GetBufferPointer(), blob->GetBufferSize(), &vl), false);
  ctx->IASetInputLayout(vl.Get());

  blob = CompileShader(kPS, sizeof(kPS), "PS", "ps_4_0");
  MS_ENSURE(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ps), false);
  const float vb[] = {
    0.0f, 0.5f, 0.5f,
    0.5f, -0.5f, 0.5f,
    -0.5f, -0.5f, 0.5f,
  };
  D3D11_BUFFER_DESC bd = {};
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.ByteWidth = sizeof(vb);
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.CPUAccessFlags = 0;

  D3D11_SUBRESOURCE_DATA data = {};
  data.pSysMem = vb;
  ComPtr<ID3D11Buffer> b;
  MS_ENSURE(dev->CreateBuffer(&bd, &data, &b), false);
  UINT stride = sizeof(vb) / 3;
  UINT offset = 0;
  ctx->IASetVertexBuffers(0, 1, b.GetAddressOf(), &stride, &offset);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  return true;
}

int main(int argc, char* argv[])
{
  struct Shaders {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
  };
  std::unordered_map<RenderContext, Shaders> ctx_shader;
  D3D11RenderLoop loop;
  loop.onDraw([&](PlatformSurface*, RenderContext ctx) {
    float r = float(intptr_t(ctx) % 1000) / 1000.0f;
    auto d3d11ctx = reinterpret_cast<ContextD3D11*>(ctx);
    ID3D11RenderTargetView* rtv = nullptr;
    d3d11ctx->deviceContext()->OMGetRenderTargets(1, &rtv, nullptr);
    const FLOAT color[] = { r, 1.0f, 0, 1.0f };
    d3d11ctx->deviceContext()->ClearRenderTargetView(rtv, color);
    rtv->Release();
    auto& s = ctx_shader[ctx];
    d3d11ctx->deviceContext()->VSSetShader(s.vs.Get(), nullptr, 0);
    d3d11ctx->deviceContext()->PSSetShader(s.ps.Get(), nullptr, 0);
    d3d11ctx->deviceContext()->Draw(3, 0);
    return true;
  }).onResize([](PlatformSurface*, int w, int h, RenderContext ctx) {
    auto d3d11ctx = reinterpret_cast<ContextD3D11*>(ctx);
    // Setup the viewport. Requires swapchain is recreated
    D3D11_VIEWPORT vp{};
    vp.Width = (FLOAT)w;
    vp.Height = (FLOAT)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    d3d11ctx->deviceContext()->RSSetViewports(1, &vp);
    std::clog << "onResize" << std::endl;
  }).onContextCreated([&](PlatformSurface*, RenderContext ctx) {
    std::clog << "onContextCreated" << std::endl;
    auto d3d11ctx = reinterpret_cast<ContextD3D11*>(ctx);
    auto& s = ctx_shader[ctx];
    build_shader(d3d11ctx->deviceContext(), &s.vs, &s.ps);
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