// ============================================================
// File: CheckpointCampfire.cpp
// Responsibility: Implement overworld campfire checkpoint rendering and
//                 proximity checks.
// ============================================================
#include "CheckpointCampfire.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <cmath>
#include <utility>

CheckpointCampfire::CheckpointCampfire(ID3D11Device* device,
                                       ID3D11DeviceContext* context,
                                       CheckpointCampfireData data,
                                       Camera2D* camera)
    : mData(std::move(data))
    , mCamera(camera)
{
    if (!JsonLoader::LoadSpriteSheet(mData.jsonPath, mSheet))
    {
        LOG("[CheckpointCampfire] ERROR: Failed to load sprite sheet '%s'.",
            mData.jsonPath.c_str());
        return;
    }

    if (!mRenderer.Initialize(device, context, mData.texturePath, mSheet))
    {
        LOG("[CheckpointCampfire] ERROR: Failed to initialize sprite '%ls'.",
            mData.texturePath.c_str());
        return;
    }

    mRenderer.PlayClip(mData.idleClip);
    mInitialized = true;

    LOG("[CheckpointCampfire] Spawned '%s' at world (%.1f, %.1f).",
        mData.id.c_str(), mData.worldX, mData.worldY);
}

CheckpointCampfire::~CheckpointCampfire()
{
    if (mInitialized)
    {
        mRenderer.Shutdown();
    }
}

void CheckpointCampfire::Update(float dt)
{
    if (!mInitialized) return;
    mRenderer.Update(dt);
}

void CheckpointCampfire::Render(ID3D11DeviceContext* ctx)
{
    if (!mInitialized || !mCamera) return;
    mRenderer.Draw(ctx, *mCamera, mData.worldX, mData.worldY, mData.renderScale, false);
}

bool CheckpointCampfire::IsPlayerNearby(float px, float py) const
{
    const float dx = px - mData.worldX;
    const float dy = py - mData.worldY;
    const float distSq = dx * dx + dy * dy;
    const float radiusSq = mData.contactRadius * mData.contactRadius;
    return distSq <= radiusSq;
}
