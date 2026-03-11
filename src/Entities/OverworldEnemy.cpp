// ============================================================
// File: OverworldEnemy.cpp
// Responsibility: Stationary overworld enemy — idle animation + collision.
//
// Key behaviours:
//   - Loads its own SpriteSheet from mData.jsonPath at construction.
//   - Plays the "idle" clip specified by mData.idleClip; no other clips.
//   - IsPlayerNearby() does a simple Euclidean distance test.
//   - No movement AI — stationary for the MVP.  Future extensions may add
//     patrol paths, aggro detection, etc. here without touching PlayState.
//
// Ownership:
//   SceneGraph holds a unique_ptr<OverworldEnemy>.
//   PlayState holds a non-owning OverworldEnemy* ONLY to call
//   IsPlayerNearby() and GetEncounterData(); it never calls Update/Render.
//
// Lifetime:
//   Constructed in PlayState::OnEnter() via SceneGraph::Spawn<OverworldEnemy>().
//   Destroyed in SceneGraph::Clear() called from PlayState::OnExit().
//
// Common mistakes:
//   1. Calling Render() when mInitialized == false -> WorldSpriteRenderer crash.
//      The mInitialized guard in Render() prevents this.
//   2. Using combat stats (hp, atk) from mData here — this file only uses
//      texturePath, jsonPath, idleClip, and contactRadius.
//   3. Forgetting to call mRenderer.Shutdown() in the destructor -> GPU leak.
// ============================================================
#include "OverworldEnemy.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <cmath>

// ------------------------------------------------------------
// Constructor
// Purpose:
//   1. Copy encounter data (all stats + sprite paths owned here).
//   2. Load the SpriteSheet from mData.jsonPath.
//   3. Initialize WorldSpriteRenderer and start the idle clip.
// Why copy data by value?
//   EnemyEncounterData is a plain struct (< 200 bytes).
//   Value semantics make the lifetime of this data unambiguous:
//   OverworldEnemy is the single owner — no dangling reference risk.
// ------------------------------------------------------------
OverworldEnemy::OverworldEnemy(ID3D11Device*       device,
                               ID3D11DeviceContext* context,
                               EnemyEncounterData   data,
                               float                worldX,
                               float                worldY,
                               Camera2D*            camera)
    : mData   (std::move(data))
    , mWorldX (worldX)
    , mWorldY (worldY)
    , mCamera (camera)
{
    // Load the sprite sheet that describes animation clips and frame layout.
    // mData.jsonPath is e.g. "assets/animations/skeleton.json".
    if (!JsonLoader::LoadSpriteSheet(mData.jsonPath, mSheet))
    {
        LOG("[OverworldEnemy] ERROR — Failed to load sprite sheet: '%s'",
            mData.jsonPath.c_str());
        return;  // mInitialized stays false; Render() will skip safely.
    }

    // Initialize GPU resources: texture loaded from mData.texturePath,
    // SpriteBatch created, and the idle clip assigned as the active clip.
    if (!mRenderer.Initialize(device, context, mData.texturePath, mSheet))
    {
        LOG("[OverworldEnemy] ERROR — WorldSpriteRenderer::Initialize failed for '%ls'",
            mData.texturePath.c_str());
        return;
    }

    // Start the idle animation specified in the encounter data.
    // "idle" is the expected clip name; any mismatch is a data authoring error.
    mRenderer.PlayClip(mData.idleClip);

    mInitialized = true;
    LOG("[OverworldEnemy] Spawned '%s' at world (%.1f, %.1f).",
        mData.name.c_str(), mWorldX, mWorldY);
}

// ------------------------------------------------------------
// Destructor
// Purpose:
//   Release GPU resources held by WorldSpriteRenderer.
//   The D3D debug layer reports ID3D11Texture2D leaks if Shutdown() is skipped.
// ------------------------------------------------------------
OverworldEnemy::~OverworldEnemy()
{
    if (mInitialized)
    {
        // Release the SpriteBatch, texture SRV, D3D states, and blend state.
        // ComPtr members automatically release on destruction, but Shutdown()
        // also removes any event subscriptions inside the renderer (if any).
        mRenderer.Shutdown();
    }
}

// ------------------------------------------------------------
// Update
// Purpose:
//   Advance the idle animation frame timer by dt.
//   No logic other than animation — movement and collision are handled
//   by the caller (PlayState) which has the player position context.
// ------------------------------------------------------------
void OverworldEnemy::Update(float dt)
{
    if (!mInitialized) return;
    mRenderer.Update(dt);
}

// ------------------------------------------------------------
// Render
// Purpose:
//   Draw the enemy sprite at its world-space anchor using the camera transform.
//   The enemy faces LEFT by default (flipX = true) to stand opposite the player.
// Why flipX = true?
//   Enemies in the encounter schema face left (same convention as battle slots).
//   Flipping at the renderer level avoids needing mirrored sprite sheets.
// ------------------------------------------------------------
void OverworldEnemy::Render(ID3D11DeviceContext* ctx)
{
    if (!mInitialized || !mCamera) return;

    // Draw the sprite at the enemy's world position.
    // flipX = true: enemies face left; player character faces right.
    // The renderer applies the camera view transform before drawing.
    // Draw signature: (ctx, camera, worldX, worldY, scale, flipX)
    mRenderer.Draw(ctx, *mCamera, mWorldX, mWorldY, 1.0f, /*flipX=*/true);
}

// ------------------------------------------------------------
// IsPlayerNearby
// Purpose:
//   Returns true when the player's world-space position (px, py)
//   is within mData.contactRadius of this enemy's anchor point.
//
// Why Euclidean distance?
//   AABB checks can produce false positives at corners.  For circular
//   characters the distance test is the most accurate and cheapest option.
//   contactRadius is set in the JSON per-enemy so tuning is data-driven.
// ------------------------------------------------------------
bool OverworldEnemy::IsPlayerNearby(float px, float py) const
{
    const float dx = px - mWorldX;
    const float dy = py - mWorldY;

    // Compare squared distance to avoid a sqrt — sufficient for a boolean result.
    const float distSq  = dx * dx + dy * dy;
    const float radiusSq = mData.contactRadius * mData.contactRadius;
    return distSq <= radiusSq;
}
