// ============================================================
// File: DebugTextureViewer.h
// Responsibility: Draw a single texture centered on screen — for debugging only.
//
// Zero dependencies on SpriteSheet, JsonLoader, UIRenderer, or CommonStates.
// Uses the absolute minimum DirectXTK surface: SpriteBatch + WICTextureLoader.
//
// --- Why DepthNone and AlphaBlend exist ---
//
//   DepthNone  (ID3D11DepthStencilState, DepthEnable=FALSE)
//   ─────────────────────────────────────────────────────────
//   CircleRenderer fills the depth buffer with z=0 on every covered pixel
//   (VS outputs float4(pos, 0.0, 1.0)).  The D3D11 default depth test is
//   D3D11_COMPARISON_LESS — "write pixel only if new_z < stored_z".
//   SpriteBatch also emits z=0, so:   0 < 0  →  FALSE  →  pixel discarded.
//   The sprite is invisible even though it is drawn last.
//   Disabling depth test entirely (DepthEnable=FALSE) lets every sprite
//   pixel overwrite the back-buffer unconditionally, which is correct for
//   screen-space UI/HUD that must always appear on top.
//
//   AlphaBlend  (SRC_ALPHA / INV_SRC_ALPHA)
//   ─────────────────────────────────────────────────────────
//   Passing nullptr for the blend state to SpriteBatch::Begin() does NOT
//   mean "opaque" — it means "whatever blend state the device context
//   currently has bound".  After CircleRenderer calls
//   OMSetBlendState(nullptr), the device default is BlendEnable=FALSE,
//   which writes RGB but ignores alpha.  PNG textures with semi-transparent
//   or fully-transparent pixels would render solid instead of blending
//   correctly.  Explicit SRC_ALPHA/INV_SRC_ALPHA makes the blend
//   deterministic regardless of what any prior renderer left behind.
//
// --- Performance note ---
//   Both state objects are created ONCE in Load() and stored as members.
//   Draw() passes the cached pointers to SpriteBatch::Begin() — zero
//   heap allocations or driver calls per frame.
//   The DrawOptions flags let you disable either override at call-site;
//   when a flag is false the corresponding Begin() slot receives nullptr
//   (SpriteBatch uses whatever is currently bound).  No state object is
//   created or destroyed at runtime regardless of the flag values.
//
// Usage:
//   DebugTextureViewer mDebugView;
//   // OnEnter:
//   mDebugView.Load(device, context, L"assets/animations/verso.png");
//   // Render (default — both overrides active):
//   mDebugView.Draw(context, W, H);
//   // Render — force opaque to test if alpha=0 was hiding the sprite:
//   DebugTextureViewer::DrawOptions opts; opts.forceOpaque = true;
//   mDebugView.Draw(context, W, H, opts);
// ============================================================
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <SpriteBatch.h>
#include <WICTextureLoader.h>
#include <memory>

class DebugTextureViewer
{
public:
    // ------------------------------------------------------------
    // DrawOptions — per-call control over which pipeline overrides
    // DebugTextureViewer applies before SpriteBatch::Begin().
    //
    //   overrideDepth  — pass mDepthNone to Begin() so depth test is OFF.
    //                    Disable if you want your own depth state to stay.
    //   overrideBlend  — pass mAlphaBlend (SRC_ALPHA/INV_SRC_ALPHA) to
    //                    Begin() instead of letting SpriteBatch inherit
    //                    whatever blend state is currently bound.
    //                    Disable only if you have already bound a correct
    //                    blend state and don't want it overridden.
    //
    // Both default to true — the safest, most predictable behaviour.
    // Changing them to false costs nothing at runtime (just passes nullptr
    // to Begin() instead of the cached pointer).
    // ------------------------------------------------------------
    struct DrawOptions
    {
        bool overrideDepth = true;   // pass DepthNone → sprite always visible
        bool overrideBlend = true;   // pass AlphaBlend → correct PNG transparency

        // --- Diagnostic flag ---
        // forceOpaque: pass mOpaqueBlend (BlendEnable=FALSE) instead of AlphaBlend.
        //
        // Purpose: isolate whether the sprite is truly invisible because alpha=0,
        //   or because SpriteBatch is not drawing at all.
        //   With BlendEnable=FALSE every pixel overwrites the back-buffer at full
        //   opacity regardless of the texture's alpha channel.  If a solid-colored
        //   rectangle appears on screen, SpriteBatch IS drawing — the PNG just has
        //   alpha=0 (transparent background) that was hiding it under normal blending.
        //
        // Usage:
        //   mDebugView.Draw(ctx, W, H, { .forceOpaque = true });
        //
        // Only meaningful when overrideBlend=true (it overrides the blend choice).
        bool forceOpaque = false;
    };

    // Load texture from disk and create SpriteBatch + cached state objects.
    // Returns false on failure — Draw() is a no-op until Load() succeeds.
    bool Load(ID3D11Device* device,
              ID3D11DeviceContext* context,
              const wchar_t* path);

    // Draw the whole texture full-screen.
    // opts controls which pipeline overrides are applied — see DrawOptions above.
    void Draw(ID3D11DeviceContext* context,
              int screenW, int screenH,
              DrawOptions opts = {});

    void Shutdown();

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSRV;
    std::unique_ptr<DirectX::SpriteBatch>            mBatch;

    // Cached D3D11 state objects — created ONCE in Load(), never per-frame.
    // See file header for a full explanation of why each one exists.
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  mDepthNone;   // DepthEnable=FALSE
    Microsoft::WRL::ComPtr<ID3D11BlendState>         mAlphaBlend;  // SRC_ALPHA/INV_SRC_ALPHA
    Microsoft::WRL::ComPtr<ID3D11BlendState>         mOpaqueBlend; // BlendEnable=FALSE (diagnostic)

    UINT mTexW = 0;
    UINT mTexH = 0;
};
