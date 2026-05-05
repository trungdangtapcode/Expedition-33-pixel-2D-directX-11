// ============================================================
// File: CameraPhaseAction.h
// Responsibility: Injects a generic timeline action that abruptly shifts camera focus rules.
// ============================================================
#pragma once

#include "IAction.h"
#include "IBattler.h"
#include "BattleCameraController.h"

class CameraPhaseAction : public IAction
{
public:
    CameraPhaseAction(BattleCameraPhase phase, IBattler* targetToFollow = nullptr, float dynamicZoom = 1.4f);

    bool Execute(float dt) override;

private:
    BattleCameraPhase mPhase;
    IBattler* mTarget;
    float mZoom;
};
