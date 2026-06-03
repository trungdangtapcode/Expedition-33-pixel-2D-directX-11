// ============================================================
// File: StatusEffectAction.cpp
// Responsibility: Implement optional-chance status application.
// ============================================================
#define NOMINMAX
#include "StatusEffectAction.h"
#include <algorithm>

namespace
{
    float NextUnitRoll()
    {
        static unsigned int sState = 0xA53C9E21u;
        sState = sState * 1664525u + 1013904223u;
        return static_cast<float>((sState >> 8) & 0xFFFFFFu) /
            static_cast<float>(0x1000000u);
    }
}

StatusEffectAction::StatusEffectAction(IBattler* target,
                                       std::unique_ptr<IStatusEffect> effect,
                                       float chance)
    : mTarget(target)
    , mEffect(std::move(effect))
    , mChance(std::max(0.0f, std::min(chance, 1.0f)))
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Transfer the owned effect to the target when the chance roll succeeds.
// Why:
//   Skill JSON can express partial status chances without deciding the
//   random result during menu input or skill construction.
// Parameters:
//   dt - Unused because this action completes immediately.
// ------------------------------------------------------------
bool StatusEffectAction::Execute(float /*dt*/)
{
    if (!mTarget || !mEffect) return true;
    if (mChance <= 0.0f) return true;
    if (mChance < 1.0f && NextUnitRoll() > mChance) return true;

    mTarget->AddEffect(std::move(mEffect));
    return true;
}
