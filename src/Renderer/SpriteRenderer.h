// ============================================================
// File: SpriteRenderer.h
// Responsibility: Draw one animated sprite from a sprite sheet texture.
//
// Uses DirectXTK's SpriteBatch for GPU-accelerated 2D drawing and
// WICTextureLoader to load the PNG texture into a shader resource view.
//
// Animation model:
//   The active clip is set by name (e.g. "idle", "walk").
//   mFrameTimer accumulates deltaTime; when it exceeds (1 / frameRate)
//   the frame index advances.  On loop clips the index wraps; on
//   non-loop clips the last frame is held.
//
//   Frame indexing (single-row sheet):
//     srcRect.left   = frameIndex * frameWidth
//     srcRect.right  = srcRect.left + frameWidth
//     srcRect.top    = 0
//     srcRect.bottom = frameHeight
//
// Pivot / origin:
//   DirectXTK SpriteBatch::Draw accepts an 'origin' vector that acts
//   as the anchor point within the source RECT.  We map the JSON pivot
//   directly: origin = XMFLOAT2(pivotX, pivotY).
//
// Owns:
//   ID3D11ShaderResourceView  mTextureSRV — GPU texture view
//   SpriteBatch               mSpriteBatch — queues and flushes draw calls
//
// Lifetime:
//   Created in  → SpriteRenderer::Initialize()  (called from PlayState::OnEnter)
//   Destroyed in → SpriteRenderer::Shutdown()   (called from PlayState::OnExit)
//
// Common mistakes:
//   1. Calling Draw() outside Begin/End pair → assertion in SpriteBatch.
//   2. Passing an absolute file path that doesn't exist → WICTextureLoader
//      returns E_FAIL; always check the return value.
//   3. Forgetting to call SpriteBatch::End() → draw calls are never flushed.
// ============================================================
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>          // DirectXTK — include path set via /I "%VCPKG_DIR%\include\directxtk"
#include <WICTextureLoader.h>     // DirectXTK
#include <memory>
#include <string>
#include "SpriteSheet.h"

class SpriteRenderer {
public:
    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose:  Load the PNG texture and create the SpriteBatch.
    // Parameters:
    //   device      — used once for texture upload + SpriteBatch creation
    //   context     — stored; SpriteBatch::Begin/End needs it each frame
    //   texturePath — absolute or relative path to the PNG file
    //   sheet       — sprite sheet descriptor (frame sizes, animations)
    // Returns:
    //   true  — ready to Draw()
    //   false — texture load or SpriteBatch creation failed
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::wstring& texturePath,
                    const SpriteSheet& sheet);

    // ------------------------------------------------------------
    // Function: PlayClip
    // Purpose:  Switch the active animation clip.
    //           Resets mFrameIndex and mFrameTimer.
    // Note:     No-op if clipName is already the active clip.
    // ------------------------------------------------------------
    void PlayClip(const std::string& clipName);

    // ------------------------------------------------------------
    // Function: Update
    // Purpose:  Advance the animation frame timer.
    //           Must be called once per frame before Draw().
    // Parameters:
    //   dt — seconds since last frame (frame-rate-independent)
    // ------------------------------------------------------------
    void Update(float dt);

    // ------------------------------------------------------------
    // Function: SetScreenSize
    // Purpose:  Tell the renderer the current render target dimensions.
    //           Must be called whenever the window is resized.
    //           Called automatically by Initialize(); call again on resize.
    // ------------------------------------------------------------
    void SetScreenSize(int width, int height);

    // ------------------------------------------------------------
    // Function: Draw
    // Purpose:  Render the current animation frame.
    //           Screen position is derived from the active clip's "align"
    //           field (parsed from JSON) and the screen size set via
    //           SetScreenSize().  No pixel coordinates are passed in;
    //           all positioning logic lives here, driven by data.
    // Parameters:
    //   context — immediate device context for this frame
    //   scale   — uniform scale factor (1.0 = original pixel size)
    // ------------------------------------------------------------
    void Draw(ID3D11DeviceContext* context, float scale = 1.0f);

    void Shutdown();

    // Returns the active clip name (useful for debug HUD).
    const std::string& GetActiveClip() const { return mActiveClipName; }

    // Returns the current 0-based frame index within the active clip.
    int GetFrameIndex() const { return mFrameIndex; }

private:
    // ---------- GPU resources ----------
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;

    // ---------- Sprite sheet data ----------
    SpriteSheet mSheet;

    // ---------- Screen dimensions ----------
    // Updated by SetScreenSize(); used in Draw() to compute
    // the anchor position from the active clip's SpriteAlign.
    int mScreenW = 0;
    int mScreenH = 0;

    // ---------- Animation state ----------
    std::string mActiveClipName;
    int         mFrameIndex  = 0;
    float       mFrameTimer  = 0.0f;   // seconds elapsed on the current frame

    // Pointer into mSheet.animations — valid as long as mSheet lives.
    const AnimationClip* mActiveClip = nullptr;

    // ------------------------------------------------------------
    // Helper: compute the screen-space draw position from a SpriteAlign value.
    // Returns the pixel coordinate (top-left origin) where the sprite's
    // pivot should land, derived purely from mScreenW/H and the align enum.
    // ------------------------------------------------------------
    DirectX::XMFLOAT2 AlignToPosition(SpriteAlign align) const;
};
