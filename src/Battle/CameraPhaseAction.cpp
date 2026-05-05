// ============================================================
// File: CameraPhaseAction.cpp
// ============================================================
#include "CameraPhaseAction.h"
#include "BattleEvents.h"
#include "../Events/EventManager.h"

CameraPhaseAction::CameraPhaseAction(BattleCameraPhase phase, IBattler* targetToFollow, float dynamicZoom)
    : mPhase(phase), mTarget(targetToFollow), mZoom(dynamicZoom)
{
}

bool CameraPhaseAction::Execute(float /*dt*/)
{
    // Throw payload onto the bus so BattleState can properly dispatch it cleanly
    CameraPhasePayload payload;
    payload.phase = mPhase;
    payload.targetToFollow = mTarget;
    payload.dynamicZoom = mZoom;

    EventData e;
    e.payload = &payload;
    EventManager::Get().Broadcast("battler_set_camera_phase", e);

    return true; // We resolve instantly, this action never blocks the queue physically.
}
