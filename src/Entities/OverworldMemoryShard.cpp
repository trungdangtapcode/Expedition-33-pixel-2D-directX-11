// ============================================================
// File: OverworldMemoryShard.cpp
// Responsibility: Implement world-space memory shard rendering and proximity.
//
// Common mistakes:
//   1. Granting rewards from the entity -> breaks save/progress ownership.
//   2. Rendering as screen UI -> shard stops belonging to the map.
//   3. Keeping collected shards alive -> save/load can duplicate rewards.
// ============================================================
#include "OverworldMemoryShard.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <WICTextureLoader.h>
#include <cmath>
#include <utility>

// ------------------------------------------------------------
// Function: OverworldMemoryShard
// Purpose:
//   Load the generated shard texture and create SpriteBatch state.
// Why:
//   The overworld should own visible exploration rewards as SceneGraph
//   objects, not as hardcoded draw calls in OverworldState.
// ------------------------------------------------------------
OverworldMemoryShard::OverworldMemoryShard(ID3D11Device* device,
                                           ID3D11DeviceContext* context,
                                           OverworldMemoryShardData data,
                                           Camera2D* camera)
    : mData(std::move(data))
    , mCamera(camera)
{
    const HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        mData.texturePath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        DirectX::WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mTextureSRV.GetAddressOf());

    if (FAILED(hr))
    {
        LOG("[OverworldMemoryShard] ERROR: Failed to load texture '%ls' for shard '%s' (0x%08X).",
            mData.texturePath.c_str(),
            mData.id.c_str(),
            hr);
        return;
    }

    mStates = std::make_unique<DirectX::CommonStates>(device);
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mInitialized = true;

    LOG("[OverworldMemoryShard] Spawned '%s' at world (%.1f, %.1f).",
        mData.id.c_str(),
        mData.worldX,
        mData.worldY);
}

OverworldMemoryShard::~OverworldMemoryShard()
{
    Shutdown();
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance the shard's idle bob timer.
// Why:
//   A small motion distinguishes collectibles from static map decoration
//   without requiring an animated sprite sheet.
// ------------------------------------------------------------
void OverworldMemoryShard::Update(float dt)
{
    if (!mInitialized) return;
    mElapsed += dt;
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the generated shard texture in world space.
// Why:
//   The collectible should be occluded and camera-transformed consistently
//   with other SceneGraph entities.
// ------------------------------------------------------------
void OverworldMemoryShard::Render(ID3D11DeviceContext* ctx)
{
    if (!mInitialized || !mAlive || !mCamera || !mSpriteBatch || !mStates) return;

    const float bob = std::sin(mElapsed * mData.bobSpeed) * mData.bobAmplitude;
    const float pulse = 0.92f + 0.08f * (0.5f + 0.5f * std::sin(mElapsed * mData.bobSpeed * 1.7f));
    const DirectX::XMFLOAT2 position(mData.worldX, mData.worldY + bob);
    const DirectX::XMFLOAT2 origin(32.0f, 32.0f);

    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->AlphaBlend(),
        mStates->PointClamp(),
        nullptr,
        nullptr,
        nullptr,
        mCamera->GetViewMatrix());

    mSpriteBatch->Draw(
        mTextureSRV.Get(),
        position,
        nullptr,
        DirectX::XMVectorSet(pulse, pulse, pulse, 1.0f),
        0.0f,
        origin,
        mData.scale);

    mSpriteBatch->End();
}

bool OverworldMemoryShard::IsPlayerNearby(float px, float py) const
{
    const float dx = px - mData.worldX;
    const float dy = py - mData.worldY;
    const float r = mData.contactRadius;
    return (dx * dx + dy * dy) <= (r * r);
}

std::string OverworldMemoryShard::GetDisplayName() const
{
    return LocalizationManager::Get().TextOrFallback(
        mData.displayNameKey,
        mData.displayName.empty() ? mData.id : mData.displayName);
}

void OverworldMemoryShard::Collect()
{
    mAlive = false;
}

void OverworldMemoryShard::Shutdown()
{
    mTextureSRV.Reset();
    mSpriteBatch.reset();
    mStates.reset();
    mInitialized = false;
}
