// ============================================================
// File: PincushionDistortionFilter.h
// Responsibility: Fullscreen pincushion/barrel distortion post-process filter.
//
// Visual effect:
//   Pincushion distortion bends straight lines outward toward the screen
//   corners (opposite of barrel distortion which bends inward).  In JRPG
//   battle transitions this creates the visual impression of being "sucked
//   into" the battle — a tightening, lens-like warp that signals the world
//   is about to change.
//
// Shader algorithm (UV distortion):
//   uv_centered = uv - 0.5           (re-center to [-0.5, +0.5])
//   r2          = dot(uv_c, uv_c)    (squared distance from center)
//   uv_warped   = uv_c * (1 + k * r2) + 0.5   (pinch outward)
//   sample      = texture.Sample(sampler, uv_warped)
//
//   k > 0 → pincushion (outward warp, corners stretch out)
//   k < 0 → barrel (inward warp, like a fisheye)
//
// Implementation:
//   1. BeginCapture() — bind the offscreen RT; all scene draws go there.
//   2. EndCapture()   — restore back-buffer RT; scene is now in mSceneSRV.
//   3. Render()       — draw fullscreen quad sampling mSceneSRV with the
//                       pincushion PS, writing distorted output to the back buffer.
//
// Offscreen RT:
//   DXGI_FORMAT_R8G8B8A8_UNORM, same size as the back buffer.
//   The DSV from D3DContext is reused (no separate depth buffer needed —
//   the offscreen RT only stores color; depth only matters during scene draw).
//
// Owns:
//   ID3D11Texture2D (offscreen RT texture)
//   ID3D11RenderTargetView (bound during BeginCapture)
//   ID3D11ShaderResourceView (sampled during Render)
//   ID3D11Buffer (fullscreen quad VB, constant buffer)
//   ID3D11VertexShader, ID3D11PixelShader, ID3D11InputLayout
//   ID3D11SamplerState (clamp sampler for edge pixels)
//   ID3D11RasterizerState (cull-none, no depth test)
//   ID3D11DepthStencilState (depth writes + test off during fullscreen draw)
//
// Lifetime:
//   Initialize() called in owning state's OnEnter().
//   Shutdown()   called in owning state's OnExit().
//
// Common mistakes:
//   1. Sampling outside [0,1] with a WRAP sampler when k is large — produces
//      tiling artifacts.  Use CLAMP sampler so edges repeat the border pixel.
//   2. Forgetting D3DContext::Get().GetDSV() reuse — the offscreen RT shares
//      the main depth buffer, which is fine because we only draw 2D sprites.
//   3. Not clamping uv_warped in the PS — pixels at extreme corners may
//      sample black if CLAMP sampler is used with a border color of 0.
//      This is intentional (edges go black) and is part of the effect.
// ============================================================
#pragma once
#include "IScreenFilter.h"
#include <d3d11.h>
#include <wrl/client.h>

class PincushionDistortionFilter : public IScreenFilter
{
public:
    // IScreenFilter interface implementation
    bool Initialize(ID3D11Device* device, int width, int height) override;
    void Shutdown()                                               override;
    void BeginCapture(ID3D11DeviceContext* ctx)                   override;
    void EndCapture(ID3D11DeviceContext* ctx)                     override;
    void Render(ID3D11DeviceContext* ctx)                         override;
    void Update(float dt, float intensity)                        override;
    bool IsActive() const                                         override;

private:
    // ---- Offscreen render target ----
    // Scene is captured here during BeginCapture/EndCapture.
    // mSceneRTV is bound as the output RT; mSceneSRV is sampled in Render().
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          mSceneTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   mSceneRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSceneSRV;

    // ---- Shader pipeline ----
    Microsoft::WRL::ComPtr<ID3D11VertexShader>       mVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>        mPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>        mInputLayout;

    // ---- Geometry ----
    // Static fullscreen quad (4 NDC vertices, TriangleStrip).
    Microsoft::WRL::ComPtr<ID3D11Buffer>             mQuadVB;

    // ---- Constant buffer ----
    // Carries the distortion coefficient k (updated each frame).
    // CB size = 16 bytes (4-float pad to satisfy D3D11 16-byte alignment).
    Microsoft::WRL::ComPtr<ID3D11Buffer>             mConstantBuffer;

    // ---- Render states ----
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       mSampler;        // CLAMP, no wrap
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>    mRasterState;    // cull-none
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  mDepthOffState;  // depth test OFF

    // ---- Saved state for BeginCapture/EndCapture ----
    // We save the back-buffer RTV so EndCapture can restore it correctly
    // even if the window was resized between frames.
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   mSavedRTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   mSavedDSV;

    // ---- Filter state ----
    float mCurrentIntensity = 0.0f;   // [0,1] — set by Update(); 0 = inactive
    bool  mInitialized      = false;

    // Maximum k coefficient at full intensity.
    // k=0.6 produces a pronounced but not grotesque pincushion at intensity=1.
    static constexpr float kMaxCoefficient = -1.6f;

    // Intensity below this threshold → IsActive() returns false.
    // Skips BeginCapture overhead when effect is imperceptible.
    static constexpr float kActivationThreshold = 0.005f;

    struct Vertex { float x, y; };

    // Constant buffer layout — must be 16-byte aligned.
    // k: pincushion distortion coefficient
    // pad0-2: unused, required to reach 16-byte struct size
    struct FilterCB
    {
        float k;
        float pad0;
        float pad1;
        float pad2;
    };
    static_assert(sizeof(FilterCB) == 16, "FilterCB must be 16 bytes.");
};
