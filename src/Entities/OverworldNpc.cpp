// ============================================================
// File: OverworldNpc.cpp
// Responsibility: Implement animated passive story NPCs in the overworld.
//
// Rendering:
//   Uses WorldSpriteRenderer with Camera2D::GetViewMatrix(), matching the
//   player and enemy sprite pipeline so Y-sorting remains consistent.
//
// Common mistakes:
//   1. Letting an NPC push DialogueState directly couples entities to states.
//   2. Hardcoding dialogue ids here makes story iteration require a rebuild.
//   3. Storing completion state here loses progress after save/load.
// ============================================================
#include "OverworldNpc.h"
#include "../Systems/GameProgress.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <cmath>
#include <utility>

namespace
{
    bool HasRequiredFlags(const std::vector<std::string>& flags)
    {
        for (const std::string& flag : flags)
        {
            if (!flag.empty() && !GameProgress::Get().HasFlag(flag))
            {
                return false;
            }
        }
        return true;
    }

    bool HasBlockedFlag(const std::vector<std::string>& flags)
    {
        for (const std::string& flag : flags)
        {
            if (!flag.empty() && GameProgress::Get().HasFlag(flag))
            {
                return true;
            }
        }
        return false;
    }
}

// ------------------------------------------------------------
// Function: OverworldNpc
// Purpose:
//   Load the configured sprite sheet and begin the idle animation.
// Why:
//   NPC placement and asset choice live in JSON, but the entity still owns
//   its renderer so SceneGraph can update and draw it without special cases.
// ------------------------------------------------------------
OverworldNpc::OverworldNpc(ID3D11Device* device,
                           ID3D11DeviceContext* context,
                           OverworldNpcData data,
                           Camera2D* camera)
    : mData(std::move(data))
    , mCamera(camera)
{
    if (!JsonLoader::LoadSpriteSheet(mData.jsonPath, mSheet))
    {
        LOG("[OverworldNpc] ERROR: Failed to load sprite sheet '%s' for '%s'.",
            mData.jsonPath.c_str(),
            mData.id.c_str());
        return;
    }

    if (!mRenderer.Initialize(device, context, mData.texturePath, mSheet))
    {
        LOG("[OverworldNpc] ERROR: Failed to initialize renderer for '%s'.",
            mData.id.c_str());
        return;
    }

    mRenderer.PlayClip(mData.idleClip.empty() ? "idle" : mData.idleClip);
    mInitialized = true;

    LOG("[OverworldNpc] Spawned '%s' at world (%.1f, %.1f).",
        mData.id.c_str(),
        mData.worldX,
        mData.worldY);
}

OverworldNpc::~OverworldNpc()
{
    Shutdown();
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance the NPC animation timer.
// Why:
//   Keeping animation inside the entity lets SceneGraph drive all visible
//   overworld actors through the same IGameObject update path.
// ------------------------------------------------------------
void OverworldNpc::Update(float dt)
{
    if (!mAlive || !mInitialized || !IsStoryVisible()) return;
    mRenderer.Update(dt);
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the NPC at its authored world anchor using the active camera.
// Caveats:
//   - Skips rendering when asset initialization failed.
//   - Uses the data-driven flip flag instead of assuming one sprite direction.
// ------------------------------------------------------------
void OverworldNpc::Render(ID3D11DeviceContext* ctx)
{
    if (!mAlive || !mInitialized || !mCamera || !IsStoryVisible()) return;

    mRenderer.Draw(ctx,
                   *mCamera,
                   mData.worldX,
                   mData.worldY,
                   mData.renderScale,
                   mData.facingLeft);
}

// ------------------------------------------------------------
// Function: IsPlayerNearby
// Purpose:
//   Check whether the player can talk to this NPC.
// Why:
//   Circular range feels better for interact prompts than an AABB corner hit.
// ------------------------------------------------------------
bool OverworldNpc::IsPlayerNearby(float px, float py) const
{
    if (!mAlive || !IsStoryVisible()) return false;

    const float dx = px - mData.worldX;
    const float dy = py - mData.worldY;
    const float radiusSq = mData.contactRadius * mData.contactRadius;
    return dx * dx + dy * dy <= radiusSq;
}

bool OverworldNpc::IsPlayerInsideRouteBlock(float px, float py) const
{
    if (!IsRouteBlockActive()) return false;
    return px >= mData.blockMinX && px <= mData.blockMaxX &&
           py >= mData.blockMinY && py <= mData.blockMaxY;
}

bool OverworldNpc::IsRouteBlockActive() const
{
    if (!IsStoryVisible()) return false;
    if (mData.routeBlockUntilFlag.empty()) return false;
    return !GameProgress::Get().HasFlag(mData.routeBlockUntilFlag);
}

std::string OverworldNpc::GetActiveDialoguePath() const
{
    for (const OverworldNpcDialogueRule& rule : mData.conditionalDialogues)
    {
        if (!rule.dialoguePath.empty() &&
            HasRequiredFlags(rule.requiresFlags) &&
            !HasBlockedFlag(rule.blockedByFlags))
        {
            return rule.dialoguePath;
        }
    }

    if (!mData.completionFlag.empty() &&
        GameProgress::Get().HasFlag(mData.completionFlag) &&
        !mData.repeatDialoguePath.empty())
    {
        return mData.repeatDialoguePath;
    }

    return mData.dialoguePath;
}

std::string OverworldNpc::GetDisplayName() const
{
    return LocalizationManager::Get().TextOrFallback(mData.displayNameKey,
                                                     mData.displayName);
}

// ------------------------------------------------------------
// Function: Hide
// Purpose:
//   Mark this NPC dead so SceneGraph can purge it.
// Why:
//   Story recruitment can happen without rebuilding OverworldState, so the
//   NPC needs a narrow runtime removal hook.
// ------------------------------------------------------------
void OverworldNpc::Hide()
{
    mAlive = false;
}

// ------------------------------------------------------------
// Function: IsStoryVisible
// Purpose:
//   Evaluate data-driven show/hide flags without destroying the entity.
// Why:
//   Maelle should appear immediately after the scout flag is set, even if the
//   entity was spawned earlier while hidden.
// ------------------------------------------------------------
bool OverworldNpc::IsStoryVisible() const
{
    if (!mData.showIfFlag.empty() && !GameProgress::Get().HasFlag(mData.showIfFlag))
    {
        return false;
    }
    if (!mData.hideIfFlag.empty() && GameProgress::Get().HasFlag(mData.hideIfFlag))
    {
        return false;
    }
    return true;
}

void OverworldNpc::Shutdown()
{
    if (mInitialized)
    {
        mRenderer.Shutdown();
        mInitialized = false;
    }
}
