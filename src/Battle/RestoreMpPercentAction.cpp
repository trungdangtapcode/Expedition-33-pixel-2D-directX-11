// ============================================================
// File: RestoreMpPercentAction.cpp
// Responsibility: Implement percentage-based MP restoration.
// ============================================================
#include "RestoreMpPercentAction.h"
#include "IBattler.h"
#include "../Utils/Log.h"
#include <cmath>

RestoreMpPercentAction::RestoreMpPercentAction(IBattler* target, float percent)
    : mTarget(target)
    , mPercent(percent)
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Restore MP based on the target's current maximum MP.
// Why:
//   Basic attacks should remain useful as a resource builder without
//   hardcoding a fixed number that becomes wrong after progression.
// Parameters:
//   dt - Unused because this action completes immediately.
// ------------------------------------------------------------
bool RestoreMpPercentAction::Execute(float /*dt*/)
{
    if (!mTarget || mPercent <= 0.0f) return true;

    BattlerStats& stats = mTarget->GetStats();
    if (stats.maxMp <= 0) return true;

    int amount = static_cast<int>(std::ceil(static_cast<float>(stats.maxMp) * mPercent));
    if (amount < 1) amount = 1;

    const int before = stats.mp;
    stats.RestoreMp(amount);

    LOG("[RestoreMpPercentAction] %s restores %d MP from %.2f%% max MP (%d -> %d).",
        mTarget->GetDebugName().c_str(),
        stats.mp - before,
        mPercent * 100.0f,
        before,
        stats.mp);
    return true;
}
