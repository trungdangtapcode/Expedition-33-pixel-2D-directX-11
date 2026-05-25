// ============================================================
// File: ColorGradeFilter.cpp
// Responsibility: GPU resource creation and rendering for the overworld
//                 color grade post-process.
//
// Shader design:
//   VS - pass-through fullscreen quad in NDC.
//   PS - saturation, contrast, tint, brightness, and vignette adjustment.
//
// Render path:
//   1. Copy the current back buffer into mSceneTexture.
//   2. Upload current grade settings to a constant buffer.
//   3. Draw a fullscreen quad that samples mSceneSRV and writes back over
//      the current back buffer.
// ============================================================
#include "ColorGradeFilter.h"
#include "D3DContext.h"
#include "../Utils/Log.h"
#include <d3dcompiler.h>
#include <cstring>
#include <cmath>

namespace
{
    constexpr float kEpsilon = 0.0005f;

    bool NearlyEqual(float a, float b)
    {
        return std::fabs(a - b) <= kEpsilon;
    }
}

static const char* kColorGradeVS = R"hlsl(
struct VSIn  { float2 pos : POSITION; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VSOut main(VSIn v)
{
    VSOut o;
    o.pos = float4(v.pos, 0.0f, 1.0f);
    o.uv = float2(v.pos.x * 0.5f + 0.5f,
                 -v.pos.y * 0.5f + 0.5f);
    return o;
}
)hlsl";

static const char* kColorGradePS = R"hlsl(
cbuffer GradeCB : register(b0)
{
    float tintR;
    float tintG;
    float tintB;
    float tintStrength;
    float saturation;
    float contrast;
    float brightness;
    float vignetteStrength;
};

Texture2D    gScene   : register(t0);
SamplerState gSampler : register(s0);

float4 main(float4 svPos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float4 color = gScene.Sample(gSampler, uv);
    float3 rgb = color.rgb;

    float luminance = dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
    rgb = lerp(float3(luminance, luminance, luminance), rgb, saturation);

    rgb = (rgb - 0.5f) * contrast + 0.5f;
    rgb = lerp(rgb, rgb * float3(tintR, tintG, tintB), saturate(tintStrength));
    rgb += brightness;

    float2 centered = uv - 0.5f;
    float radial = saturate(length(centered) * 1.41421356f);
    float vignette = 1.0f - vignetteStrength * smoothstep(0.35f, 0.95f, radial);

    return float4(saturate(rgb * vignette), color.a);
}
)hlsl";

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Compile shaders and allocate the resources needed for fullscreen
//   color grading.
// Why:
//   The filter must own a copy texture because a D3D11 texture cannot be
//   safely sampled while it is also the active render target.
// Parameters:
//   device - D3D11 device used to allocate GPU objects.
//   width  - back-buffer width in pixels.
//   height - back-buffer height in pixels.
// ------------------------------------------------------------
bool ColorGradeFilter::Initialize(ID3D11Device* device, int width, int height)
{
    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;

    hr = D3DCompile(kColorGradeVS,
                    std::strlen(kColorGradeVS),
                    "ColorGradeVS",
                    nullptr,
                    nullptr,
                    "main",
                    "vs_5_0",
                    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
                    0,
                    vsBlob.GetAddressOf(),
                    errBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (errBlob)
        {
            LOG("[ColorGradeFilter] VS compile error: %s",
                static_cast<const char*>(errBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                    vsBlob->GetBufferSize(),
                                    nullptr,
                                    mVS.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateVertexShader failed (0x%08X).", hr);
        return false;
    }

    errBlob.Reset();
    hr = D3DCompile(kColorGradePS,
                    std::strlen(kColorGradePS),
                    "ColorGradePS",
                    nullptr,
                    nullptr,
                    "main",
                    "ps_5_0",
                    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
                    0,
                    psBlob.GetAddressOf(),
                    errBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (errBlob)
        {
            LOG("[ColorGradeFilter] PS compile error: %s",
                static_cast<const char*>(errBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                   psBlob->GetBufferSize(),
                                   nullptr,
                                   mPS.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreatePixelShader failed (0x%08X).", hr);
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = device->CreateInputLayout(layoutDesc,
                                   1,
                                   vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(),
                                   mInputLayout.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateInputLayout failed (0x%08X).", hr);
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = static_cast<UINT>(width);
    texDesc.Height = static_cast<UINT>(height);
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = device->CreateTexture2D(&texDesc, nullptr, mSceneTexture.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateTexture2D failed (0x%08X).", hr);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(mSceneTexture.Get(),
                                          &srvDesc,
                                          mSceneSRV.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateShaderResourceView failed (0x%08X).", hr);
        return false;
    }

    const Vertex quad[4] = {
        { -1.0f,  1.0f },
        {  1.0f,  1.0f },
        { -1.0f, -1.0f },
        {  1.0f, -1.0f }
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(quad);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = quad;

    hr = device->CreateBuffer(&vbDesc, &vbData, mQuadVB.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateBuffer(VB) failed (0x%08X).", hr);
        return false;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(GradeCB);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&cbDesc, nullptr, mConstantBuffer.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateBuffer(CB) failed (0x%08X).", hr);
        return false;
    }

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = device->CreateSamplerState(&samplerDesc, mSampler.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateSamplerState failed (0x%08X).", hr);
        return false;
    }

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;

    hr = device->CreateRasterizerState(&rasterDesc, mRasterState.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateRasterizerState failed (0x%08X).", hr);
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

    hr = device->CreateDepthStencilState(&depthDesc, mDepthOffState.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[ColorGradeFilter] CreateDepthStencilState failed (0x%08X).", hr);
        return false;
    }

    mInitialized = true;
    LOG("[ColorGradeFilter] Initialized (%dx%d).", width, height);
    return true;
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Release all GPU resources held by the filter.
// Why:
//   Explicit release before state exit avoids stale resources during device
//   resize and keeps DirectX debug-layer leak reports useful.
// ------------------------------------------------------------
void ColorGradeFilter::Shutdown()
{
    mSceneTexture.Reset();
    mSceneSRV.Reset();
    mVS.Reset();
    mPS.Reset();
    mInputLayout.Reset();
    mQuadVB.Reset();
    mConstantBuffer.Reset();
    mSampler.Reset();
    mRasterState.Reset();
    mDepthOffState.Reset();
    mInitialized = false;
    LOG("[ColorGradeFilter] Shutdown.");
}

void ColorGradeFilter::BeginCapture(ID3D11DeviceContext* ctx)
{
    (void)ctx;
}

void ColorGradeFilter::EndCapture(ID3D11DeviceContext* ctx)
{
    (void)ctx;
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Apply the configured color grade over the current back buffer.
// Why:
//   Copying the back buffer first lets the pixel shader sample the original
//   scene while writing the graded result back to the active render target.
// Parameters:
//   ctx - D3D11 context for this frame.
// Caveats:
//   - Call before UI rendering so HUD and text remain color-accurate.
// ------------------------------------------------------------
void ColorGradeFilter::Render(ID3D11DeviceContext* ctx)
{
    if (!mInitialized || !IsActive()) return;

    ID3D11RenderTargetView* rtv = D3DContext::Get().GetRTV();
    if (!rtv) return;

    Microsoft::WRL::ComPtr<ID3D11Resource> backBuffer;
    rtv->GetResource(backBuffer.GetAddressOf());
    if (!backBuffer) return;

    ctx->CopyResource(mSceneTexture.Get(), backBuffer.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(ctx->Map(mConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        GradeCB cb = {};
        cb.tintR = mSettings.tintR;
        cb.tintG = mSettings.tintG;
        cb.tintB = mSettings.tintB;
        cb.tintStrength = mSettings.tintStrength * mIntensity;
        cb.saturation = 1.0f + (mSettings.saturation - 1.0f) * mIntensity;
        cb.contrast = 1.0f + (mSettings.contrast - 1.0f) * mIntensity;
        cb.brightness = mSettings.brightness * mIntensity;
        cb.vignetteStrength = mSettings.vignetteStrength * mIntensity;

        std::memcpy(mapped.pData, &cb, sizeof(GradeCB));
        ctx->Unmap(mConstantBuffer.Get(), 0);
    }

    ctx->VSSetShader(mVS.Get(), nullptr, 0);
    ctx->PSSetShader(mPS.Get(), nullptr, 0);
    ctx->IASetInputLayout(mInputLayout.Get());

    ID3D11ShaderResourceView* srv = mSceneSRV.Get();
    ctx->PSSetShaderResources(0, 1, &srv);

    ID3D11SamplerState* sampler = mSampler.Get();
    ctx->PSSetSamplers(0, 1, &sampler);

    ID3D11Buffer* constantBuffer = mConstantBuffer.Get();
    ctx->PSSetConstantBuffers(0, 1, &constantBuffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D11Buffer* vertexBuffer = mQuadVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    ctx->RSSetState(mRasterState.Get());
    ctx->OMSetDepthStencilState(mDepthOffState.Get(), 0);
    ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    ctx->Draw(4, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);
    ctx->RSSetState(nullptr);
    ctx->OMSetDepthStencilState(nullptr, 0);
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Store the external filter intensity.
// Why:
//   Theme blending is handled by OverworldThemeManager, but the interface
//   supports animated intensity for future weather or cutscene effects.
// ------------------------------------------------------------
void ColorGradeFilter::Update(float dt, float intensity)
{
    (void)dt;
    mIntensity = intensity;
}

bool ColorGradeFilter::IsActive() const
{
    if (!mInitialized) return false;
    if (mIntensity <= kEpsilon) return false;

    return !NearlyEqual(mSettings.tintR, 1.0f)
        || !NearlyEqual(mSettings.tintG, 1.0f)
        || !NearlyEqual(mSettings.tintB, 1.0f)
        || !NearlyEqual(mSettings.tintStrength, 0.0f)
        || !NearlyEqual(mSettings.saturation, 1.0f)
        || !NearlyEqual(mSettings.contrast, 1.0f)
        || !NearlyEqual(mSettings.brightness, 0.0f)
        || !NearlyEqual(mSettings.vignetteStrength, 0.0f);
}

void ColorGradeFilter::SetSettings(const ColorGradeSettings& settings)
{
    mSettings = settings;
}
