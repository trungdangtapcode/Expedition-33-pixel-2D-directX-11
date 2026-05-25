// ============================================================
// File: OverworldStaticProp.h
// Responsibility: Render a data-driven static overworld prop with
//                 SceneGraph ownership and Y-sorting.
//
// Architecture:
//   Large props that need to overlap the player cannot live safely in a
//   flat tile layer. This entity lets those props participate in the same
//   IGameObject render ordering as the player, enemies, and campfires.
//
// Data:
//   Placement, source rectangle, pivot, scale, and sort layer come from
//   data/overworld_props.json. This keeps map composition out of C++.
//
// Owns:
//   ID3D11ShaderResourceView - texture loaded from the configured atlas.
//   SpriteBatch             - world-space static sprite draw calls.
//   CommonStates            - alpha blend and point sampling states.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter via SceneGraph::Spawn.
//   Destroyed in -> SceneGraph::Clear via unique_ptr cleanup.
// ============================================================
#pragma once

#include "../Renderer/Camera.h"
#include "../Scene/IGameObject.h"
#include <CommonStates.h>
#include <SpriteBatch.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <wrl/client.h>

struct OverworldStaticPropData
{
    std::string id;
    std::wstring texturePath;
    int sourceX = 0;
    int sourceY = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    float scale = 1.0f;
    int layer = 50;
    float sortYOffset = 0.0f;
};

class OverworldStaticProp : public IGameObject
{
public:
    OverworldStaticProp(ID3D11Device* device,
                        ID3D11DeviceContext* context,
                        OverworldStaticPropData data,
                        Camera2D* camera);
    ~OverworldStaticProp() override;

    void Update(float dt) override;
    void Render(ID3D11DeviceContext* ctx) override;

    int GetLayer() const override { return mData.layer; }
    float GetSortY() const override { return mData.worldY + mData.sortYOffset; }
    bool IsAlive() const override { return true; }

private:
    void Shutdown();

    OverworldStaticPropData mData;
    Camera2D* mCamera = nullptr;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;

    bool mInitialized = false;
};
