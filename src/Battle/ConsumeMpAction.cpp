// ============================================================
// File: ConsumeMpAction.cpp
// Responsibility: Execute MP spending through the battle action queue.
// ============================================================
#include "ConsumeMpAction.h"
#include "IBattler.h"
#include "../Utils/Log.h"

ConsumeMpAction::ConsumeMpAction(IBattler* caster, int amount)
    : mCaster(caster)
    , mAmount(amount)
{
}

bool ConsumeMpAction::Execute(float /*dt*/)
{
    if (!mCaster || mAmount <= 0) return true;

    if (!mCaster->GetStats().SpendMp(mAmount))
    {
        LOG("[ConsumeMpAction] WARNING: %s could not spend %d MP.",
            mCaster->GetDebugName().c_str(),
            mAmount);
    }
    return true;
}
