// ============================================================
// File: BattleRenderer.cpp
// Responsibility: Initialize, animate, and render all combatant sprites
//   during a battle — 3 player slots (left side) and 3 enemy slots (right side).
//
// Screen layout (1280x720):
//
//   PLAYER SIDE (left, face right)     ENEMY SIDE (right, face left)
//   Slot 1  screen(200, 280)  back     Slot 1  screen(1080, 280)  back
//   Slot 0  screen(320, 400)  front    Slot 0  screen( 960, 400)  front
//   Slot 2  screen(200, 520)  back     Slot 2  screen(1080, 520)  back
//
// Camera / coordinate conversion:
//   Camera2D at pos=(0,0) zoom=1 maps world origin (0,0) to the SCREEN CENTER
//   (W/2, H/2).  To draw a sprite at screen pixel (px, py), supply world:
//     worldX = px - screenW/2
//     worldY = py - screenH/2
//   Render() performs this via ScreenToWorld() before each Draw() call.
//
// Common mistakes:
//   1. Passing raw screen pixels to Draw() — sprite appears at wrong position.
//      Always subtract (screenW/2, screenH/2) to convert to world space.
//   2. Forgetting flipX for enemy slots — skeletons face the wrong direction.
//   3. Calling Update() before Initialize() — mActiveClip is nullptr → crash.
// ============================================================
#include "BattleRenderer.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   For each occupied slot, load the SpriteSheet JSON, then call
//   WorldSpriteRenderer::Initialize() to upload the texture to the GPU.
//   Immediately play the start clip so the sprite has a valid frame on the
//   first Render() call.
//
// Parameters:
//   device/context — D3D11 objects passed to each WorldSpriteRenderer
//   playerSlots    — array of kMaxSlots slot descriptors for the player team
//   enemySlots     — array of kMaxSlots slot descriptors for the enemy team
//   screenW/H      — render-target size; used to build the flat Camera2D
//
// Why a flat Camera2D?
//   WorldSpriteRenderer::Draw() multiplies sprite vertices by a view matrix.
//   With Camera2D at pos=(0,0) and zoom=1 the view matrix is effectively
//   an identity scale, so the world coordinates supplied to Draw() map
//   directly to screen pixels — no manual conversion needed.
// ------------------------------------------------------------
bool BattleRenderer::Initialize(ID3D11Device*                          device,
                                 ID3D11DeviceContext*                   context,
                                 const std::array<SlotInfo, kMaxSlots>& playerSlots,
                                 const std::array<SlotInfo, kMaxSlots>& enemySlots,
                                 int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;

    // ----------------------------------------------------------------
    // Build the flat battle camera.
    // pos=(0,0), zoom=1 — world origin maps to screen CENTER (W/2, H/2).
    // Sprites are drawn by converting screen pixels to world coords:
    //   worldX = screenX - screenW/2,  worldY = screenY - screenH/2
    // This keeps the Camera2D pipeline intact without a custom projection.
    // ----------------------------------------------------------------
    mCamera = std::make_unique<Camera2D>(screenW, screenH);
    mCamera->SetPosition(0.0f, 0.0f);
    mCamera->SetZoom(1.0f);
    mCamera->Update();

    // ----------------------------------------------------------------
    // Initialize player-side renderers.
    // ----------------------------------------------------------------
    for (int i = 0; i < kMaxSlots; ++i)
    {
        const SlotInfo& info = playerSlots[i];
        if (!info.occupied) { mPlayerActive[i] = false; continue; }

        // Load the SpriteSheet descriptor (frame size, clip list, pivots).
        SpriteSheet sheet;
        if (!JsonLoader::LoadSpriteSheet(info.jsonPath, sheet))
        {
            LOG("[BattleRenderer] Failed to load player sheet: %s", info.jsonPath.c_str());
            return false;
        }

        // Upload PNG to GPU and create SpriteBatch + D3D states.
        if (!mPlayerRenderers[i].Initialize(device, context, info.texturePath, sheet))
        {
            LOG("[BattleRenderer] Failed to init player renderer slot %d", i);
            return false;
        }

        // Start the idle (or configured start) clip immediately so the first
        // Render() call has a valid mActiveClip and does not assert.
        mPlayerRenderers[i].PlayClip(info.startClip);
        mPlayerActive[i] = true;
    }

    // ----------------------------------------------------------------
    // Initialize enemy-side renderers.
    // Exactly the same pattern — only the active flag and renderer array differ.
    // ----------------------------------------------------------------
    for (int i = 0; i < kMaxSlots; ++i)
    {
        const SlotInfo& info = enemySlots[i];
        if (!info.occupied) { mEnemyActive[i] = false; continue; }

        SpriteSheet sheet;
        if (!JsonLoader::LoadSpriteSheet(info.jsonPath, sheet))
        {
            LOG("[BattleRenderer] Failed to load enemy sheet: %s", info.jsonPath.c_str());
            return false;
        }

        if (!mEnemyRenderers[i].Initialize(device, context, info.texturePath, sheet))
        {
            LOG("[BattleRenderer] Failed to init enemy renderer slot %d", i);
            return false;
        }

        mEnemyRenderers[i].PlayClip(info.startClip);
        mEnemyActive[i] = true;
    }

    LOG("[BattleRenderer] Initialized — screen %dx%d", screenW, screenH);
    return true;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance the animation frame timer for every active slot.
//   Must be called once per frame, before Render().
//
// Why update inactive slots is skipped:
//   WorldSpriteRenderer::Update() dereferences mActiveClip; calling it
//   on an uninitialized renderer that never had PlayClip() called is UB.
// ------------------------------------------------------------
void BattleRenderer::Update(float dt)
{
    // Rebuild the camera matrix each frame.
    // For the flat battle camera this is a no-op (pos/zoom never change),
    // but calling it ensures correctness if the camera is ever moved later.
    mCamera->Update();

    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (mPlayerActive[i]) mPlayerRenderers[i].Update(dt);
        if (mEnemyActive[i])  mEnemyRenderers[i].Update(dt);
    }
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw all active combatant sprites at their fixed screen positions.
//   Player sprites face right (flipX=false); enemy sprites face left (flipX=true).
//
// Coordinate conversion:
//   Camera2D at pos=(0,0) maps world (0,0) to screen CENTER (W/2, H/2).
//   To place a sprite at screen pixel (px, py):
//     worldX = px - mScreenW / 2
//     worldY = py - mScreenH / 2
//   The slot tables store screen-pixel positions; conversion happens here.
// ------------------------------------------------------------
void BattleRenderer::Render(ID3D11DeviceContext* context)
{
    // Half-screen offsets — used to convert pixel coords to world coords.
    const float halfW = static_cast<float>(mScreenW) * 0.5f;
    const float halfH = static_cast<float>(mScreenH) * 0.5f;

    // -- Player side ------------------------------------------------
    // Players face right (default sprite orientation) — flipX=false.
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mPlayerActive[i]) continue;
        mPlayerRenderers[i].Draw(
            context,
            *mCamera,
            kPlayerScreenX[i] - halfW,   // screen pixel -> world x
            kPlayerScreenY[i] - halfH,   // screen pixel -> world y
            2.0f,                         // scale up for visibility
            false                         // face right
        );
    }

    // -- Enemy side -------------------------------------------------
    // Enemies face left (flipX=true) so they face the player side.
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mEnemyActive[i]) continue;
        mEnemyRenderers[i].Draw(
            context,
            *mCamera,
            kEnemyScreenX[i] - halfW,    // screen pixel -> world x
            kEnemyScreenY[i] - halfH,    // screen pixel -> world y
            2.0f,                          // scale up for visibility
            true                           // face left — mirror the sprite
        );
    }
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Release all GPU resources owned by each active WorldSpriteRenderer.
//   Renderer slots that were never initialized (inactive) are safe to call
//   Shutdown() on — WorldSpriteRenderer::Shutdown() is a no-op in that case.
// ------------------------------------------------------------
void BattleRenderer::Shutdown()
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        mPlayerRenderers[i].Shutdown();
        mEnemyRenderers[i].Shutdown();
    }

    // Release the flat camera — all memory freed; GPU resources unaffected.
    mCamera.reset();

    LOG("[BattleRenderer] Shutdown complete.");
}
