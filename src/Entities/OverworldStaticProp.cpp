// ============================================================
// File: OverworldStaticProp.cpp
// Responsibility: Implement world-space rendering for static props.
//
// Rendering:
//   Uses SpriteBatch with Camera2D::GetViewMatrix(), the same transform
//   convention as tile and character rendering. The source rectangle comes
//   from the prop data so one object atlas can hold many props.
//
// Common mistakes:
//   1. Drawing large props in a background tile layer makes the player
//      appear on top of roofs, ruins, and tents incorrectly.
//   2. Using linear sampling blurs pixel-art atlas edges while the camera
//      moves; PointClamp preserves authored pixels.
//   3. Passing Camera2D::GetViewProjectionMatrix() to SpriteBatch double
//      projects the sprite and makes it disappear.
// ============================================================
#include "OverworldStaticProp.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <WICTextureLoader.h>
#include <utility>

// ------------------------------------------------------------
// Function: OverworldStaticProp
// Purpose:
//   Load the configured atlas texture and create SpriteBatch state.
// Why:
//   Each prop owns its draw resources today because the project has no
//   shared ResourceManager yet. Keeping the constructor data-driven still
//   gives map authors control without touching C++.
// Parameters:
//   device  - D3D11 device used to create texture resources.
//   context - D3D11 context stored by SpriteBatch.
//   data    - prop placement and source rectangle loaded from JSON.
//   camera  - non-owning pointer; OverworldState owns the camera.
// Caveats:
//   - A failed texture load leaves the prop inert rather than crashing.
//   - The camera must outlive this entity.
// ------------------------------------------------------------
OverworldStaticProp::OverworldStaticProp(ID3D11Device* device,
                                         ID3D11DeviceContext* context,
                                         OverworldStaticPropData data,
                                         Camera2D* camera)
    : mData(std::move(data))
    , mCamera(camera)
{
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
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
        LOG("[OverworldStaticProp] ERROR: Failed to load texture '%ls' for prop '%s' (0x%08X).",
            mData.texturePath.c_str(),
            mData.id.c_str(),
            hr);
        return;
    }

    mStates = std::make_unique<DirectX::CommonStates>(device);
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mInitialized = true;

    LOG("[OverworldStaticProp] Spawned '%s' at world (%.1f, %.1f).",
        mData.id.c_str(),
        mData.worldX,
        mData.worldY);
}

OverworldStaticProp::~OverworldStaticProp()
{
    Shutdown();
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Static props have no per-frame simulation in V1.
// Why:
//   Keeping the method explicit satisfies IGameObject and leaves a future
//   hook for wind sway, glow, or interactable prop state without changing
//   SceneGraph.
// ------------------------------------------------------------
void OverworldStaticProp::Update(float dt)
{
    (void)dt;
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the atlas source rectangle at the configured world anchor.
// Why:
//   The pivot is authored per prop so the same atlas can contain signs,
//   ruins, tents, and shrine objects with correct Y-sort feet positions.
// Parameters:
//   ctx - D3D11 context for this frame.
// Caveats:
//   - Skips rendering when initialization failed.
//   - Uses world-space camera transform, never screen-space coordinates.
// ------------------------------------------------------------
void OverworldStaticProp::Render(ID3D11DeviceContext* ctx)
{
    if (!mInitialized || !mCamera || !mSpriteBatch || !mStates) return;
    if (mData.sourceWidth <= 0 || mData.sourceHeight <= 0) return;

    const RECT sourceRect = {
        mData.sourceX,
        mData.sourceY,
        mData.sourceX + mData.sourceWidth,
        mData.sourceY + mData.sourceHeight
    };

    const DirectX::XMFLOAT2 position(mData.worldX, mData.worldY);
    const DirectX::XMFLOAT2 origin(mData.pivotX, mData.pivotY);

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
        &sourceRect,
        DirectX::Colors::White,
        0.0f,
        origin,
        mData.scale);

    mSpriteBatch->End();
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Release all GPU resources owned by the prop.
// Why:
//   Deterministic teardown keeps the DirectX debug layer clean when the
//   state exits or the device is recreated.
// ------------------------------------------------------------
void OverworldStaticProp::Shutdown()
{
    mTextureSRV.Reset();
    mSpriteBatch.reset();
    mStates.reset();
    mInitialized = false;
}
