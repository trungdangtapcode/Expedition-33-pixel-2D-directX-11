// ============================================================
// File: WorldRenderer.h
// Responsibility: Draw world-space animated sprites transformed
//                 through a Camera into screen-space pixels.
//
// WorldRenderer is purpose-built for game-world objects:
//   characters walking on a map, enemies, projectiles, NPCs.
// These objects have a position in world units that the Camera
// maps to screen pixels every frame.
//
// Key design constraints:
//   - REQUIRES a Camera reference on every Draw() call.
//     The camera converts world (X, Y) → screen (px, py).
//   - Does NOT know or store screen size — the Camera + screen
//     dimensions passed to Draw() provide all needed layout info.
//   - Frame slicing is HORIZONTAL ONLY (single-row sprite sheet).
//     An assert fires in Initialize() if sheetHeight != frameHeight.
//   - The device context is passed per-frame; never stored.
//
// Contrast with UIRenderer:
//   UIRenderer  — screen-space, no Camera, named anchor methods.
//   WorldRenderer — world-space, Camera required, world X/Y params.
//
// Typical usage:
//   WorldRenderer mVersoBattle;
//   mVersoBattle.Initialize(device, context, L"assets/animations/verso.png", sheet);
//   mVersoBattle.PlayClip("idle");
//   // each frame:
//   mVersoBattle.Update(dt);
//   mVersoBattle.Draw(ctx, mCamera, versoWorldX, versoWorldY, screenW, screenH, scale);
//
// Owns:
//   ID3D11ShaderResourceView  mTextureSRV  — GPU texture
//   SpriteBatch               mSpriteBatch — queues draw calls
//
// Lifetime:
//   Created in  → WorldRenderer::Initialize()  (called from state OnEnter)
//   Destroyed in → WorldRenderer::Shutdown()   (called from state OnExit)
//
// Common mistakes:
//   1. Using WorldRenderer for UI elements — they will drift with the camera.
//   2. Passing world coordinates in pixels instead of world units —
//      the zoom factor will produce incorrect scaling.
//   3. Multi-row sheets — the assert in Initialize() fires immediately.
// ============================================================
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <WICTextureLoader.h>
#include <memory>
#include <string>
#include "SpriteSheet.h"
#include "Camera.h"

class WorldRenderer
{
public:
    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose:
    //   Load the sprite sheet texture and create the SpriteBatch.
    //   Asserts that the sheet is single-row (sheetHeight == frameHeight).
    // Parameters:
    //   device      — used once for texture upload + SpriteBatch creation
    //   context     — passed to SpriteBatch constructor
    //   texturePath — wide-string path to the PNG/JPEG texture
    //   sheet       — sprite sheet descriptor (frame sizes, animations)
    // Returns:
    //   true  — ready to call Draw()
    //   false — texture or SpriteBatch creation failed
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device*        device,
                    ID3D11DeviceContext*  context,
                    const std::wstring&  texturePath,
                    const SpriteSheet&   sheet);

    // ------------------------------------------------------------
    // Function: PlayClip
    // Purpose:
    //   Activate a named animation clip.  Resets frame index and timer.
    //   No-op if the clip is already active.
    // ------------------------------------------------------------
    void PlayClip(const std::string& clipName);

    // ------------------------------------------------------------
    // Function: Update
    // Purpose:
    //   Advance the animation frame timer.
    //   Must be called once per frame before Draw().
    // Parameters:
    //   dt — seconds since last frame
    // ------------------------------------------------------------
    void Update(float dt);

    // ------------------------------------------------------------
    // Function: Draw
    // Purpose:
    //   Pass the Camera2D's ViewProjection matrix to SpriteBatch::Begin()
    //   so the GPU Vertex Shader applies the full camera transform to every
    //   sprite vertex.  The CPU only needs to supply the world-space position.
    //
    //   GPU transform pipeline per vertex:
    //     clipPos = float4(worldPos, 0, 1) * ViewProjMatrix
    //
    // Parameters:
    //   context        — immediate device context for this frame
    //   camera         — Camera2D whose Update() has been called this frame
    //   worldX, worldY — pivot position of the sprite in world units
    //   scale          — additional artist scale on top of camera zoom
    //                    (1.0 = camera zoom only; 2.0 = boss-size override)
    //
    // Why remove screenW/H from the signature?
    //   Camera2D already stores screen dimensions (needed to build its
    //   projection matrix).  Passing them again as separate parameters
    //   was redundant and created a possible mismatch between the camera's
    //   internal projection and the caller-supplied size.
    // ------------------------------------------------------------
    void Draw(ID3D11DeviceContext* context,
              const Camera2D&      camera,
              float worldX, float worldY,
              float scale = 1.0f);

    void Shutdown();

    // --- Accessors ---
    const std::string& GetActiveClip() const { return mActiveClipName; }
    int  GetFrameIndex()               const { return mFrameIndex; }
    bool IsInitialized()               const { return mSpriteBatch != nullptr; }

private:
    // ---------- GPU resources ----------
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;

    // ---------- Sprite sheet data ----------
    SpriteSheet mSheet;

    // ---------- Animation state ----------
    std::string          mActiveClipName;
    int                  mFrameIndex  = 0;
    float                mFrameTimer  = 0.0f;
    const AnimationClip* mActiveClip  = nullptr;
};
