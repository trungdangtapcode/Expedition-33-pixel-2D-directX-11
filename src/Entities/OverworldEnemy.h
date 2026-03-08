// ============================================================
// File: OverworldEnemy.h
// Responsibility: A stationary enemy entity in the overworld scene.
//
// Implements IGameObject so SceneGraph drives it transparently —
// PlayState only calls mScene.Update(dt) + mScene.Render(ctx).
//
// The enemy holds an EnemyEncounterData package that is passed to
// BattleState when the player initiates combat.  This ensures the
// battle enemy uses the SAME texture, animation, and stats as the
// overworld entity — no two sources of truth.
//
// Collision:
//   IsPlayerNearby(px, py) performs a simple Euclidean radius test
//   using mData.contactRadius.  PlayState calls this each frame while
//   checking whether to show the "press B to fight" prompt.
//
// Architecture:
//   OverworldEnemy is PASSIVE — it never initiates a battle itself.
//   PlayState is the coordinator: detect proximity + input → push BattleState.
//   This preserves the Single Responsibility Principle: the entity knows
//   where it is and what its data is; the state knows when to trigger.
//
// Owns:
//   WorldSpriteRenderer  mRenderer  — GPU texture + SpriteBatch
//   SpriteSheet          mSheet     — frame/clip data (loaded from JSON)
//
// Lifetime:
//   Created in  → SceneGraph::Spawn<OverworldEnemy>(...)  (PlayState::OnEnter)
//   Destroyed in → SceneGraph::Clear()  (PlayState::OnExit)
//
// Common mistakes:
//   1. Calling GetEncounterData() after SceneGraph::Clear() — the data is gone.
//      Copy it BEFORE Clear() (PlayState copies on B-press, not on exit).
//   2. Hardcoding stats in this file — all values must come from EnemyEncounterData.
//   3. Passing contactRadius = 0 — the proximity check will never fire.
// ============================================================
#pragma once
#include "../Scene/IGameObject.h"
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Renderer/Camera.h"
#include "../Renderer/SpriteSheet.h"
#include "../Battle/EnemyEncounterData.h"
#include <d3d11.h>
#include <string>

class OverworldEnemy : public IGameObject
{
public:
    // ------------------------------------------------------------
    // Constructor
    // Purpose:
    //   Initialize the overworld enemy sprite at a given world position.
    //   GPU resources are created via WorldSpriteRenderer::Initialize().
    //   The sprite sheet is loaded from data.jsonPath.
    //
    // Parameters:
    //   device  — D3D11 device for texture and state creation
    //   context — D3D11 context stored by SpriteBatch
    //   data    — full encounter data package (stats + sprite paths)
    //             Copied by value — caller's copy is not referenced.
    //   worldX/worldY — world-space position of the enemy's feet anchor
    //   camera        — non-owning; must outlive this entity
    // ------------------------------------------------------------
    OverworldEnemy(ID3D11Device*            device,
                   ID3D11DeviceContext*      context,
                   EnemyEncounterData        data,
                   float                     worldX,
                   float                     worldY,
                   Camera2D*                 camera);

    ~OverworldEnemy() override;

    // IGameObject interface — called only by SceneGraph.
    void Update(float dt) override;
    void Render(ID3D11DeviceContext* ctx) override;

    // Layer 48 — rendered below the player (layer 50) so player sprite
    // overlaps enemies when they share the same Y coordinate.
    int  GetLayer() const override { return 48; }

    // Overworld enemies do not die; they remain alive until the scene ends.
    // A future implementation could set mAlive = false after battle victory.
    bool IsAlive() const override  { return mAlive; }

    // ------------------------------------------------------------
    // IsPlayerNearby
    // Purpose:
    //   Returns true if (px, py) is within mData.contactRadius pixels
    //   of this enemy's world-space anchor.
    // Called by:
    //   PlayState::Update() each frame to check if the player can engage.
    // ------------------------------------------------------------
    bool IsPlayerNearby(float px, float py) const;

    // ------------------------------------------------------------
    // MarkDefeated
    // Purpose:
    //   Called by PlayState after the associated battle ends in victory.
    //   Sets mAlive = false so SceneGraph::PurgeDead() removes this enemy,
    //   preventing the player from fighting a defeated enemy again.
    // ------------------------------------------------------------
    void MarkDefeated() { mAlive = false; }

    // Read-only access to the full encounter package.
    // Copy this struct to a local variable BEFORE the SceneGraph is cleared.
    const EnemyEncounterData& GetEncounterData() const { return mData; }

    // World position accessors (used by PlayState for proximity check).
    float GetX() const { return mWorldX; }
    float GetY() const { return mWorldY; }

private:
    EnemyEncounterData  mData;           // full battle package (stats + sprite)
    float               mWorldX;         // world-space anchor X (feet)
    float               mWorldY;         // world-space anchor Y (feet)
    Camera2D*           mCamera;         // non-owning
    WorldSpriteRenderer mRenderer;       // GPU sprite renderer
    SpriteSheet         mSheet;          // frame/clip data loaded from JSON
    bool                mInitialized = false;  // guards Render against partial init
    bool                mAlive       = true;   // false after MarkDefeated()
};
