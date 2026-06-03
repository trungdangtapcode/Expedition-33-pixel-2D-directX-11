// ============================================================
// File: RageGainAction.cpp
// Responsibility: Implement explicit rage gain actions.
// ============================================================
#include "RageGainAction.h"
#include "BattleResourceRules.h"
#include <utility>

RageGainAction::RageGainAction(IBattler* target, int amount, std::string reason)
    : mTarget(target)
    , mAmount(amount)
    , mReason(std::move(reason))
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Add a known amount of rage to one combatant.
// Why:
//   Non-damage rage gains, such as basic attack momentum, should still
//   pass through the action queue instead of being applied from input.
// Parameters:
//   dt - Unused because this action completes immediately.
// ------------------------------------------------------------
bool RageGainAction::Execute(float /*dt*/)
{
    BattleResourceRules::Get().EnsureLoaded();
    BattleResourceRules::Get().GrantRage(mTarget, mAmount, mReason.c_str());
    return true;
}
