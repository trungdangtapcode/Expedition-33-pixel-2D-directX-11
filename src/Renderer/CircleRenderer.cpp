// ============================================================
// File: CircleRenderer.cpp
// Responsibility: Compile shaders + create GPU resources for circle SDF rendering.
//
// Architecture decision:
//   Shaders are embedded as raw string literals and compiled at runtime via
//   D3DCompile(). This avoids requiring a separate .hlsl compile step in the
//   build script while keeping the HLSL readable and editable in one place.
//   For a production title you would use offline FXC/DXC compilation and load
//   pre-compiled .cso blobs — but for this prototype runtime compile is fine.
//
// DirectX concepts used:
//   - ID3D11Buffer (D3D11_BIND_VERTEX_BUFFER)    : NDC quad geometry, static
//   - ID3D11Buffer (D3D11_BIND_CONSTANT_BUFFER)  : per-draw circle params
//   - ID3DBlob                                   : shader bytecode from D3DCompile
//   - ID3D11BlendState                           : alpha blend for AA ring edge
//
// Lifetime:
//   All ComPtr members auto-release when CircleRenderer is destroyed.
//   Explicitly call Shutdown() before device teardown if you want the
//   DX debug layer to report a clean slate.
//
// Common mistakes:
//   1. Struct size not a multiple of 16 bytes → D3D11 silently uploads garbage.
//   2. D3D11_MAPPED_SUBRESOURCE written to after Unmap → undefined behavior.
//   3. Omitting IASetPrimitiveTopology → draw call is silently ignored.
// ============================================================
#include "CircleRenderer.h"
#include <d3dcompiler.h>
#include <cstdio>
#include "../Utils/Log.h"
#include "../Utils/HrCheck.h"

// ============================================================
// Embedded HLSL — Vertex Shader
//
// Why a full-screen quad at NDC coords?
//   We want the pixel shader to run over every pixel that could possibly
//   be inside the circle. A quad slightly larger than the circle would be
//   more efficient, but for simplicity we cover the entire screen.
//   The only data the PS needs is SV_Position (built-in pixel coordinates).
// ============================================================
static const char* kCircleVS = R"hlsl(
struct VSIn  { float2 pos : POSITION; };
struct VSOut { float4 pos : SV_Position; };

VSOut main(VSIn v)
{
    VSOut o;
    // Pass the NDC quad vertices straight to the rasterizer.
    // z = 0 (no depth test needed for 2D overlays),
    // w = 1 (no perspective divide).
    o.pos = float4(v.pos, 0.0f, 1.0f);
    return o;
}
)hlsl";

// ============================================================
// Embedded HLSL — Pixel Shader
//
// Algorithm: Signed Distance Field circle
//   dist = length(pixel_pos - center) - radius
//   dist < 0          → fully inside   → opaque fill color
//   0 <= dist < AA    → on the edge    → smoothstep alpha (anti-alias)
//   dist >= AA        → outside        → discard pixel (transparent)
//
// Why SDF instead of a vertex-based circle mesh?
//   - Moving the circle requires only 1 constant-buffer upload (no VB change).
//   - The AA is mathematically exact regardless of circle size.
//   - Zero extra draw calls; reuses the same static quad VB every frame.
// ============================================================
static const char* kCirclePS = R"hlsl(
cbuffer CircleCB : register(b0)
{
    float2 center;      // center in screen pixels (top-left origin)
    float  radius;
    float  pad0;
    float3 color;
    float  pad1;
    float2 resolution;  // render target size in pixels
    float  pad2;
    float  pad3;
};

float4 main(float4 svPos : SV_Position) : SV_Target
{
    // SV_Position.xy is the pixel center in screen space (top-left origin).
    // Compute the Euclidean distance from this pixel to the circle center.
    float dist = length(svPos.xy - center) - radius;

    // Anti-alias width: 1.5 pixels on the outer edge.
    // smoothstep maps [0, 1.5] → alpha [1, 0] for a smooth falloff.
    float alpha = 1.0f - smoothstep(0.0f, 1.5f, dist);

    // Pixels outside the circle + AA band are completely transparent.
    clip(alpha - 0.001f);

    return float4(color, alpha);
}
)hlsl";

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   1. Compile VS + PS from embedded HLSL strings via D3DCompile.
//   2. Create the static full-screen quad vertex buffer.
//   3. Create the per-draw constant buffer (dynamic, CPU write).
//   4. Create a blend state that supports alpha transparency for
//      the anti-aliased circle edge.
// Why:
//   All GPU objects are created once here and reused every frame.
//   Only the constant buffer data changes per Draw() call.
// Returns:
//   true  — all resources created successfully, safe to call Draw()
//   false — at least one resource failed, do not call Draw()
// ------------------------------------------------------------
bool CircleRenderer::Initialize(ID3D11Device* device)
{
    HRESULT hr;

    // -----------------------------------------------------------------
    // Step 1 — Compile Vertex Shader
    // D3DCompile compiles HLSL source to DXBC bytecode in memory.
    // The ID3DBlob interface is just a COM-wrapped byte array.
    // pErrorBlob contains human-readable compile error text if hr fails.
    // -----------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    hr = D3DCompile(
        kCircleVS, strlen(kCircleVS),  // source text + length
        "CircleVS",                    // optional debug name (shows in PIX/RenderDoc)
        nullptr,                       // no #define macros
        nullptr,                       // no #include handler
        "main",                        // entry point function name
        "vs_5_0",                      // shader model target
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,  // debug flags (no-op in Release)
        0,
        vsBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );
    if (FAILED(hr)) {
        const char* msg = errorBlob ? (const char*)errorBlob->GetBufferPointer() : "(no error blob)";
        LOG("[CircleRenderer] VS compile failed: %s", msg);
        return false;
    }

    hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,     // no class linkage
        mVS.GetAddressOf()
    );
    if (FAILED(hr)) { LOG("[CircleRenderer] CreateVertexShader failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 2 — Compile Pixel Shader
    // -----------------------------------------------------------------
    hr = D3DCompile(
        kCirclePS, strlen(kCirclePS),
        "CirclePS",
        nullptr, nullptr,
        "main", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        psBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );
    if (FAILED(hr)) {
        const char* msg = errorBlob ? (const char*)errorBlob->GetBufferPointer() : "(no error blob)";
        LOG("[CircleRenderer] PS compile failed: %s", msg);
        return false;
    }

    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        mPS.GetAddressOf()
    );
    if (FAILED(hr)) { LOG("[CircleRenderer] CreatePixelShader failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 3 — Input Layout
    // Describes how the IA stage interprets one vertex from the VB.
    // Must match the VSIn struct: one POSITION element, 2 floats.
    // We pass vsBlob so D3D can validate the layout against the shader.
    // -----------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = device->CreateInputLayout(
        layoutDesc, 1,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        mInputLayout.GetAddressOf()
    );
    if (FAILED(hr)) { LOG("[CircleRenderer] CreateInputLayout failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 4 — Full-Screen Quad Vertex Buffer (static, never changes)
    // NDC coordinates: x in [-1,+1], y in [-1,+1].
    // D3D11 uses a left-hand NDC where +Y is up, unlike screen space.
    // Two triangles via TriangleStrip topology:
    //   v0(-1,+1) ---- v1(+1,+1)
    //      |        /        |
    //   v2(-1,-1) ---- v3(+1,-1)
    // -----------------------------------------------------------------
    Vertex quad[4] = {
        { -1.0f,  1.0f },   // top-left
        {  1.0f,  1.0f },   // top-right
        { -1.0f, -1.0f },   // bottom-left
        {  1.0f, -1.0f },   // bottom-right
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth      = sizeof(quad);
    vbDesc.Usage          = D3D11_USAGE_IMMUTABLE;  // GPU read-only; upload once, reuse forever
    vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = quad;

    hr = device->CreateBuffer(&vbDesc, &vbData, mQuadVB.GetAddressOf());
    if (FAILED(hr)) { LOG("[CircleRenderer] CreateBuffer (VB) failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 5 — Constant Buffer (dynamic — written every Draw() call)
    // D3D11_USAGE_DYNAMIC + CPU_ACCESS_WRITE allows Map/Unmap each frame
    // without a staging buffer detour.
    // ByteWidth MUST be a multiple of 16; sizeof(CircleCB) = 48 which is OK.
    // -----------------------------------------------------------------
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(CircleCB);       // 48 bytes (multiple of 16 ✓)
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&cbDesc, nullptr, mConstantBuffer.GetAddressOf());
    if (FAILED(hr)) { LOG("[CircleRenderer] CreateBuffer (CB) failed (0x%08X)", hr); return false; }

    // -----------------------------------------------------------------
    // Step 6 — Alpha Blend State
    // Needed for the anti-aliased edge pixels whose alpha < 1.
    // SRC_ALPHA / INV_SRC_ALPHA is the standard "painter's" blend:
    //   result = src.rgb * src.a  +  dst.rgb * (1 - src.a)
    // Without this state the AA ring appears as a harsh opaque fringe.
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
    if (FAILED(hr)) { LOG("[CircleRenderer] CreateBlendState failed (0x%08X)", hr); return false; }

    LOG("[CircleRenderer] Initialized successfully.");
    return true;
}

// ------------------------------------------------------------
// Function: Draw
// Purpose:
//   Upload the new circle parameters to the GPU constant buffer,
//   bind all pipeline stages, and issue a 4-vertex TriangleStrip draw.
//   The pixel shader will discard fragments outside the circle.
// Why Map/Unmap instead of UpdateSubresource?
//   For D3D11_USAGE_DYNAMIC buffers, Map(DISCARD) is the correct and
//   fastest path — it lets the driver give you a fresh memory region
//   without a GPU-CPU sync stall.
// Caveats:
//   - screenW/H must match the actual render target or the circle
//     will appear at the wrong position.
//   - Draw() does NOT save and restore prior pipeline state.
//     Call it only as the last draw call in the frame if other
//     renderers need their own state to remain bound.
// ------------------------------------------------------------
void CircleRenderer::Draw(ID3D11DeviceContext* context,
                          float centerX, float centerY, float radius,
                          float r, float g, float b,
                          int screenW, int screenH)
{
    // --- Update constant buffer ---
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(
        mConstantBuffer.Get(),
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mapped
    );
    CHECK_HR(hr, "CircleRenderer::Draw — Map constant buffer");
    if (FAILED(hr)) return;

    CircleCB* cb = reinterpret_cast<CircleCB*>(mapped.pData);
    cb->center     = { centerX, centerY };
    cb->radius     = radius;
    cb->pad0       = 0.0f;
    cb->color      = { r, g, b };
    cb->pad1       = 1.0f;  // alpha (unused, kept for alignment)
    cb->resolution = { static_cast<float>(screenW), static_cast<float>(screenH) };
    cb->pad2       = 0.0f;
    cb->pad3       = 0.0f;

    context->Unmap(mConstantBuffer.Get(), 0);
    // Do NOT touch mapped.pData after Unmap — it is a dangling pointer.

    // --- Bind pipeline state ---
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, mQuadVB.GetAddressOf(), &stride, &offset);
    context->IASetInputLayout(mInputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    context->VSSetShader(mVS.Get(), nullptr, 0);
    context->PSSetShader(mPS.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, mConstantBuffer.GetAddressOf());

    // Bind the alpha-blend state (blend factor and sample mask use defaults).
    const float blendFactor[4] = { 0, 0, 0, 0 };
    context->OMSetBlendState(mBlendState.Get(), blendFactor, 0xFFFFFFFF);

    // Issue 4 vertices → 2 triangles → full-screen quad.
    context->Draw(4, 0);

    // ---------------------------------------------------------------
    // Restore pipeline state to D3D11 defaults after the draw.
    //
    // Why this is mandatory:
    //   SpriteBatch::End() issues its own draw call using DirectXTK's
    //   internal VS/PS/InputLayout.  If CircleRenderer's shaders and
    //   input layout remain bound, the D3D11 debug layer emits errors
    //   (input layout / shader mismatch) that cause SpriteBatch to throw
    //   std::logic_error("Cannot nest Begin calls") on the NEXT frame —
    //   because End() threw on this frame, leaving mInBeginEndPair=true
    //   inside SpriteBatch's internal state permanently.
    //
    // What we reset and why:
    //   VS / PS       — SpriteBatch sets its own; clearing avoids the
    //                   debug layer warning C4: shader/layout mismatch.
    //   InputLayout   — CircleRenderer uses POSITION-only (2 floats);
    //                   SpriteBatch needs a different layout.  Leaving it
    //                   bound causes D3D11 ERROR on SpriteBatch's draw call.
    //   BlendState    — null = D3D11 default (opaque); SpriteBatch will
    //                   overwrite with its own state in End() anyway.
    //   DepthStencil  — null = D3D11 default (depth ON); UIRenderer's
    //                   Begin(DepthNone) overrides this — but clearing here
    //                   makes the pipeline state predictable for every
    //                   renderer that runs after CircleRenderer.
    //   PrimitiveTopology — reset to TRIANGLELIST (D3D11 default); avoids
    //                       confusing any renderer that forgets to set it.
    // ---------------------------------------------------------------
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->IASetInputLayout(nullptr);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(nullptr, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Unbind Vertex Buffer slot 0 and PS Constant Buffer slot 0.
    //
    // Why these two are easy to miss:
    //   SpriteBatch DOES rebind the vertex buffer before its own draw call,
    //   so leaving ours bound would be overwritten — but the D3D11 debug
    //   layer still validates the layout match at IASetVertexBuffers time.
    //   Leaving mConstantBuffer bound at PS slot 0 is more dangerous: SpriteBatch
    //   does NOT use a PS constant buffer (it uses a VS constant buffer at slot 0).
    //   On some drivers, a "phantom" resource in a PS slot the shader declares
    //   no cbuffer for can cause undefined reads or a silent draw-call drop.
    ID3D11Buffer* nullBuf = nullptr;
    UINT zero = 0;
    context->IASetVertexBuffers(0, 1, &nullBuf, &zero, &zero);
    context->PSSetConstantBuffers(0, 1, &nullBuf);
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Explicitly reset all ComPtr members, releasing the D3D11 references.
//   The GPU objects are destroyed once their ref-count hits zero.
// Why call this explicitly when ComPtr auto-releases on destruction?
//   Calling Shutdown() before D3D11 device teardown ensures that
//   ID3D11Debug::ReportLiveDeviceObjects() shows a clean slate.
//   If the device is destroyed before the ComPtrs go out of scope
//   (e.g., member ordering), the debug layer reports spurious leaks.
// ------------------------------------------------------------
void CircleRenderer::Shutdown()
{
    mBlendState.Reset();
    mInputLayout.Reset();
    mPS.Reset();
    mVS.Reset();
    mConstantBuffer.Reset();
    mQuadVB.Reset();
    LOG("[CircleRenderer] Shutdown complete.");
}
