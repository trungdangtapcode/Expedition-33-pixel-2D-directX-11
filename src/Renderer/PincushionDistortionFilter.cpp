// ============================================================
// File: PincushionDistortionFilter.cpp
// Responsibility: GPU resources and per-frame logic for pincushion distortion.
//
// Shader design:
//   VS — pass-through NDC quad (same pattern as IrisTransitionRenderer).
//   PS — re-center UV, apply k*r^2 radial warp, sample scene texture.
//
// Offscreen capture design:
//   BeginCapture: saves back-buffer RTV/DSV, binds mSceneRTV + same DSV.
//   Scene renders normally into mSceneTexture.
//   EndCapture:   restores back-buffer RTV/DSV.
//   Render:       fullscreen quad sampling mSceneSRV with k applied.
//
// Constant buffer update:
//   mCurrentIntensity (set by Update) maps to k = intensity * kMaxCoefficient.
//   k=0 → identity (pass-through), k=kMaxCoefficient → full warp.
//
// Lifetime:
//   Initialize() / Shutdown() called once in owning state OnEnter/OnExit.
// ============================================================
#include "PincushionDistortionFilter.h"
#include "D3DContext.h"
#include "../Utils/Log.h"
#include <d3dcompiler.h>

// ============================================================
// Embedded HLSL — Vertex Shader
//
// A simple pass-through VS for the fullscreen NDC quad.
// Outputs UV coordinates (0..1) for the PS to sample the scene texture.
// ============================================================
static const char* kPincushionVS = R"hlsl(
struct VSIn  { float2 pos : POSITION; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VSOut main(VSIn v)
{
    VSOut o;
    o.pos = float4(v.pos, 0.0f, 1.0f);
    // Map NDC [-1,+1] to UV [0,1].  NDC +Y is up, UV +V is down — flip Y.
    o.uv  = float2(v.pos.x * 0.5f + 0.5f,
                  -v.pos.y * 0.5f + 0.5f);
    return o;
}
)hlsl";

// ============================================================
// Embedded HLSL — Pixel Shader
//
// Algorithm:
//   1. Re-center UV to [-0.5, +0.5] so distortion is symmetric about center.
//   2. r2 = dot(centered_uv, centered_uv) — squared distance from center.
//   3. warp = centered_uv * (1 + k * r2) — radial stretch.
//      k > 0: pincushion (corners stretch outward; center is unaffected).
//   4. Re-map back to [0,1] and sample the scene texture.
//      The CLAMP sampler fills OOB pixels with the nearest edge color.
//
// Why r2 not r?
//   r2 gives a smoother, more natural-looking distortion that matches real
//   lens pincushion behavior and feels less "cheap" than a linear falloff.
// ============================================================
static const char* kPincushionPS = R"hlsl(
cbuffer FilterCB : register(b0)
{
    float k;       // pincushion coefficient: 0=none, 0.6=strong
    float pad0;
    float pad1;
    float pad2;
};

Texture2D    gScene   : register(t0);
SamplerState gSampler : register(s0);

float4 main(float4 svPos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    // Re-center UV to [-0.5, +0.5] so the warp is symmetric.
    float2 centered = uv - 0.5f;

    // Squared radial distance from center.
    float r2 = dot(centered, centered);

    // Pincushion warp: stretch UV radially outward proportional to r2.
    // At center (r2=0): no warp. At corners (r2≈0.5): maximum warp.
    // float2 warped = centered * (1.0f + k * r2) + 0.5f;
    float zoom = 1.0f - k * 0.5f;
    float2 warped = centered * (1.0f + k * r2) * zoom + 0.5f;

    // Sample the captured scene.  The CLAMP sampler handles out-of-range UVs
    // gracefully — pixels outside the original image show the edge color.
    return gScene.Sample(gSampler, warped);
}
)hlsl";

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Create all GPU resources for the pincushion filter:
//     1. Compile VS and PS.
//     2. Create the offscreen render target texture + RTV + SRV.
//     3. Create the static fullscreen quad VB.
//     4. Create the dynamic constant buffer for k.
//     5. Create sampler (CLAMP), rasterizer (cull-none), depth-off state.
//
// Why create an offscreen texture the same size as the back buffer?
//   The scene must be captured at full resolution so the PS can sample
//   every pixel without quality loss.  Smaller textures would blur the
//   scene before distortion, which is not the intended effect.
// ------------------------------------------------------------
bool PincushionDistortionFilter::Initialize(ID3D11Device* device, int width, int height)
{
    HRESULT hr;

    // -----------------------------------------------------------------
    // Step 1 — Compile shaders
    // -----------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

    hr = D3DCompile(kPincushionVS, strlen(kPincushionVS), "PincushionVS",
                    nullptr, nullptr, "main", "vs_5_0",
                    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                    vsBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) LOG("[PincushionFilter] VS compile error: %s",
                         (char*)errBlob->GetBufferPointer());
        return false;
    }
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
             vsBlob->GetBufferSize(), nullptr, mVS.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateVertexShader failed (0x%08X)", hr); return false; }

    hr = D3DCompile(kPincushionPS, strlen(kPincushionPS), "PincushionPS",
                    nullptr, nullptr, "main", "ps_5_0",
                    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                    psBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) LOG("[PincushionFilter] PS compile error: %s",
                         (char*)errBlob->GetBufferPointer());
        return false;
    }
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
             psBlob->GetBufferSize(), nullptr, mPS.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreatePixelShader failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 2 — Input layout: POSITION float2 (NDC)
    // -----------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(layoutDesc, 1,
             vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
             mInputLayout.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateInputLayout failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 3 — Offscreen render target texture (same size as back buffer)
    // -----------------------------------------------------------------
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width            = static_cast<UINT>(width);
    texDesc.Height           = static_cast<UINT>(height);
    texDesc.MipLevels        = 1;
    texDesc.ArraySize        = 1;
    texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage            = D3D11_USAGE_DEFAULT;
    // Bind as both RTV (scene renders into it) and SRV (PS samples from it).
    // These two bind flags cannot be active on the same resource simultaneously —
    // BeginCapture binds as RTV; EndCapture unbinds; Render binds as SRV.
    texDesc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    hr = device->CreateTexture2D(&texDesc, nullptr, mSceneTexture.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateTexture2D failed (0x%08X)", hr); return false; }

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    hr = device->CreateRenderTargetView(mSceneTexture.Get(), &rtvDesc,
             mSceneRTV.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateRenderTargetView failed (0x%08X)", hr); return false; }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    hr = device->CreateShaderResourceView(mSceneTexture.Get(), &srvDesc,
             mSceneSRV.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateShaderResourceView failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 4 — Fullscreen quad VB (static NDC quad)
    //   TriangleStrip: v0-v1-v2 (upper-left triangle), v1-v3-v2 (lower-right).
    // -----------------------------------------------------------------
    Vertex quad[4] = {
        { -1.0f,  1.0f },   // top-left
        {  1.0f,  1.0f },   // top-right
        { -1.0f, -1.0f },   // bottom-left
        {  1.0f, -1.0f },   // bottom-right
    };
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth  = sizeof(quad);
    vbDesc.Usage      = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags  = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = { quad };
    hr = device->CreateBuffer(&vbDesc, &vbData, mQuadVB.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateBuffer (VB) failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 5 — Constant buffer for distortion coefficient k
    // -----------------------------------------------------------------
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(FilterCB);          // 16 bytes exactly
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // updated every active frame
    hr = device->CreateBuffer(&cbDesc, nullptr, mConstantBuffer.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateBuffer (CB) failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 6 — Sampler (CLAMP so out-of-range UVs don't tile/wrap)
    // -----------------------------------------------------------------
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&sampDesc, mSampler.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateSamplerState failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 7 — Rasterizer (cull-none, fullscreen)
    // -----------------------------------------------------------------
    D3D11_RASTERIZER_DESC rasDesc = {};
    rasDesc.FillMode = D3D11_FILL_SOLID;
    rasDesc.CullMode = D3D11_CULL_NONE;  // no winding order requirement
    hr = device->CreateRasterizerState(&rasDesc, mRasterState.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateRasterizerState failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 8 — Depth-stencil state (OFF) for the fullscreen draw
    //   The distorted image is drawn over everything; depth is irrelevant.
    // -----------------------------------------------------------------
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = FALSE;    // no depth test during fullscreen blit
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    hr = device->CreateDepthStencilState(&dsDesc, mDepthOffState.GetAddressOf());
    if (FAILED(hr)) { LOG("[PincushionFilter] CreateDepthStencilState failed (0x%08X)", hr); return false; }

    mInitialized = true;
    LOG("[PincushionFilter] Initialized (%dx%d).", width, height);
    return true;
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose: Release all GPU resources.  ComPtr members auto-release via
//   their destructors; this method just clears the mInitialized flag.
// ------------------------------------------------------------
void PincushionDistortionFilter::Shutdown()
{
    mSceneTexture.Reset();
    mSceneRTV.Reset();
    mSceneSRV.Reset();
    mVS.Reset();
    mPS.Reset();
    mInputLayout.Reset();
    mQuadVB.Reset();
    mConstantBuffer.Reset();
    mSampler.Reset();
    mRasterState.Reset();
    mDepthOffState.Reset();
    mSavedRTV.Reset();
    mSavedDSV.Reset();
    mInitialized = false;
    LOG("[PincushionFilter] Shutdown.");
}

// ------------------------------------------------------------
// Function: BeginCapture
// Purpose:
//   No longer redirects render targets! We now use a pure post-process
//   CopyResource approach in Render() to avoid D3D11 state hazards
//   with SpriteBatch and depth stencils.
// ------------------------------------------------------------
void PincushionDistortionFilter::BeginCapture(ID3D11DeviceContext* ctx)
{
    // Do nothing. Scene draws normally to the back buffer.
}

// ------------------------------------------------------------
// Function: EndCapture
// Purpose:
//   No longer needed since we didn't change the render target.
// ------------------------------------------------------------
void PincushionDistortionFilter::EndCapture(ID3D11DeviceContext* ctx)
{
    // Do nothing.
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   1. Copy the current back buffer directly to mSceneTexture.
//   2. Draw the fullscreen quad with the pincushion PS over the back buffer,
//      sampling from the copy we just made.
// ------------------------------------------------------------
void PincushionDistortionFilter::Render(ID3D11DeviceContext* ctx)
{
    if (!mInitialized || !IsActive()) return;

    // --- STEP 1: Copy back buffer to our shader resource ---
    ID3D11RenderTargetView* rtv = D3DContext::Get().GetRTV();
    if (!rtv) return;

    Microsoft::WRL::ComPtr<ID3D11Resource> backBuffer;
    rtv->GetResource(backBuffer.GetAddressOf());
    
    // Copy identical-format back buffer straight into our texture
    ctx->CopyResource(mSceneTexture.Get(), backBuffer.Get());

    // --- STEP 2: Draw warped quad over the back buffer ---

    // Upload distortion coefficient k to the constant buffer.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(ctx->Map(mConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        FilterCB cb;
        cb.k    = mCurrentIntensity * kMaxCoefficient;
        cb.pad0 = cb.pad1 = cb.pad2 = 0.0f;
        memcpy(mapped.pData, &cb, sizeof(FilterCB));
        ctx->Unmap(mConstantBuffer.Get(), 0);
    }

    // Set the pincushion shader pipeline.
    ctx->VSSetShader(mVS.Get(), nullptr, 0);
    ctx->PSSetShader(mPS.Get(), nullptr, 0);
    ctx->IASetInputLayout(mInputLayout.Get());

    // Bind the captured scene as shader resource.
    // IMPORTANT: the texture cannot be simultaneously bound as RTV (output)
    // and SRV (input).  EndCapture() must have been called first.
    ID3D11ShaderResourceView* srv = mSceneSRV.Get();
    ctx->PSSetShaderResources(0, 1, &srv);

    ID3D11SamplerState* samp = mSampler.Get();
    ctx->PSSetSamplers(0, 1, &samp);

    ID3D11Buffer* cb = mConstantBuffer.Get();
    ctx->PSSetConstantBuffers(0, 1, &cb);

    // Bind the fullscreen quad.
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ID3D11Buffer* vb = mQuadVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Override render states for the fullscreen blit.
    ctx->RSSetState(mRasterState.Get());
    ctx->OMSetDepthStencilState(mDepthOffState.Get(), 0);

    // Disable alpha blending so the warped image completely overwrites the original back buffer.
    ctx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    // Draw the quad — 4 vertices → 2 triangles covering the full screen.
    ctx->Draw(4, 0);

    // Unbind SRV after draw so the texture can be reused as RTV next frame.
    // Leaving an SRV bound while the same resource is bound as RTV triggers
    // a D3D11 debug warning and may produce undefined behavior on some drivers.
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);

    // Restore default rasterizer and depth states.
    ctx->RSSetState(nullptr);
    ctx->OMSetDepthStencilState(nullptr, 0);
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Store the caller-provided intensity for use in the next Render() call.
//   The intensity is already animated by the owning state (e.g., ramped up
//   over 0.6s in OverworldState's battle trigger sequence).
//   This method accepts it and maps it to k in Render().
//
// Parameters:
//   dt        — frame delta-time (unused by this filter; provided for interface compliance)
//   intensity — [0,1] distortion strength set by the owning state
// ------------------------------------------------------------
void PincushionDistortionFilter::Update(float dt, float intensity)
{
    (void)dt;   // not used — intensity ramp is owned by the caller state
    mCurrentIntensity = intensity;
}

// ------------------------------------------------------------
// Function: IsActive
// Purpose:
//   Return false when intensity is below the activation threshold so the
//   owning state can skip BeginCapture/EndCapture on frames with no effect.
//   This avoids the overhead of an offscreen RT bind + clear every frame
//   when pincushion is at rest (intensity = 0).
// ------------------------------------------------------------
bool PincushionDistortionFilter::IsActive() const
{
    return mInitialized && (mCurrentIntensity > kActivationThreshold);
}
