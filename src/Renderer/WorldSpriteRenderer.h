// ============================================================
// File: WorldSpriteRenderer.h
// Responsibility: Draw an animated sprite at a WORLD-SPACE position.
//
// Key difference from UIRenderer / SpriteRenderer:
//   UIRenderer anchors to SCREEN pixels (bottom-center of the viewport).
//   WorldSpriteRenderer anchors to a WORLD coordinate (e.g. player feet).
//   The camera's ViewProjection matrix is forwarded to SpriteBatch::Begin()
//   as the 7th argument; the GPU then transforms every sprite vertex from
//   world space → clip space.  The caller submits world-unit coordinates.
//
// Pivot / bottom-center semantics:
//   SpriteBatch::Draw accepts an 'origin' vector in SOURCE-RECT-LOCAL pixels.
//   We map clip.pivotX / clip.pivotY directly from JSON.
//   For a 128×128 frame with pivot=[64,128] (bottom-center):
//     pivotX=64  → horizontal center of the frame lands at worldX
//     pivotY=128 → bottom edge of the frame lands at worldY (feet)
//   No caller arithmetic needed — pivot encodes all alignment information.
//
// UV slicing:
//   srcRect is computed entirely from SpriteSheet data — no hardcoded
//   pixel offsets anywhere.  Multi-row sprite sheets are supported via
//   framesPerRow().
//
// Owns:
//   ID3D11ShaderResourceView  mTextureSRV   — GPU texture view (ComPtr)
//   SpriteBatch               mSpriteBatch  — DirectXTK draw queue (unique_ptr)
//   ID3D11DepthStencilState   mDepthNone    — depth test OFF, depth write OFF
//   ID3D11BlendState          mAlphaBlend   — standard pre-multiplied alpha
//
// Lifetime:
//   Created in  → WorldSpriteRenderer::Initialize()  (PlayState::OnEnter)
//   Destroyed in → WorldSpriteRenderer::Shutdown()   (PlayState::OnExit)
//
// Common mistakes:
//   1. Forgetting to call camera.Update() before Draw() — stale ViewProj
//      matrix causes the sprite to lag one frame behind.
//   2. Passing a screen-pixel position to Draw() instead of a world position
//      — the sprite will appear far outside the visible area.
//   3. Omitting SetViewport() before SpriteBatch::Begin() — SpriteBatch calls
//      RSGetViewports internally; if the count is 0 it throws an exception.
// ============================================================
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>        // DirectXTK
#include <WICTextureLoader.h>   // DirectXTK
#include <memory>
#include <string>

#include "SpriteSheet.h"
#include "Camera.h"

class WorldSpriteRenderer
{
public:
    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose:  Load the sprite-sheet texture, create the SpriteBatch,
    //           and build the D3D11 render states used every frame.
    // Parameters:
    //   device      — used once to upload the texture + create states
    //   context     — stored; SpriteBatch::Begin/End requires it each frame
    //   texturePath — path to the PNG (e.g. L"assets/animations/verso.png")
    //   sheet       — sprite-sheet descriptor loaded from JSON
    // Returns:
    //   true  — ready to call PlayClip / Update / Draw
    //   false — texture or state creation failed (error logged)
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device*        device,
                    ID3D11DeviceContext*  context,
                    const std::wstring&  texturePath,
                    const SpriteSheet&   sheet);

    // ------------------------------------------------------------
    // Function: PlayClip
    // Purpose:  Switch the active animation clip by name.
    //           Resets mFrameIndex=0 and mFrameTimer=0.
    // Note:     No-op if clipName is already the active clip — avoids
    //           visible stutter on repeated calls from game logic.
    // ------------------------------------------------------------
    void PlayClip(const std::string& clipName);

    // ------------------------------------------------------------
    // Function: Update
    // Purpose:  Advance the animation frame timer by dt seconds.
    //           Must be called once per frame before Draw().
    // Why subtract instead of reset?
    //   Subtracting the frame duration carries the remainder forward,
    //   preventing animation drift at low frame rates where dt can
    //   exceed the frame duration in one step.
    // ------------------------------------------------------------
    void Update(float dt);

    // ------------------------------------------------------------
    // Function: Draw
    // Purpose:
    //   Issue one SpriteBatch draw call placing the active frame so that
    //   the JSON pivot lands exactly at (worldX, worldY) in world space.
    //
    //   SpriteBatch::Begin() receives camera.GetViewMatrix() —
    //   the GPU then transforms each sprite vertex from world → screen.
    //   The caller never converts coordinates manually.
    //
    // Parameters:
    //   context         — D3D11 device context for this frame
    //   camera          — Camera2D whose view matrix transforms the sprite
    //   worldX, worldY  — world-space position where the pivot lands
    //   scale           — uniform scale applied on top of the sprite size
    //   flipX           — when true, mirrors the sprite horizontally
    //                     (use for left-facing movement on a right-facing sprite)
    //
    // Preconditions:
    //   Initialize() must have returned true.
    //   PlayClip() must have been called with a valid clip name.
    //   camera.Update() must have been called this frame.
    // ------------------------------------------------------------
    void Draw(ID3D11DeviceContext* context,
              const Camera2D&     camera,
              float               worldX,
              float               worldY,
              float               scale = 1.0f,
              bool                flipX = false);

    // ------------------------------------------------------------
    // Function: Shutdown
    // Purpose:  Release all GPU resources deterministically.
    //           Called from PlayState::OnExit() before D3D device teardown.
    // ------------------------------------------------------------
    void Shutdown();

private:
    // ------------------------------------------------------------
    // GPU resources — all reference-counted via ComPtr / unique_ptr.
    // Released automatically when Shutdown() is called or the object
    // goes out of scope (as long as Shutdown was called first).
    // ------------------------------------------------------------

    // Shader resource view wrapping the atlas texture.
    // Owner: WorldSpriteRenderer::Initialize()
    // Released: mTextureSRV.Reset() in Shutdown()
    // Leak consequence: DX debug layer reports 1 live SRV + underlying texture.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;

    // DirectXTK SpriteBatch — batches Draw calls; flushes on End().
    // Owner: WorldSpriteRenderer::Initialize() via make_unique
    // Released: mSpriteBatch.reset() in Shutdown()
    // Leak consequence: internal VB/IB/CB remain live; DX debug reports them.
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;

    // Depth-stencil state with depth test AND write disabled.
    // Required: SpriteBatch draws 2-D quads at z=0; depth test would fail or
    //   clip sprites depending on what left the depth buffer from 3-D draws.
    // Owner: WorldSpriteRenderer::Initialize()
    // Released: mDepthNone.Reset() in Shutdown()
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthNone;

    // Alpha-blend state — standard one-minus-source-alpha blending.
    // Passed explicitly to SpriteBatch::Begin() so SpriteBatch never queries
    // or modifies the pipeline blend state outside our Begin/End block.
    // Owner: WorldSpriteRenderer::Initialize()
    // Released: mAlphaBlend.Reset() in Shutdown()
    Microsoft::WRL::ComPtr<ID3D11BlendState> mAlphaBlend;

    // Sprite-sheet data — frame dimensions, animation clips.
    // Value type (no GPU resource) — cheap to copy.
    SpriteSheet mSheet;

    // Pointer into mSheet.animations — valid as long as mSheet is alive.
    // Set by PlayClip(); nullptr until the first PlayClip() call.
    const AnimationClip* mActiveClip     = nullptr;
    std::string          mActiveClipName;

    // Animation playback state — driven by Update(dt).
    int   mFrameIndex = 0;   // index within the active clip [0, numFrames)
    float mFrameTimer = 0.0f; // accumulated time within the current frame (seconds)
};
