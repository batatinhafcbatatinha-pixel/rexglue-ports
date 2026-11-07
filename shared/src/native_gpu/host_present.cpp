#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifndef _WIN32
#include "../platform/native_win32_posix.h"
#endif
#include <d3d11.h>
#include <dxgi1_2.h>
#include "../platform/native_shader_compiler.h"
#include "../platform/native_com_ptr.h"
#include <algorithm>
#include <stdexcept>
#include "host_present.h"

namespace pcs {
using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT result, const char* operation) {
  if (FAILED(result)) throw std::runtime_error(operation);
}
struct Presenter {
  ComPtr<ID3D11Device> owner;
  ComPtr<ID3D11VertexShader> vs;
  ComPtr<ID3D11PixelShader> ps;
  ComPtr<ID3D11Buffer> constants;
  ComPtr<ID3D11SamplerState> sampler;
  ComPtr<ID3D11RasterizerState> rasterizer;
  void Initialize(ID3D11Device* device) {
    if (owner.Get() == device) return;
    *this = Presenter{};
    owner = device;
    constexpr char source[] = R"(
cbuffer Frame : register(b0) { float2 uvMax; float swapRB; float unused; };
struct Vertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Vertex vs_main(uint id : SV_VertexID) {
  Vertex v;
  float2 uv = float2((id << 1) & 2, id & 2);
  v.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
  v.uv = uv * uvMax;
  return v;
}
Texture2D image : register(t0);
SamplerState linearClamp : register(s0);
float4 ps_main(Vertex v) : SV_Target {
  float4 color = image.Sample(linearClamp, v.uv);
  return swapRB != 0 ? color.bgra : color;
}
)";
    ComPtr<ID3DBlob> code, errors;
    Check(D3DCompile(source, sizeof(source), "native-present", nullptr, nullptr,
                     "vs_main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors), "Native present VS compilation failed");
    Check(device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &vs), "Native present VS creation failed");
    code.Reset(); errors.Reset();
    Check(D3DCompile(source, sizeof(source), "native-present", nullptr, nullptr,
                     "ps_main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &errors), "Native present PS compilation failed");
    Check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &ps), "Native present PS creation failed");
    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = 16; cb.Usage = D3D11_USAGE_DEFAULT; cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Check(device->CreateBuffer(&cb, nullptr, &constants), "Native present constants creation failed");
    D3D11_SAMPLER_DESC ss{};
    ss.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ss.MaxLOD = D3D11_FLOAT32_MAX;
    Check(device->CreateSamplerState(&ss, &sampler), "Native present sampler creation failed");
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID; rs.CullMode = D3D11_CULL_NONE; rs.DepthClipEnable = true;
    Check(device->CreateRasterizerState(&rs, &rasterizer), "Native present rasterizer creation failed");
  }
};
}

long CreateDevice(unsigned flags, ID3D11Device** device, ID3D11DeviceContext** context) {
  D3D_FEATURE_LEVEL level;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
  // Require actual hardware; no software rasterizer/WARP fallback.
  return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                          levels, 1, D3D11_SDK_VERSION, device, &level, context);
}

void Present(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain1* swap,
             const Picture& picture, const BackBufferFn& back_buffer) {
  static Presenter state;
  state.Initialize(device);
  ID3D11RenderTargetView* target = nullptr;
  uint32_t width = 0, height = 0;
  if (!back_buffer(&target, &width, &height)) throw std::runtime_error("Native backbuffer unavailable");
  const float black[] = {0, 0, 0, 1};
  context->ClearRenderTargetView(target, black);
  context->OMSetRenderTargets(1, &target, nullptr);
  if (picture.srv) {
    float scale = std::min(float(width) / std::max(picture.width, 1.0f),
                           float(height) / std::max(picture.height, 1.0f));
    D3D11_VIEWPORT viewport{};
    viewport.Width = picture.width * scale; viewport.Height = picture.height * scale;
    viewport.TopLeftX = (float(width) - viewport.Width) * 0.5f;
    viewport.TopLeftY = (float(height) - viewport.Height) * 0.5f;
    viewport.MaxDepth = 1;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(state.rasterizer.Get());
    float frame[] = {picture.u1, picture.v1, picture.swap_rb ? 1.0f : 0.0f, 0};
    context->UpdateSubresource(state.constants.Get(), 0, nullptr, frame, 0, 0);
    ID3D11Buffer* cb = state.constants.Get();
    ID3D11SamplerState* sampler = state.sampler.Get();
    context->VSSetShader(state.vs.Get(), nullptr, 0);
    context->PSSetShader(state.ps.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &cb);
    context->PSSetConstantBuffers(0, 1, &cb);
    context->PSSetSamplers(0, 1, &sampler);
    context->PSSetShaderResources(0, 1, &picture.srv);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->Draw(3, 0);
    ID3D11ShaderResourceView* null_srv = nullptr;
    context->PSSetShaderResources(0, 1, &null_srv);
  }
  Check(swap->Present(1, 0), "Native swapchain presentation failed");
}
}
