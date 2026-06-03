// ============================================================
// File: HealAction.cpp
// Responsibility: Implement HP restoration as an action-queue mutation.
// ============================================================
#define NOMINMAX
#include "HealAction.h"
#include "IBattler.h"
#include "../Utils/Log.h"
#include <algorithm>

HealAction::HealAction(IBattler* target, int amount)
    : mTarget(target)
    , mAmount(amount)
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Restore HP on one living target, clamped by BattlerStats.
// Why:
//   Skills must not mutate combat state during input selection; the
//   action queue is the deterministic site for battle changes.
// Parameters:
//   dt - Unused because this action completes immediately.
// Caveats:
//   - KO targets are ignored; ReviveAction owns that recovery path.
// ------------------------------------------------------------
bool HealAction::Execute(float /*dt*/)
{
    if (!mTarget || mAmount <= 0) return true;
    if (!mTarget->IsAlive())
    {
        LOG("[HealAction] Ignored healing on defeated target '%s'.",
            mTarget->GetDebugName().c_str());
        return true;
    }

    BattlerStats& stats = mTarget->GetStats();
    const int before = stats.hp;
    stats.hp += mAmount;
    stats.ClampHp();

    LOG("[HealAction] %s restores %d HP (%d -> %d).",
        mTarget->GetDebugName().c_str(),
        stats.hp - before,
        before,
        stats.hp);
    return true;
}
