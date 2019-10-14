#include "D3D11RenderLoop.h"
#include "ugs/PlatformSurface.h"
#include <iostream>
#include <unordered_map>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <directxcolors.h>
#include <wrl/client.h>
#include <fstream>
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

using namespace DirectX;
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

const char kFxPacked[] = R"(
Texture2D tex : register(t0);
SamplerState samLinear : register(s0);

cbuffer cbChangeOnResize : register(b0)
{
    matrix uMPV;
};
//matrix colorMatrix : register(b1); //?

struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
  input.Pos.w = 1.0f;
  PS_INPUT output = (PS_INPUT)0;
  output.Pos = mul(uMPV, input.Pos);
  output.TexCoord = input.TexCoord;   
  return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
  float4 c = tex.Sample(samLinear, input.TexCoord);
  return c;//mul(colorMatrix, c);
})";

ComPtr<ID3DBlob> CompileShader(const char* data, size_t size, LPCSTR szEntryPoint, LPCSTR szShaderModel)
{
  DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS| D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
  dwShaderFlags |= D3DCOMPILE_DEBUG;
  dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
  dsShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
  ComPtr<ID3DBlob> b;
  ComPtr<ID3DBlob> eb;
  MS_WARN(D3DCompile(data, size, nullptr, nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, &b, &eb));
  if (eb.Get())
    OutputDebugStringA(reinterpret_cast<const char*>(eb->GetBufferPointer()));
  return b;
}

struct Shaders {
  ComPtr<ID3D11VertexShader> vs;
  ComPtr<ID3D11PixelShader> ps;
  ComPtr<ID3D11Buffer> mpv;
  int vertexCount;
};

bool build_shader(ID3D11DeviceContext* ctx, Shaders* s)
{
  ComPtr<ID3D11Device> dev;
  ctx->GetDevice(&dev);
  auto blob = CompileShader(kVS, sizeof(kVS), "VS", "vs_4_0");
  MS_ENSURE(dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &s->vs), false);

  D3D11_INPUT_ELEMENT_DESC layout[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };
  ComPtr<ID3D11InputLayout> vl;
  MS_ENSURE(dev->CreateInputLayout(layout, std::extent_v<decltype(layout)>, blob->GetBufferPointer(), blob->GetBufferSize(), &vl), false);
  ctx->IASetInputLayout(vl.Get());

  blob = CompileShader(kPS, sizeof(kPS), "PS", "ps_4_0");
  MS_ENSURE(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &s->ps), false);
  const float vb[] = {
    0, 0.5f, 0,
    0.5f, -0.5f, 0,
    -0.5f, -0.5f, 0,
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
  UINT stride = sizeof(float) *3;
  UINT offset = 0;
  ctx->IASetVertexBuffers(0, 1, b.GetAddressOf(), &stride, &offset);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  s->vertexCount = 3;
  return true;
}

constexpr int W = 512;
constexpr int H = 512;
static uint8_t rgba[W * H * 4];

void load_rgba()
{
  memset(rgba, 0xff, sizeof(rgba));
  std::ifstream img("Lenna512.rgba", std::ios::binary);
  if (img.is_open())
    img.read((char*)rgba, sizeof(rgba));
}

bool build_shader2(ID3D11DeviceContext* ctx, Shaders* s)
{
  ComPtr<ID3D11Device> dev;
  ctx->GetDevice(&dev);
  auto blob = CompileShader(kFxPacked, sizeof(kFxPacked), "VS", "vs_4_0"); // vs_4_0_level_9_3
  MS_ENSURE(dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &s->vs), false);

  D3D11_INPUT_ELEMENT_DESC layout[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };
  ComPtr<ID3D11InputLayout> vl;
  MS_ENSURE(dev->CreateInputLayout(layout, std::extent_v<decltype(layout)>, blob->GetBufferPointer(), blob->GetBufferSize(), &vl), false);
  ctx->IASetInputLayout(vl.Get()); //  input-assembler stage


  blob = CompileShader(kFxPacked, sizeof(kFxPacked), "PS", "ps_4_0");
  MS_ENSURE(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &s->ps), false);
  
  struct cbChangeOnResize
  {
    XMMATRIX Matrix = XMMatrixIdentity();
  } MPVData;

  D3D11_BUFFER_DESC bd = {};
  bd.Usage = D3D11_USAGE_DEFAULT;
  bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bd.CPUAccessFlags = 0;
  bd.ByteWidth = sizeof(cbChangeOnResize);
  
  ComPtr<ID3D11Buffer> uMPV;
  MS_ENSURE(dev->CreateBuffer(&bd, nullptr, &uMPV), false);
  ctx->UpdateSubresource(uMPV.Get(), 0, nullptr, &MPVData, 0, 0);
  ctx->VSSetConstantBuffers(0, 1, uMPV.GetAddressOf());

  struct TexMap
  {
    XMFLOAT3 Pos; // TODO: why 3 ? VS_INPUT? 4 is set in layout
    XMFLOAT2 Tex;
  } texMap[] = {
    {XMFLOAT3(-0.5, -0.5, 0), XMFLOAT2(0, 1)},
    {XMFLOAT3(-0.5, 0.5, 0), XMFLOAT2(0, 0)},
    {XMFLOAT3(0.5, -0.5, 0), XMFLOAT2(1, 1)},
    {XMFLOAT3(0.5, 0.5, 0), XMFLOAT2(1, 0)},
  };
  bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  bd.ByteWidth = sizeof(texMap);
  D3D11_SUBRESOURCE_DATA vbdata = {};
  vbdata.pSysMem = texMap;
  ComPtr<ID3D11Buffer> vb;
  MS_ENSURE(dev->CreateBuffer(&bd, &vbdata, &vb), false);
  UINT stride = sizeof(TexMap);
  UINT offset = 0;
  ctx->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

  load_rgba();
  D3D11_SUBRESOURCE_DATA texData{};
  texData.pSysMem = rgba;
  texData.SysMemPitch = W * 4;
  D3D11_TEXTURE2D_DESC texDesc{};
  texDesc.Width = W;
  texDesc.Height = H;
  texDesc.MipLevels = 1;
  texDesc.ArraySize = 1;
  texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  texDesc.SampleDesc.Count = 1;
  texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  texDesc.Usage = D3D11_USAGE_DEFAULT; // D3D11_USAGE_IMMUTABLE if has initial data and is used as src
  texDesc.CPUAccessFlags = 0; // read for download(bind is 0)
  
  ComPtr<ID3D11Texture2D> tex;
  MS_ENSURE(dev->CreateTexture2D(&texDesc, &texData, &tex), false);

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = texDesc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;
  ComPtr<ID3D11ShaderResourceView> srv;
  MS_ENSURE(dev->CreateShaderResourceView(tex.Get(), &srvDesc, &srv), false);
  // rtv if use as output

  D3D11_SAMPLER_DESC sampDesc{};
  sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
  sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
  sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
  //sampDesc.MaxAnisotropy = 1;
  ComPtr<ID3D11SamplerState> samp;
  MS_ENSURE(dev->CreateSamplerState(&sampDesc, &samp), false);

  ctx->PSSetShaderResources(0, 1, srv.GetAddressOf());
  ctx->PSSetSamplers(0, 1, samp.GetAddressOf());

  s->mpv = uMPV;
  s->vertexCount = std::size(texMap);
  return true;
}

int main(int argc, char* argv[])
{
  std::unordered_map<RenderContext, Shaders> ctx_shader;
  D3D11RenderLoop loop;
  loop.onDraw([&](PlatformSurface*, RenderContext ctx) {
    float r = float(intptr_t(ctx) % 1000) / 1000.0f;
    auto d3d11ctx = reinterpret_cast<ContextD3D11*>(ctx);
    /*ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    d3d11ctx->deviceContext()->OMGetRenderTargets(std::size(rtvs), rtvs, nullptr);
    auto rtv = rtvs[0]; // can be null if buffers>1
    */
    auto rtv = d3d11ctx->renderTargetView();
    if (!rtv)
      return true;
    d3d11ctx->deviceContext()->OMSetRenderTargets(1, &rtv, nullptr); // why required if buffers>1?
    const FLOAT color[] = { r, 1.0f, 0, 1.0f };
    d3d11ctx->deviceContext()->ClearRenderTargetView(rtv, color);
    //rtv->Release();
    auto& s = ctx_shader[ctx];
    d3d11ctx->deviceContext()->VSSetShader(s.vs.Get(), nullptr, 0);
    d3d11ctx->deviceContext()->PSSetShader(s.ps.Get(), nullptr, 0);
    d3d11ctx->deviceContext()->Draw(s.vertexCount, 0);
    d3d11ctx->deviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
    return true;
  }).onResize([&](PlatformSurface*, int w, int h, RenderContext ctx) {
    std::clog << "onResize" << std::endl;
    if (w == 0 || h == 0)
      return;
    auto d3d11ctx = reinterpret_cast<ContextD3D11*>(ctx);
    d3d11ctx->resizeBuffers(w, h);
    // Setup the viewport. Requires swapchain is recreated
    D3D11_VIEWPORT vp{};
    vp.Width = (FLOAT)w;
    vp.Height = (FLOAT)h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    d3d11ctx->deviceContext()->RSSetViewports(1, &vp);

    auto& s = ctx_shader[ctx];
    if (!s.mpv.Get())
      return;
  }).onContextCreated([&](PlatformSurface*, RenderContext ctx) {
    std::clog << "onContextCreated" << std::endl;
    auto d3d11ctx = reinterpret_cast<ContextD3D11*>(ctx);
    auto& s = ctx_shader[ctx];
    static int count = 0;
    if (count++ % 2)
      build_shader(d3d11ctx->deviceContext(), &s);
    else
      build_shader2(d3d11ctx->deviceContext(), &s);

  }).onDestroyContext([](PlatformSurface*, RenderContext) {
    std::clog << "onDestroyContext" << std::endl;
  });

  auto surface = loop.add(PlatformSurface::create()).lock();
  surface->resize(128, 128);
  surface = loop.add(PlatformSurface::create()).lock();
  surface->resize(480, 320);
  loop.start();
  loop.update();
  loop.waitForStopped();
  return 0;
}