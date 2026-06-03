// ============================================================
// File: RestoreMpAction.cpp
// Responsibility: Implement MP restoration as an action-queue mutation.
// ============================================================
#include "RestoreMpAction.h"
#include "IBattler.h"
#include "../Utils/Log.h"

RestoreMpAction::RestoreMpAction(IBattler* target, int amount)
    : mTarget(target)
    , mAmount(amount)
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Restore MP on one target and clamp the result.
// Why:
//   Future support skills can replenish resources without duplicating
//   item effect code.
// Parameters:
//   dt - Unused because this action completes immediately.
// ------------------------------------------------------------
bool RestoreMpAction::Execute(float /*dt*/)
{
    if (!mTarget || mAmount <= 0) return true;

    BattlerStats& stats = mTarget->GetStats();
    const int before = stats.mp;
    stats.RestoreMp(mAmount);

    LOG("[RestoreMpAction] %s restores %d MP (%d -> %d).",
        mTarget->GetDebugName().c_str(),
        stats.mp - before,
        before,
        stats.mp);
    return true;
}
