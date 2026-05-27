// ============================================================
// File: RageSpendAction.cpp
// Responsibility: Implement explicit rage spending actions.
// ============================================================
#define NOMINMAX
#include "RageSpendAction.h"
#include "IBattler.h"
#include "../Utils/Log.h"
#include <algorithm>

RageSpendAction::RageSpendAction(IBattler* target, int amount, bool spendAll)
    : mTarget(target)
    , mAmount(amount)
    , mSpendAll(spendAll)
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Spend a fixed amount of rage or empty the whole rage bar.
// Why:
//   Rage spend should happen at the committed action moment, not when
//   the player merely highlights or previews a skill.
// Parameters:
//   dt - Unused because this action completes immediately.
// ------------------------------------------------------------
bool RageSpendAction::Execute(float /*dt*/)
{
    if (!mTarget) return true;

    BattlerStats& stats = mTarget->GetStats();
    const int before = stats.rage;
    if (mSpendAll)
    {
        stats.rage = 0;
    }
    else
    {
        stats.rage = std::max(0, stats.rage - std::max(0, mAmount));
    }

    LOG("[RageSpendAction] %s spends %d rage (%d -> %d).",
        mTarget->GetDebugName().c_str(),
        before - stats.rage,
        before,
        stats.rage);
    return true;
}
