// ============================================================
// File: IBattleTransitionController.h
// Responsibility: Define an interface for complex transition effects 
//   when triggering a battle in the Overworld. Decouples transition
//   logic (pincushion, zoom, rotation, timing) from OverworldState.
// ============================================================
#pragma once
#include <d3d11.h>
#include "../Battle/EnemyEncounterData.h"
#include "../Entities/OverworldEnemy.h"
#include "../Renderer/Camera.h"

class IBattleTransitionController {
public:
    virtual ~IBattleTransitionController() = default;

    virtual bool Initialize(ID3D11Device* device, int screenW, int screenH) = 0;
    virtual void Shutdown() = 0;
    
    // Called when screen resizes
    virtual void OnResize(int screenW, int screenH) = 0;

    // Start the transition sequence sequence
    virtual void StartTransition(const EnemyEncounterData& encounter, OverworldEnemy* enemySource) = 0;

    // Advance the transition simulation. uiDt is real time (unaffected by slowmo).
    virtual void Update(float uiDt, Camera2D* camera) = 0;

    // Apply any screen-space capturing (e.g. pincushion off-screen RT bind)
    virtual void BeginCapture(ID3D11DeviceContext* ctx) = 0;
    
    // Draw the captured screen-space effect to the back buffer
    virtual void EndCaptureAndRender(ID3D11DeviceContext* ctx) = 0;

    // Status queries
    virtual bool IsActive() const = 0;
    virtual bool IsFinished() const = 0;

    // Fetch the pending data to push the BattleState
    virtual EnemyEncounterData GetPendingEncounter() const = 0;
    virtual OverworldEnemy* GetPendingEnemySource() const = 0;
    
    // Acknowledge the transition is complete and clear state.
    virtual void ClearPending() = 0;
};
