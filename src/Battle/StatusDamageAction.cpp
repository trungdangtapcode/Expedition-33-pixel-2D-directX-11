// ============================================================
// File: StatusDamageAction.cpp
// Responsibility: Apply status tick damage without bypassing IAction.
// ============================================================
#define NOMINMAX
#include "StatusDamageAction.h"
#include "IBattler.h"
#include "IDamageCalculator.h"
#include "../Utils/Log.h"
#include <algorithm>

StatusDamageAction::StatusDamageAction(IBattler* target, int damage, std::string effectId)
    : mTarget(target)
    , mDamage(damage)
    , mEffectId(std::move(effectId))
{
}

bool StatusDamageAction::Execute(float /*dt*/)
{
    if (!mTarget || !mTarget->IsAlive()) return true;

    DamageResult result;
    result.rawDamage = std::max(1, mDamage);
    result.defenseUsed = 0;
    result.effectiveDamage = result.rawDamage;
    result.isCritical = false;
    mTarget->TakeDamage(result, nullptr);

    LOG("[StatusDamageAction] %s takes %d damage from %s.",
        mTarget->GetDebugName().c_str(),
        result.effectiveDamage,
        mEffectId.c_str());
    return true;
}
