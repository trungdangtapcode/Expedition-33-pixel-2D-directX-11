// ============================================================
// File: IrisTransitionRenderer.h
// Responsibility: Render an iris-circle screen transition overlay.
//
// Visual effect:
//   A full-screen black quad with a circular "hole" (the iris opening).
//   The hole edge is feathered with a smoothstep SDF for a soft,
//   cinematic look — identical to classic JRPG battle transitions.
//
//   CLOSING (enter battle): the hole shrinks to 0 → screen goes black.
//   OPENING (reveal scene): the hole grows to full screen → black fades away.
//
// Coordinate convention:
//   The iris center is screen-space pixels, with (0,0) at top-left.
//   Default center = screen center (W/2, H/2).
//   mMaxRadius = sqrt(W*W + H*H)/2 ensures the circle covers all corners.
//
// Shader architecture:
//   VS — static full-screen quad (4 vertices, TriangleStrip).
//         Input layout has one POSITION (float2, NDC coords).
//   PS — computes dist = length(SV_Position.xy - center);
//         alpha = smoothstep(radius - softEdge, radius + softEdge, dist)
//         Output: float4(0, 0, 0, alpha).
//   Blend state: SRC_ALPHA / INV_SRC_ALPHA.
//         alpha=0 inside iris (transparent → game visible).
//         alpha=1 outside iris (opaque black → masks the game).
//
// HLSL compiled at runtime via D3DCompile (same pattern as CircleRenderer).
//
// Usage pattern (per-state):
//   Every state calls Initialize() in OnEnter() — the iris starts at radius=0
//   (fully black).  Call StartOpen() immediately to reveal the scene with a
//   circle-wipe expanding outward (classic JRPG battle-reveal).
//
//   PlayState::OnEnter()   → Initialize() + StartOpen() = overworld fades in
//   on B + enemy nearby    → push BattleState directly (no close needed)
//
//   BattleState::OnEnter() → Initialize() + StartOpen() = battle circle-reveal
//   on WIN / LOSE detected  → StartClose([this]() { mPendingSafeExit=true; })
//                             then defer PopState to top of next Update()
//
// Safe-exit pattern:
//   The StartClose callback MUST NOT call StateManager::PopState() or
//   ChangeState() directly.  The callback fires inside Update(), which is
//   inside the active state's own call stack.  Instead, set a bool flag
//   and check it at the VERY END of Update() — after all other code has
//   returned — then call PopState() safely.
//   (See mPendingSafeExit usage in BattleState.cpp.)
//
// Owns:
//   ID3D11VertexShader, ID3D11PixelShader, ID3D11Buffer (VB + CB),
//   ID3D11InputLayout, ID3D11BlendState, ID3D11RasterizerState,
//   ID3D11DepthStencilState  — all in ComPtr, auto-released on Shutdown().
//
// Lifetime:
//   Initialize() called once in the owning state's OnEnter().
//   Shutdown()   called once in the owning state's OnExit().
//
// Common mistakes:
//   1. Calling Render() when phase == IDLE and radius >= mMaxRadius →
//      wastes GPU time but is not incorrect.  The alpha is 0 everywhere.
//   2. Not calling Update(dt) in the owning state's Update — the iris freezes.
//   3. Calling PopState() directly inside the StartClose callback →
//      use-after-free.  Always defer via a bool flag.
//   4. Forgetting to set a blend state — the black overlay draws opaque everywhere.
// ============================================================
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <functional>
#include <cmath>

// ------------------------------------------------------------
// IrisPhase: the four states of the iris animation FSM.
// ------------------------------------------------------------
enum class IrisPhase
{
    IDLE,      // Fully open (no overlay rendered). Initial state after StartOpen completes.
    CLOSING,   // Circle radius shrinking toward 0 (screen going black).
    CLOSED,    // Radius reached 0. Callback fires exactly once. Stays here until StartOpen.
    OPENING,   // Circle radius growing from 0 toward mMaxRadius.
};

class IrisTransitionRenderer
{
public:
    // ------------------------------------------------------------
    // Initialize
    // Purpose:
    //   Compile shaders, create the quad vertex buffer, constant buffer,
    //   and pipeline states.  Must be called before Update/Render.
    // Parameters:
    //   device   — D3D11 device
    //   screenW/H — render target dimensions for radius and center defaults
    // Returns:
    //   true  — all GPU resources created; safe to use.
    //   false — at least one resource failed; Render() will be a no-op.
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device* device, int screenW, int screenH);

    // Release all GPU resources.  Safe to call even if Initialize failed.
    void Shutdown();

    // ------------------------------------------------------------
    // StartClose
    // Purpose:
    //   Begin shrinking the iris from the current radius toward 0.
    //   When the radius reaches 0, onClosed is called ONCE and the
    //   phase transitions to CLOSED.
    //
    //   IMPORTANT: onClosed fires inside Update().  Do NOT call
    //   StateManager::PopState() inside it — set a deferred flag instead.
    //
    // Parameters:
    //   onClosed — callback invoked exactly once when radius == 0
    //   speed    — pixels per second the radius shrinks (default: 800)
    // ------------------------------------------------------------
    void StartClose(std::function<void()> onClosed, float speed = 800.0f);

    // ------------------------------------------------------------
    // StartOpen
    // Purpose:
    //   Begin growing the iris from the current radius toward mMaxRadius.
    //   When mMaxRadius is reached the phase transitions to IDLE and
    //   the overlay becomes a no-op.
    // Parameters:
    //   speed — pixels per second (default: 800)
    // ------------------------------------------------------------
    void StartOpen(float speed = 800.0f);

    // Advance the iris radius and fire the onClosed callback when appropriate.
    // Must be called every frame from the owning state's Update(dt).
    void Update(float dt);

    // Draw the black overlay with the iris hole.
    // No-op when phase == IDLE (radius >= mMaxRadius — screen fully visible).
    // Must be called at the END of the owning state's Render(), after all
    // game content has been drawn, so the overlay sits on top.
    void Render(ID3D11DeviceContext* ctx);

    // State queries.
    IrisPhase GetPhase()        const { return mPhase; }
    bool      IsFullyClosed()   const { return mPhase == IrisPhase::CLOSED;  }
    bool      IsIdle()          const { return mPhase == IrisPhase::IDLE;    }
    bool      IsTransitioning() const {
        return mPhase == IrisPhase::CLOSING || mPhase == IrisPhase::OPENING;
    }

private:
    // ---- Animation state ----
    IrisPhase mPhase     = IrisPhase::IDLE;
    float     mRadius    = 0.0f;    // current iris opening radius (screen pixels)
    float     mMaxRadius = 0.0f;    // sqrt(W^2 + H^2)/2 — covers all screen corners
    float     mSpeed     = 800.0f;  // pixels per second
    float     mCenterX   = 0.0f;    // iris center X (screen pixels, top-left origin)
    float     mCenterY   = 0.0f;    // iris center Y (screen pixels, top-left origin)

    // Feather width in screen pixels.  24px gives a visually smooth edge without
    // blurring the gameplay visible through the iris hole.
    static constexpr float kSoftEdge = 24.0f;

    // mOnClosed is held until the phase transitions to CLOSED, then invoked once.
    // Cleared immediately after invocation to prevent double-firing.
    std::function<void()> mOnClosed;

    // ---- D3D resources ----
    struct Vertex { float x, y; };

    // Constant buffer layout — MUST be a multiple of 16 bytes.
    // 4 floats × 4 bytes = 16 bytes ✓
    struct IrisCB
    {
        float centerX;    // iris center in screen pixels (top-left origin)
        float centerY;
        float radius;     // current opening radius
        float softEdge;   // feather width
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>      mVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       mPS;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            mQuadVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            mConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       mInputLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        mBlendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   mRasterState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthState;

    bool mInitialized = false;
};
