// ============================================================
// File: ObjectiveBeaconRenderer.h
// Responsibility: Render a world-space objective marker for the active
//                 overworld objective waypoint.
//
// Owns:
//   PointerRenderer mPointer - sprite batch, texture SRV, animation timing.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter()
//   Destroyed in -> OverworldState::OnExit()
//
// Important:
//   - Visual asset paths and hide distance are loaded from JSON.
//   - The active objective is supplied each frame by ObjectiveDirector.
//   - The marker is world-space, so it follows camera pan/zoom/rotation.
//
// Common mistakes:
//   1. Drawing the marker as screen UI -> it no longer points at the route.
//   2. Keeping it visible at point-blank range -> it covers enemies/NPCs.
//   3. Hardcoding texture paths in OverworldState -> art swaps require code.
// ============================================================
#pragma once

#include "PointerRenderer.h"
#include "../Renderer/Camera.h"
#include "../Systems/ObjectiveDirector.h"
#include <d3d11.h>
#include <string>

struct ObjectiveBeaconConfig
{
    bool enabled = true;
    std::string texturePath = "assets/UI/objective_beacon.png";
    std::string layoutPath = "assets/UI/objective-beacon-ui.json";
    float hideWithinDistanceUnits = 48.0f;
    float hideEnemyWithinDistanceUnits = 520.0f;
    float hideNpcWithinDistanceUnits = 420.0f;
    float hideOnScreenPaddingPx = 80.0f;
    float enemyDrawOffsetY = -96.0f;
    float npcDrawOffsetY = -80.0f;
};

class ObjectiveBeaconRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    int screenW,
                    int screenH);

    void Update(float dt);
    void Render(ID3D11DeviceContext* context,
                const ObjectiveView& objective,
                float playerX,
                float playerY,
                const Camera2D& camera);
    void SetScreenSize(int screenW, int screenH);
    void Shutdown();

private:
    bool LoadConfig(const std::string& path);
    bool ShouldRender(const ObjectiveView& objective,
                      float playerX,
                      float playerY,
                      const Camera2D& camera) const;
    float ResolveDrawOffsetY(const ObjectiveView& objective) const;
    bool IsInteractableTarget(const ObjectiveView& objective) const;
    float ResolveInteractableHideDistance(const ObjectiveView& objective) const;
    bool IsWaypointVisibleOnScreen(const ObjectiveView& objective,
                                   const Camera2D& camera) const;

    ObjectiveBeaconConfig mConfig;
    PointerRenderer mPointer;
    bool mInitialized = false;
};
