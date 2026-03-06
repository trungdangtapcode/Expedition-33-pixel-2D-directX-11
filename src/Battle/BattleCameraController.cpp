// ============================================================
// File: BattleCameraController.cpp
// Responsibility: Smooth camera transition logic for the three battle
//   camera phases (OVERVIEW / ACTOR_FOCUS / TARGET_FOCUS).
//
// Lerp formula (exponential decay — frame-rate independent):
//   current += (desired - current) * kSmoothSpeed * dt
//   At kSmoothSpeed=5.0 and 60 fps: ~95% closure in ~0.6 s.
//   At 240 fps: same visual speed — the gap shrinks identically.
//
// Why exponential decay and not linear lerp?
//   Linear lerp (t += speed * dt) overshoots when dt spikes.
//   Exponential decay is self-correcting: the step is always proportional
//   to the remaining gap, so it asymptotically approaches the target.
//   This is the standard camera easing technique in 2-D JRPGs.
//
// Ownership: BattleCameraController is a value member of BattleRenderer.
// ============================================================
#include "BattleCameraController.h"
#include "../Utils/Log.h"

// ------------------------------------------------------------
// Initialize: construct the Camera2D and set the OVERVIEW defaults.
// ------------------------------------------------------------
void BattleCameraController::Initialize(int screenW, int screenH)
{
    // Camera2D(W, H) builds its initial matrices in the constructor,
    // so GetCamera() is safe immediately after Initialize() returns.
    mCamera = std::make_unique<Camera2D>(screenW, screenH);
    mCamera->SetPosition(0.0f, 0.0f);
    mCamera->SetZoom(kZoomOverview);
    mCamera->Update();

    // Seed the interpolated state to match the camera's starting value,
    // so the first-frame lerp has zero distance to travel.
    mCurrentX    = 0.0f;
    mCurrentY    = 0.0f;
    mCurrentZoom = kZoomOverview;
    mPhase       = BattleCameraPhase::OVERVIEW;

    LOG("[BattleCamera] Initialized — screen %dx%d, phase=OVERVIEW", screenW, screenH);
}

// ------------------------------------------------------------
// SetActorPos: store the world-space center of the acting combatant.
//   Call this whenever the active combatant changes (turn changes).
// ------------------------------------------------------------
void BattleCameraController::SetActorPos(float worldX, float worldY)
{
    mActorX = worldX;
    mActorY = worldY;
}

// ------------------------------------------------------------
// SetTargetPos: store the world-space center of the selected target.
//   Call this whenever the player changes the target cursor.
// ------------------------------------------------------------
void BattleCameraController::SetTargetPos(float worldX, float worldY)
{
    mTargetX = worldX;
    mTargetY = worldY;
}

// ------------------------------------------------------------
// SetPhase: transition to a new camera state.
//   The lerp in Update() will smoothly move from the current
//   interpolated position to the new desired position each frame.
// ------------------------------------------------------------
void BattleCameraController::SetPhase(BattleCameraPhase phase)
{
    mPhase = phase;
}

// ------------------------------------------------------------
// ComputeDesired: map the current phase to (desiredX, desiredY, desiredZoom).
//
//   OVERVIEW:
//     pos = (0, 0)  — camera centered, all combatants visible.
//     zoom = 1.0
//
//   ACTOR_FOCUS:
//     pos = actorWorldPos — camera centers on the acting character.
//     zoom = 1.6          — mild close-up without losing context.
//
//   TARGET_FOCUS:
//     pos = lerp(actorPos, targetPos, kTargetBlend)
//           = targetPos * 0.8 + actorPos * 0.2
//     zoom = 1.0  — wide again so the full enemy row is readable.
//     The 80/20 blend keeps the actor barely in frame as a reminder of
//     who is attacking, while the target is the clear focal point.
// ------------------------------------------------------------
void BattleCameraController::ComputeDesired(float& outX, float& outY,
                                             float& outZoom) const
{
    switch (mPhase)
    {
    case BattleCameraPhase::ACTOR_FOCUS:
        outX    = mActorX;
        outY    = mActorY;
        outZoom = kZoomFocus;
        break;

    case BattleCameraPhase::TARGET_FOCUS:
        // Weighted blend: 80% target + 20% actor.
        // This puts the target clearly in frame while hinting at the attacker.
        outX    = mTargetX * kTargetBlend + mActorX * (1.0f - kTargetBlend);
        outY    = mTargetY * kTargetBlend + mActorY * (1.0f - kTargetBlend);
        outZoom = kZoomTarget;
        break;

    case BattleCameraPhase::OVERVIEW:
    default:
        outX    = 0.0f;
        outY    = 0.0f;
        outZoom = kZoomOverview;
        break;
    }
}

// ------------------------------------------------------------
// Update: advance the smooth interpolation toward the desired state.
//
// Per-frame steps:
//   1. Compute the desired (x, y, zoom) for the current phase.
//   2. Exponential-decay-lerp mCurrent* toward desired.
//   3. Push the interpolated values into Camera2D and rebuild the matrix.
//
// Why lerp position AND zoom separately?
//   They have different physical semantics — blending them with separate
//   coefficients lets us tune the "zoom pops faster than the pan" feel
//   typical of JRPG cinematics.  (Both use kSmoothSpeed here for simplicity;
//   split into kSmoothPos / kSmoothZoom if tuning is needed.)
// ------------------------------------------------------------
void BattleCameraController::Update(float dt)
{
    float desiredX, desiredY, desiredZoom;
    ComputeDesired(desiredX, desiredY, desiredZoom);

    const float k = kSmoothSpeed * dt;

    // Exponential decay toward desired position.
    mCurrentX    += (desiredX    - mCurrentX)    * k;
    mCurrentY    += (desiredY    - mCurrentY)    * k;
    mCurrentZoom += (desiredZoom - mCurrentZoom) * k;

    // Push the interpolated values into Camera2D.
    mCamera->SetPosition(mCurrentX, mCurrentY);
    mCamera->SetZoom(mCurrentZoom);
    mCamera->Update();  // rebuild view matrix with the new pos/zoom
}
