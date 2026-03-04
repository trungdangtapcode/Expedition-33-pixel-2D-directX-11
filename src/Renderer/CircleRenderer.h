// ============================================================
// File: CircleRenderer.h
// Responsibility: Draw a filled anti-aliased circle in 2D screen space
//                 using a full-screen quad + pixel shader SDF technique.
//
// HOW IT WORKS:
//   A quad (2 triangles covering the whole back buffer) is submitted.
//   The Pixel Shader receives the circle's center (pixels), radius (pixels),
//   and the current render target resolution via a Constant Buffer.
//   For each pixel it computes the signed distance to the circle edge:
//     dist = length(pixelPos - center) - radius
//   If dist < 0  → inside   → draw the fill color.
//   If dist < AA → on edge  → smooth alpha blend (anti-alias).
//   If dist >= AA → outside → discard (transparent).
//
// WHY A QUAD + SDF INSTEAD OF A TRIANGLE FAN:
//   - No geometry changes when the circle moves — only the constant buffer
//     is updated (one 16-byte upload per frame).
//   - Perfect anti-aliasing in the PS at no extra vertex cost.
//   - Trivially extensible to rings, glows, health bars.
//
// Owns:
//   ID3D11Buffer             mQuadVB        — 4 NDC vertices (never changes)
//   ID3D11Buffer             mConstantBuffer — updated each Draw() call
//   ID3D11VertexShader       mVS
//   ID3D11PixelShader        mPS
//   ID3D11InputLayout        mInputLayout
//   ID3D11BlendState         mBlendState    — alpha blending for AA edge
//
// Lifetime:
//   Created in  → PlayState::OnEnter()  via CircleRenderer::Initialize()
//   Destroyed in → PlayState::OnExit()  via CircleRenderer::Shutdown()
//                  (or when the unique_ptr holding it is reset)
//
// Common mistakes:
//   1. Forgetting to call IASetVertexBuffers before Draw → nothing renders.
//   2. Uploading the wrong resolution → circle appears at wrong position.
//   3. Not setting the blend state → AA edge appears as a hard black ring.
// ============================================================
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

class CircleRenderer {
public:
    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose:  Compile HLSL shaders, create GPU resources.
    // Why:      Must be called once before any Draw() call.
    // Returns:  false if shader compilation or resource creation fails.
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device* device);

    // ------------------------------------------------------------
    // Function: Draw
    // Purpose:  Upload circle parameters to GPU and issue 1 draw call.
    // Parameters:
    //   context    — the immediate device context for this frame
    //   centerX/Y  — circle center in screen pixels (top-left origin)
    //   radius     — circle radius in pixels
    //   r/g/b      — fill color, each in [0.0, 1.0]
    //   screenW/H  — current render target dimensions (for NDC conversion)
    // ------------------------------------------------------------
    void Draw(ID3D11DeviceContext* context,
              float centerX, float centerY, float radius,
              float r, float g, float b,
              int screenW, int screenH);

    void Shutdown();

private:
    // ------------------------------------------------------------
    // Constant buffer layout — must match the cbuffer in the HLSL exactly
    // (same field order, same total size, 16-byte aligned).
    // ------------------------------------------------------------
    struct CircleCB {
        DirectX::XMFLOAT2 center;      // circle center in screen pixels
        float             radius;      // circle radius in pixels
        float             pad0;        // pad to 16 bytes
        DirectX::XMFLOAT3 color;       // RGB fill color
        float             pad1;        // pad to 32 bytes
        DirectX::XMFLOAT2 resolution;  // render target size in pixels
        float             pad2;        // pad to 48 bytes
        float             pad3;
    };

    // Quad: two triangles covering the full NDC screen [-1,1] x [-1,1].
    // The VS passes them through unchanged; the PS does all the circle math.
    struct Vertex { float x, y; };

    Microsoft::WRL::ComPtr<ID3D11Buffer>        mQuadVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        mConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  mVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   mPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   mInputLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState>    mBlendState;
};
