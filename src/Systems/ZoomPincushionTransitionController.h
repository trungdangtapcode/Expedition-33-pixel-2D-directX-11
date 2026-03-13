// ============================================================
// File: ZoomPincushionTransitionController.h
// Responsibility: Concrete battle transition that ramps up a 
//   pincushion distortion while dynamically zooming and rotating 
//   the camera (15 degrees) to create a dramatic entrance effect.
// ============================================================
#pragma once
#include "IBattleTransitionController.h"
#include "../Renderer/PincushionDistortionFilter.h"
#include <memory>

class ZoomPincushionTransitionController : public IBattleTransitionController {
public:
    virtual ~ZoomPincushionTransitionController() = default;

    bool Initialize(ID3D11Device* device, int screenW, int screenH) override;
    void Shutdown() override;
    
    void OnResize(int screenW, int screenH) override;

    void StartTransition(const EnemyEncounterData& encounter, OverworldEnemy* enemySource) override;
    void Update(float uiDt, Camera2D* camera) override;

    void BeginCapture(ID3D11DeviceContext* ctx) override;
    void EndCaptureAndRender(ID3D11DeviceContext* ctx) override;

    bool IsActive() const override;
    bool IsFinished() const override;

    EnemyEncounterData GetPendingEncounter() const override;
    OverworldEnemy* GetPendingEnemySource() const override;
    
    void ClearPending() override;

private:
    std::unique_ptr<PincushionDistortionFilter> mPincushionFilter;
    ID3D11Device* mDevice = nullptr;
    
    bool mIsActive = false;
    bool mIsFinished = false;
    float mTimer = 0.0f;
    
    // Ramp up duration
    static constexpr float kDuration = 1.0f;
    
    // Target camera effect stats
    static constexpr float kTargetZoom = 2.5f;
    static constexpr float kTargetRotation = 7.0f * (3.14159265f / 180.0f); // 12 degrees

    EnemyEncounterData mPendingEncounter;
    OverworldEnemy* mPendingEnemySource = nullptr;
};
