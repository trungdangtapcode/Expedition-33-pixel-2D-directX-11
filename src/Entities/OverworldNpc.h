// ============================================================
// File: OverworldNpc.h
// Responsibility: Render and expose interaction data for a story NPC.
//
// Architecture:
//   The NPC is a passive SceneGraph entity. It owns only its animation sprite
//   and data loaded by OverworldState. Dialogue ownership stays in DialogueState.
//
// Owns:
//   WorldSpriteRenderer - animated overworld sprite GPU resources.
//   SpriteSheet         - animation clip descriptors loaded from JSON.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter via SceneGraph::Spawn.
//   Destroyed in -> SceneGraph::Clear via unique_ptr cleanup.
//
// Important:
//   - Route-block data is exposed through narrow getters.
//   - Story progression is read from GameProgress, never stored here.
// ============================================================
#pragma once

#include "../Renderer/Camera.h"
#include "../Renderer/SpriteSheet.h"
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Scene/IGameObject.h"
#include <d3d11.h>
#include <string>

struct OverworldNpcData
{
    std::string id;
    std::string displayName;
    std::string displayNameKey;
    std::wstring texturePath;
    std::string jsonPath;
    std::string idleClip = "idle";
    float worldX = 0.0f;
    float worldY = 0.0f;
    float contactRadius = 96.0f;
    float renderScale = 1.0f;
    bool facingLeft = true;

    std::string dialoguePath;
    std::string repeatDialoguePath;
    std::string completionFlag;
    std::string showIfFlag;
    std::string hideIfFlag;

    std::string routeBlockUntilFlag;
    float blockMinX = 0.0f;
    float blockMinY = 0.0f;
    float blockMaxX = 0.0f;
    float blockMaxY = 0.0f;
    float pushbackX = 0.0f;
    float pushbackY = 0.0f;
};

class OverworldNpc : public IGameObject
{
public:
    OverworldNpc(ID3D11Device* device,
                 ID3D11DeviceContext* context,
                 OverworldNpcData data,
                 Camera2D* camera);
    ~OverworldNpc() override;

    void Update(float dt) override;
    void Render(ID3D11DeviceContext* ctx) override;

    int GetLayer() const override { return 50; }
    float GetSortY() const override { return mData.worldY; }
    bool IsAlive() const override { return mAlive; }

    bool IsPlayerNearby(float px, float py) const;
    bool IsPlayerInsideRouteBlock(float px, float py) const;
    bool IsRouteBlockActive() const;

    const OverworldNpcData& GetData() const { return mData; }
    std::string GetActiveDialoguePath() const;
    std::string GetDisplayName() const;
    void Hide();

private:
    void Shutdown();
    bool IsStoryVisible() const;

    OverworldNpcData mData;
    Camera2D* mCamera = nullptr;
    WorldSpriteRenderer mRenderer;
    SpriteSheet mSheet;
    bool mInitialized = false;
    bool mAlive = true;
};
