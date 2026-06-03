// ============================================================
// File: OverworldMemoryShard.h
// Responsibility: Render and expose interaction data for one
//                 overworld memory-shard collectible.
//
// Architecture:
//   The shard is a passive SceneGraph entity. It does not grant rewards or
//   open dialogue by itself; OverworldState owns interaction consequences.
//
// Owns:
//   ID3D11ShaderResourceView - generated shard texture.
//   SpriteBatch             - world-space sprite drawing.
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
#include <vector>
#include <wrl/client.h>

struct OverworldMemoryShardData
{
    std::string id;
    std::string displayName;
    std::string displayNameKey;
    std::wstring texturePath;
    std::string collectedFlag;
    std::string dialoguePath;
    std::string itemId;
    std::vector<std::string> requiresFlags;
    std::vector<std::string> blockedByFlags;
    int itemAmount = 0;
    int coinReward = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float contactRadius = 82.0f;
    float scale = 0.70f;
    float bobAmplitude = 6.0f;
    float bobSpeed = 3.0f;
    int layer = 51;
    float sortYOffset = -12.0f;
};

class OverworldMemoryShard : public IGameObject
{
public:
    OverworldMemoryShard(ID3D11Device* device,
                         ID3D11DeviceContext* context,
                         OverworldMemoryShardData data,
                         Camera2D* camera);
    ~OverworldMemoryShard() override;

    void Update(float dt) override;
    void Render(ID3D11DeviceContext* ctx) override;

    int GetLayer() const override { return mData.layer; }
    float GetSortY() const override { return mData.worldY + mData.sortYOffset; }
    bool IsAlive() const override { return mAlive; }

    bool IsPlayerNearby(float px, float py) const;
    const OverworldMemoryShardData& GetData() const { return mData; }
    std::string GetDisplayName() const;
    void Collect();

private:
    void Shutdown();

    OverworldMemoryShardData mData;
    Camera2D* mCamera = nullptr;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;

    bool mInitialized = false;
    bool mAlive = true;
    float mElapsed = 0.0f;
};
