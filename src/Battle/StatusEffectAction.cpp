// ============================================================
// File: StatusEffectAction.cpp
// ============================================================
#include "StatusEffectAction.h"

StatusEffectAction::StatusEffectAction(IBattler* target,
                                       std::unique_ptr<IStatusEffect> effect)
    : mTarget(target)
    , mEffect(std::move(effect))
{}

bool StatusEffectAction::Execute(float /*dt*/)
{
    // Transfer ownership of the effect to the target combatant.
    // After this call mEffect is nullptr — the target owns it.
    mTarget->AddEffect(std::move(mEffect));
    return true;    // instantaneous
}
