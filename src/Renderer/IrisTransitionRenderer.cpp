// ============================================================
// File: IrisTransitionRenderer.cpp
// Responsibility: Compile shaders + manage GPU resources for iris overlay.
//
// Architecture decision:
//   Shaders are embedded as raw string literals and compiled at runtime via
//   D3DCompile() — the same pattern as CircleRenderer.cpp.
//   For production, prefer offline FXC/DXC compilation to .cso blobs.
//
// Shader design:
//   VS: outputs a static full-screen NDC quad (no vertex math).
//   PS: SDF iris mask.
//     dist  = length(pixel_pos - center)
//     alpha = smoothstep(radius - softEdge, radius + softEdge, dist)
//     → alpha=0 inside the iris (transparent — game visible)
//     → alpha=1 outside the iris (opaque black — masks the scene)
//   This is the INVERSE of CircleRenderer's PS: here the inside is
//   transparent and the outside is opaque, producing the iris hole.
//
// Blend state:
//   SRC_ALPHA / INV_SRC_ALPHA (standard painter's blend).
//   The black overlay (RGB=0) composites over whatever was drawn before.
//
// Depth / Rasterizer state:
//   Depth writes and test are disabled — the overlay always renders on top.
//   Rasterizer: CullNone + no scissor — covers the full screen.
//
// Lifetime:
//   Initialize() → one-time GPU resource creation.
//   Shutdown()   → ComPtr members release automatically; explicit Shutdown()
//                  call resets mInitialized to prevent stale Render() calls.
//
// Common mistakes:
//   1. Struct IrisCB size not a multiple of 16 bytes → GPU reads garbage.
//      Four floats × 4 bytes = 16 bytes.  Always verify sizeof(IrisCB) == 16.
//   2. Forgetting to reset depth-stencil state after Render() →
//      other renderers that rely on depth testing break.
//      We save/restore the previous DSS in Render().
//   3. Calling the StartClose callback directly inside a state pop sequence
//      → use-after-free.  The callback only sets a bool flag.
// ============================================================
#include "IrisTransitionRenderer.h"
#include <d3dcompiler.h>
#include "../Utils/Log.h"
#include "../Utils/HrCheck.h"
#include <algorithm>

// ============================================================
// Embedded HLSL — Vertex Shader
//
// The quad vertices are stored in a static VB at NDC coordinates.
// The VS just passes them through — no world/view/projection transforms.
// SV_Position output is in NDC (not screen pixels); the PS uses
// SV_Position which the rasterizer converts to screen-space pixels.
// ============================================================
static const char* kIrisVS = R"hlsl(
struct VSIn  { float2 pos : POSITION; };
struct VSOut { float4 pos : SV_Position; };

VSOut main(VSIn v)
{
    VSOut o;
    // Pass NDC position straight through.  z=0 (no depth test), w=1.
    o.pos = float4(v.pos, 0.0f, 1.0f);
    return o;
}
)hlsl";

// ============================================================
// Embedded HLSL — Pixel Shader
//
// Algorithm: SDF iris mask (INVERTED circle).
//   dist  = Euclidean distance from the pixel to the iris center.
//   alpha = smoothstep(radius - softEdge, radius + softEdge, dist)
//
//   dist < radius - softEdge  → alpha = 0  (fully transparent: inside iris hole)
//   dist > radius + softEdge  → alpha = 1  (fully opaque black: outside iris)
//   between                   → smooth blend (feathered / soft edge)
//
// This is the OPPOSITE of CircleRenderer (which draws the filled disk).
// Output: black pixel whose transparency is controlled by the iris radius.
//
// Why smoothstep instead of a hard step?
//   smoothstep produces a smooth cubic Hermite curve between the inner
//   and outer edge, giving the soft "vignette" edge requested by design.
//   kSoftEdge = 24 pixels produces a visible but not overly blurry edge.
// ============================================================
static const char* kIrisPS = R"hlsl(
cbuffer IrisCB : register(b0)
{
    float centerX;    // iris center — screen pixels (top-left origin)
    float centerY;
    float radius;     // current opening radius in screen pixels
    float softEdge;   // feather half-width in pixels
};

float4 main(float4 svPos : SV_Position) : SV_Target
{
    // SV_Position.xy is the pixel center in screen space (top-left origin, +Y down).
    float dist  = length(svPos.xy - float2(centerX, centerY));

    // alpha: 0 = inside hole (transparent), 1 = outside iris (black).
    // smoothstep([edge0], [edge1], x): 0 when x <= edge0, 1 when x >= edge1.
    float alpha = smoothstep(radius - softEdge, radius + softEdge, dist);

    return float4(0.0f, 0.0f, 0.0f, alpha);
}
)hlsl";

// ------------------------------------------------------------
// Initialize
// Purpose:
//   1. Compile VS + PS.
//   2. Create the static fullscreen quad VB.
//   3. Create the dynamic constant buffer (radius changes every frame).
//   4. Create alpha blend state, depth-off state, and cull-none rasterizer.
// Why cull-none?
//   The fullscreen quad is drawn from the front; CullNone avoids any
//   winding-order issue if the quad vertices are accidentally reversed.
// ------------------------------------------------------------
bool IrisTransitionRenderer::Initialize(ID3D11Device* device, int screenW, int screenH)
{
    HRESULT hr;

    // Store screen dimensions for default center and max-radius calculation.
    mCenterX   = static_cast<float>(screenW) * 0.5f;
    mCenterY   = static_cast<float>(screenH) * 0.5f;

    // mMaxRadius: the distance from the center to the farthest screen corner.
    // When radius >= mMaxRadius the circle entirely covers the render target —
    // the iris is fully open and the overlay is invisible everywhere.
    const float hw = static_cast<float>(screenW) * 0.5f;
    const float hh = static_cast<float>(screenH) * 0.5f;
    mMaxRadius = std::sqrtf(hw * hw + hh * hh) + kSoftEdge;  // +softEdge so edge fades fully

    // Start fully closed (screen is black).
    // The owning state must call StartOpen() to reveal the scene.
    // This ensures every state transition starts from a black screen and
    // expands outward — the classic JRPG circle-wipe reveal.
    mRadius = 0.0f;
    mPhase  = IrisPhase::CLOSED;

    // -----------------------------------------------------------------
    // Step 1 — Compile shaders
    // -----------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    hr = D3DCompile(kIrisVS, strlen(kIrisVS), "IrisVS",
                    nullptr, nullptr, "main", "vs_5_0",
                    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                    vsBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        const char* msg = errorBlob ? (const char*)errorBlob->GetBufferPointer() : "(none)";
        LOG("[IrisTransitionRenderer] VS compile failed: %s", msg);
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                    vsBlob->GetBufferSize(), nullptr, mVS.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] CreateVertexShader failed (0x%08X)", hr); return false; }

    hr = D3DCompile(kIrisPS, strlen(kIrisPS), "IrisPS",
                    nullptr, nullptr, "main", "ps_5_0",
                    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
                    psBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        const char* msg = errorBlob ? (const char*)errorBlob->GetBufferPointer() : "(none)";
        LOG("[IrisTransitionRenderer] PS compile failed: %s", msg);
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                   psBlob->GetBufferSize(), nullptr, mPS.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] CreatePixelShader failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 2 — Input layout: one POSITION (float2 NDC)
    // -----------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = device->CreateInputLayout(layoutDesc, 1,
                                   vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                   mInputLayout.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] CreateInputLayout failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 3 — Full-screen quad VB (static, never changes)
    // NDC: x in [-1,+1], y in [-1,+1].  D3D11 NDC has +Y up.
    // TriangleStrip topology: v0-v1-v2 then v1-v3-v2 covers the whole screen.
    //   v0(-1,+1) ---- v1(+1,+1)   (top row)
    //      |               |
    //   v2(-1,-1) ---- v3(+1,-1)   (bottom row)
    // -----------------------------------------------------------------
    Vertex quad[4] = {
        { -1.0f,  1.0f },  // top-left     (NDC)
        {  1.0f,  1.0f },  // top-right
        { -1.0f, -1.0f },  // bottom-left
        {  1.0f, -1.0f },  // bottom-right
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth  = sizeof(quad);
    vbDesc.Usage      = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags  = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = { quad };
    hr = device->CreateBuffer(&vbDesc, &vbData, mQuadVB.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] VB creation failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 4 — Constant buffer (dynamic — updated every Render() call)
    // sizeof(IrisCB) = 4 * sizeof(float) = 16 bytes (multiple of 16 ✓).
    // -----------------------------------------------------------------
    static_assert(sizeof(IrisCB) == 16, "IrisCB must be 16 bytes (D3D11 CB alignment).");
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(IrisCB);
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&cbDesc, nullptr, mConstantBuffer.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] CB creation failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 5 — Alpha blend state
    // SRC_ALPHA / INV_SRC_ALPHA: the black overlay blends over the scene.
    // Without this the iris edges look like a harsh pixel-art rectangle.
    // -----------------------------------------------------------------
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable           = TRUE;
    blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = device->CreateBlendState(&blendDesc, mBlendState.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] BlendState creation failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 6 — Depth-stencil state: depth OFF.
    // The overlay must always be drawn on top of everything; disabling the
    // depth test prevents it from being occluded by scene geometry.
    // -----------------------------------------------------------------
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = FALSE;   // no depth test or write
    dsDesc.StencilEnable  = FALSE;
    hr = device->CreateDepthStencilState(&dsDesc, mDepthState.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] DSS creation failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 7 — Rasterizer: CullNone, no scissor.
    // CullNone ensures the full-screen quad draws regardless of winding order.
    // -----------------------------------------------------------------
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    hr = device->CreateRasterizerState(&rsDesc, mRasterState.GetAddressOf());
    if (FAILED(hr)) { LOG("[IrisTransitionRenderer] RasterizerState creation failed (0x%08X)", hr); return false; }

    mInitialized = true;
    LOG("[IrisTransitionRenderer] Initialized. maxRadius=%.1f center=(%.1f,%.1f)",
        mMaxRadius, mCenterX, mCenterY);
    return true;
}

// ------------------------------------------------------------
// Shutdown
// ------------------------------------------------------------
void IrisTransitionRenderer::Shutdown()
{
    // ComPtr members release automatically on Reset().
    mVS.Reset();
    mPS.Reset();
    mQuadVB.Reset();
    mConstantBuffer.Reset();
    mInputLayout.Reset();
    mBlendState.Reset();
    mDepthState.Reset();
    mRasterState.Reset();
    mOnClosed = nullptr;
    mInitialized = false;
}

// ------------------------------------------------------------
// StartClose
// Purpose:
//   Transition the iris from its current radius toward 0.
//   The onClosed callback fires exactly once when radius reaches 0,
//   then mPhase switches to CLOSED.
// Caveats:
//   - Calling StartClose while already CLOSING replaces the speed and
//     callback — this is intentional for mid-transition retriggering.
//   - If already CLOSED, the callback fires on the next Update() tick.
// ------------------------------------------------------------
void IrisTransitionRenderer::StartClose(std::function<void()> onClosed, float speed)
{
    mOnClosed = std::move(onClosed);
    mSpeed    = speed;
    mPhase    = IrisPhase::CLOSING;
    LOG("[IrisTransitionRenderer] StartClose: speed=%.0f px/s", mSpeed);
}

// ------------------------------------------------------------
// StartOpen
// Purpose:
//   Transition the iris from its current radius toward mMaxRadius.
//   When mMaxRadius is reached, mPhase transitions to IDLE.
// ------------------------------------------------------------
void IrisTransitionRenderer::StartOpen(float speed)
{
    mSpeed = speed;
    mPhase = IrisPhase::OPENING;
    LOG("[IrisTransitionRenderer] StartOpen: speed=%.0f px/s", mSpeed);
}

// ------------------------------------------------------------
// Update
// Purpose:
//   Advance mRadius by mSpeed * dt toward the target.
//   Fire the onClosed callback when CLOSING reaches 0.
//   Transition to IDLE when OPENING reaches mMaxRadius.
// Why no lerp — use linear step instead?
//   Linear motion at a fixed pixels/sec gives the designer a predictable
//   duration regardless of starting radius.  A lerp would feel different
//   depending on when StartClose is called.
// ------------------------------------------------------------
void IrisTransitionRenderer::Update(float dt)
{
    if (mPhase == IrisPhase::CLOSING)
    {
        mRadius -= mSpeed * dt;
        if (mRadius <= 0.0f)
        {
            mRadius = 0.0f;
            mPhase  = IrisPhase::CLOSED;

            // Fire the callback exactly once.  Clear it immediately so a
            // subsequent StartClose with no callback does not re-fire a stale one.
            if (mOnClosed)
            {
                auto cb = std::move(mOnClosed);
                mOnClosed = nullptr;
                cb();  // callback may set a deferred-pop flag on the owning state
            }
        }
    }
    else if (mPhase == IrisPhase::OPENING)
    {
        mRadius += mSpeed * dt;
        if (mRadius >= mMaxRadius)
        {
            mRadius = mMaxRadius;
            mPhase  = IrisPhase::IDLE;  // overlay is fully invisible — no-op from here
        }
    }
    // IDLE and CLOSED phases: nothing to advance.
}

// ------------------------------------------------------------
// Render
// Purpose:
//   Upload current iris parameters to the constant buffer and draw the overlay.
//   No-op when IDLE (radius >= mMaxRadius and alpha is 0 everywhere).
//
// Pipeline override sequence:
//   1. Save existing depth-stencil state (restored after draw).
//   2. Set iris VS, PS, input layout, VB, CB.
//   3. Set alpha blend state + depth OFF.
//   4. Draw 4-vertex TriangleStrip.
//   5. Restore depth-stencil state.
//
// Why save/restore depth state?
//   SpriteBatch and other renderers rely on the default depth state.
//   If we leave depth OFF after the iris, other draw calls may render
//   incorrectly (e.g., UI sprites occluding world sprites).
// ------------------------------------------------------------
void IrisTransitionRenderer::Render(ID3D11DeviceContext* ctx)
{
    if (!mInitialized) return;

    // Skip rendering entirely when the iris is fully open — alpha is 0 everywhere.
    if (mPhase == IrisPhase::IDLE) return;

    // -----------------------------------------------------------------
    // Step 1 — Update constant buffer with current radius and center.
    // -----------------------------------------------------------------
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = ctx->Map(mConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    CHECK_HR(hr, "IrisTransitionRenderer::Render — Map CB");
    if (FAILED(hr)) return;

    IrisCB* cb   = reinterpret_cast<IrisCB*>(mapped.pData);
    cb->centerX  = mCenterX;
    cb->centerY  = mCenterY;
    cb->radius   = mRadius;
    cb->softEdge = kSoftEdge;
    ctx->Unmap(mConstantBuffer.Get(), 0);

    // -----------------------------------------------------------------
    // Step 2 — Save current depth-stencil state to restore after draw.
    // -----------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> prevDSS;
    UINT prevStencilRef = 0;
    ctx->OMGetDepthStencilState(prevDSS.GetAddressOf(), &prevStencilRef);

    // -----------------------------------------------------------------
    // Step 3 — Bind the iris pipeline.
    // -----------------------------------------------------------------
    ctx->VSSetShader(mVS.Get(), nullptr, 0);
    ctx->PSSetShader(mPS.Get(), nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, mConstantBuffer.GetAddressOf());

    constexpr UINT stride = sizeof(Vertex);
    constexpr UINT offset = 0;
    ctx->IASetInputLayout(mInputLayout.Get());
    ctx->IASetVertexBuffers(0, 1, mQuadVB.GetAddressOf(), &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Alpha blend: black overlay composites over the scene.
    static constexpr float kBlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->OMSetBlendState(mBlendState.Get(), kBlendFactor, 0xFFFFFFFF);

    // Disable depth test — the overlay must always rasterize on top.
    ctx->OMSetDepthStencilState(mDepthState.Get(), 0);

    ctx->RSSetState(mRasterState.Get());

    // -----------------------------------------------------------------
    // Step 4 — Draw the fullscreen quad (4 vertices, TriangleStrip).
    // -----------------------------------------------------------------
    ctx->Draw(4, 0);

    // -----------------------------------------------------------------
    // Step 5 — Restore depth-stencil state for subsequent draw calls.
    // -----------------------------------------------------------------
    ctx->OMSetDepthStencilState(prevDSS.Get(), prevStencilRef);
}
