// ============================================================
// File: ZoomPincushionTransitionController.cpp
// ============================================================
#include "ZoomPincushionTransitionController.h"
#include "../Utils/Log.h"
#include <algorithm>
#include <cmath>

bool ZoomPincushionTransitionController::Initialize(ID3D11Device* device, int screenW, int screenH)
{
    mDevice = device;
    mPincushionFilter = std::make_unique<PincushionDistortionFilter>();
    if (!mPincushionFilter->Initialize(device, screenW, screenH))
    {
        LOG("[ZoomPincushionTransition] WARNING â€” PincushionDistortionFilter init failed.");
        mPincushionFilter.reset(); 
    }
    
    mIsActive = false;
    mIsFinished = false;
    mTimer = 0.0f;
    return true;
}

void ZoomPincushionTransitionController::Shutdown()
{
    if (mPincushionFilter)
    {
        mPincushionFilter->Shutdown();
        mPincushionFilter.reset();
    }
    mIsActive = false;
    mIsFinished = false;
}

void ZoomPincushionTransitionController::OnResize(int screenW, int screenH)
{
    // If the window resizes, we might need to recreate the filter RT.
    // For now, we simply re-initialize the filter.
    if (mPincushionFilter && mDevice)
    {
        mPincushionFilter->Shutdown();
        mPincushionFilter->Initialize(mDevice, screenW, screenH);
    }
}

void ZoomPincushionTransitionController::StartTransition(const EnemyEncounterData& encounter, OverworldEnemy* enemySource)
{
    mPendingEncounter = encounter;
    mPendingEnemySource = enemySource;
    mIsActive = true;
    mIsFinished = false;
    mTimer = 0.0f;
    
    LOG("[ZoomPincushionTransition] Started transition vs '%s'.", encounter.name.c_str());
}

void ZoomPincushionTransitionController::Update(float uiDt, Camera2D* camera)
{
    if (!mIsActive || mIsFinished) return;

    mTimer += uiDt;
    float t = mTimer / kDuration;
    if (t > 1.0f) t = 1.0f;

    // Apply ease-in-out curve for smoother motion
    float ease = t * t * (3.0f - 2.0f * t);

    // 1. Update filter intensity
    float intensity = (mTimer < kDuration) ? (mTimer / kDuration) : 1.0f;
    if (mPincushionFilter) {
        mPincushionFilter->Update(uiDt, intensity * intensity);
    }

    // 2. Camera zoom & rotation
    if (camera)
    {
        // Linearly interpolate zoom and rotation from default (1.0 zoom, 0.0 rot) 
        // to our target values using the eased parameter.
        float currentZoom = 1.0f + (kTargetZoom - 1.0f) * ease;
        float currentRot = kTargetRotation * ease;
        
        camera->SetZoom(currentZoom);
        camera->SetRotation(currentRot);
        camera->Update();
    }

    if (mTimer >= kDuration)
    {
        mIsFinished = true;
        LOG("[ZoomPincushionTransition] Transition sequence completed.");
    }
}

void ZoomPincushionTransitionController::BeginCapture(ID3D11DeviceContext* ctx)
{
    if (mIsActive && mPincushionFilter && mPincushionFilter->IsActive())
    {
        mPincushionFilter->BeginCapture(ctx);
    }
}

void ZoomPincushionTransitionController::EndCaptureAndRender(ID3D11DeviceContext* ctx)
{
    if (mIsActive && mPincushionFilter && mPincushionFilter->IsActive())
    {
        mPincushionFilter->EndCapture(ctx);
        mPincushionFilter->Render(ctx);
    }
}

bool ZoomPincushionTransitionController::IsActive() const
{
    return mIsActive;
}

bool ZoomPincushionTransitionController::IsFinished() const
{
    return mIsFinished;
}

EnemyEncounterData ZoomPincushionTransitionController::GetPendingEncounter() const
{
    return mPendingEncounter;
}

OverworldEnemy* ZoomPincushionTransitionController::GetPendingEnemySource() const
{
    return mPendingEnemySource;
}

void ZoomPincushionTransitionController::ClearPending()
{
    mPendingEnemySource = nullptr;
    mIsActive = false;
    mIsFinished = false;
    
    // Reset filter
    if (mPincushionFilter) {
        mPincushionFilter->Update(0.0f, 0.0f);
    }
}
