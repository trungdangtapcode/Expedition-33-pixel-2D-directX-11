// ============================================================
// File: UIRenderer.h
// Responsibility: Draw screen-space animated sprites for UI and HUD elements.
//
// UIRenderer is purpose-built for anything that must stay fixed on the
// screen regardless of the camera — health bars, dialogue portraits,
// battle menus, skill icons, and cutscene overlays.
//
// Key design constraints:
//   - Does NOT use a Camera.  Screen size is set directly via SetScreenSize().
//   - Frame slicing is HORIZONTAL ONLY (single-row sprite sheet).
//     The sheet height MUST equal the frame height.  An assert fires
//     in Initialize() if this is violated to catch mis-configured JSON.
//   - Each named Draw method (DrawBottomCenter, DrawTopLeft, etc.) encodes
//     the intent at the call site.  There are no "magic" enum arguments.
//   - The device context is passed per-frame to Draw() — never stored as a
//     member, so the renderer stays valid across device recreations.
//
// Typical usage:
//   UIRenderer mPortrait;
//   mPortrait.Initialize(device, context, L"assets/ui/portrait.png", sheet);
//   mPortrait.SetScreenSize(1280, 720);
//   mPortrait.PlayClip("talk");
//   // each frame:
//   mPortrait.Update(dt);
//   mPortrait.DrawBottomCenter(ctx, 2.0f);
//
// Owns:
//   ID3D11ShaderResourceView  mTextureSRV  — GPU texture (one SRV per renderer)
//   SpriteBatch               mSpriteBatch — queues and batches draw calls
//
// Lifetime:
//   Created in  → UIRenderer::Initialize()  (called from state OnEnter)
//   Destroyed in → UIRenderer::Shutdown()   (called from state OnExit)
//
// Common mistakes:
//   1. Using UIRenderer for world-space characters — use WorldRenderer instead.
//   2. Forgetting SetScreenSize() after a window resize.
//   3. Multi-row sprite sheets — UIRenderer only supports single-row sheets.
//      Use SpriteSheet.sheetHeight == SpriteSheet.frameHeight; assert enforces this.
//   4. Calling any DrawXxx() before Initialize() — mSpriteBatch is null, silent no-op.
// ============================================================
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <WICTextureLoader.h>
#include <memory>
#include <string>
#include "SpriteSheet.h"

class UIRenderer
{
public:
    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose:
    //   Load the sprite sheet texture and create the SpriteBatch.
    //   Asserts that the sheet is single-row (sheetHeight == frameHeight).
    // Parameters:
    //   device      — used once for texture upload + SpriteBatch creation
    //   context     — passed to SpriteBatch constructor (used in Begin/End)
    //   texturePath — path to the PNG/JPEG texture (wide string for WIC)
    //   sheet       — sprite sheet descriptor from JsonLoader
    // Returns:
    //   true  — ready to call DrawXxx()
    //   false — texture load or SpriteBatch creation failed
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device*        device,
                    ID3D11DeviceContext*  context,
                    const std::wstring&  texturePath,
                    const SpriteSheet&   sheet);

    // ------------------------------------------------------------
    // Function: SetScreenSize
    // Purpose:
    //   Provide the current render target dimensions.
    //   Must be called after Initialize() and after every window resize.
    //   All DrawXxx() methods use these dimensions to place the sprite.
    // Parameters:
    //   width, height — render target size in pixels
    // ------------------------------------------------------------
    void SetScreenSize(int width, int height);

    // ------------------------------------------------------------
    // Function: PlayClip
    // Purpose:
    //   Activate a named animation clip by name.
    //   Resets mFrameIndex and mFrameTimer to start at frame 0.
    //   No-op if the clip is already active.
    // ------------------------------------------------------------
    void PlayClip(const std::string& clipName);

    // ------------------------------------------------------------
    // Function: Update
    // Purpose:
    //   Advance the animation frame timer by dt seconds.
    //   Must be called once per frame before any DrawXxx() call.
    // ------------------------------------------------------------
    void Update(float dt);

    // ------------------------------------------------------------------
    // Draw helpers — each method anchors the sprite to a fixed screen
    // corner or edge.  No pixel coordinates are passed by the caller.
    // The pivot from the JSON "pivot" field is applied internally.
    //
    // Parameters for all Draw methods:
    //   context — immediate device context for this frame
    //   scale   — uniform scale (1.0 = original pixel size)
    // ------------------------------------------------------------------

    // Sprite pivot lands at top-left corner (0, 0).
    void DrawTopLeft(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at the top edge, horizontally centered.
    void DrawTopCenter(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at top-right corner (screenW, 0).
    void DrawTopRight(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at the left edge, vertically centered.
    void DrawMiddleLeft(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at the exact screen center.
    void DrawCenter(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at the right edge, vertically centered.
    void DrawMiddleRight(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at bottom-left corner (0, screenH).
    void DrawBottomLeft(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at the bottom edge, horizontally centered.
    // Default for standing characters (pivot at their feet).
    void DrawBottomCenter(ID3D11DeviceContext* context, float scale = 1.0f);

    // Sprite pivot lands at bottom-right corner (screenW, screenH).
    void DrawBottomRight(ID3D11DeviceContext* context, float scale = 1.0f);

    // Draw at an explicit screen-space pixel position.
    // Use only when none of the named anchors fit (e.g. dialogue portrait
    // offset from the corner by a fixed inset).
    void DrawAt(ID3D11DeviceContext* context,
                float screenX, float screenY,
                float scale = 1.0f);

    void Shutdown();

    // --- Accessors (useful for debug HUD) ---
    const std::string& GetActiveClip() const { return mActiveClipName; }
    int  GetFrameIndex()               const { return mFrameIndex; }
    bool IsInitialized()               const { return mSpriteBatch != nullptr; }

private:
    // ---------------------------------------------------------------
    // Internal draw implementation — all public DrawXxx() methods
    // resolve their anchor to a pixel position and call this.
    // ---------------------------------------------------------------
    void DrawAtPosition(ID3D11DeviceContext* context,
                        float screenX, float screenY,
                        float scale);

    // ---------- GPU resources ----------
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;
    // CommonStates provides pre-built D3D11 state objects (blend, depth, raster,
    // sampler) so we can explicitly pass DepthNone() to SpriteBatch::Begin().
    // This prevents CircleRenderer's lingering depth state from discarding sprite
    // pixels — CircleRenderer's full-screen quad writes z=0 to the entire depth
    // buffer; without DepthNone, every subsequent SpriteBatch pixel fails z=0 < z=0.
    std::unique_ptr<DirectX::CommonStates>           mStates;

    // ---------- Sprite sheet data ----------
    // Full copy stored here — no dependency on the caller's SpriteSheet lifetime.
    SpriteSheet mSheet;

    // ---------- Screen dimensions (set via SetScreenSize) ----------
    int mScreenW = 0;
    int mScreenH = 0;

    // ---------- Animation state ----------
    std::string          mActiveClipName;
    int                  mFrameIndex  = 0;
    float                mFrameTimer  = 0.0f;  // seconds elapsed in the current frame
    const AnimationClip* mActiveClip  = nullptr; // pointer into mSheet.animations
};
