// ============================================================
// File: CheckpointCampfire.h
// Responsibility: Render and expose proximity for an overworld campfire
//                 checkpoint.
//
// Architecture:
//   The campfire is an IGameObject so SceneGraph owns update/render
//   lifetime. It does not save, load, or upgrade directly. OverworldState
//   coordinates input and calls SaveManager, PartyManager, and GameProgress.
//
// Data:
//   Spawn position, contact radius, sprite paths, and upgrade EXP come from
//   data/campfires.json. This keeps checkpoint placement and tuning out of
//   C++ source.
//
// Owns:
//   WorldSpriteRenderer - GPU texture, SpriteBatch, and clip state.
//   SpriteSheet         - frame/clip metadata loaded from JSON.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter via SceneGraph::Spawn.
//   Destroyed in -> SceneGraph::Clear via unique_ptr cleanup.
// ============================================================
#pragma once

#include "../Renderer/Camera.h"
#include "../Renderer/SpriteSheet.h"
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Scene/IGameObject.h"
#include <d3d11.h>
#include <string>

struct CheckpointCampfireData
{
    std::string id;
    std::wstring texturePath;
    std::string jsonPath;
    std::string idleClip = "idle";
    float worldX = 0.0f;
    float worldY = 0.0f;
    float contactRadius = 80.0f;
    float renderScale = 1.0f;
    int upgradeExpReward = 0;
};

class CheckpointCampfire : public IGameObject
{
public:
    CheckpointCampfire(ID3D11Device* device,
                       ID3D11DeviceContext* context,
                       CheckpointCampfireData data,
                       Camera2D* camera);

    ~CheckpointCampfire() override;

    void Update(float dt) override;
    void Render(ID3D11DeviceContext* ctx) override;

    int GetLayer() const override { return 50; }
    float GetSortY() const override { return mData.worldY; }
    bool IsAlive() const override { return true; }

    bool IsPlayerNearby(float px, float py) const;
    const CheckpointCampfireData& GetData() const { return mData; }

private:
    CheckpointCampfireData mData;
    Camera2D* mCamera = nullptr;
    WorldSpriteRenderer mRenderer;
    SpriteSheet mSheet;
    bool mInitialized = false;
};
