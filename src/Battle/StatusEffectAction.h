// ============================================================
// File: StatusEffectAction.h
// Responsibility: Atomic action that may attach one IStatusEffect to a target.
//
// Takes ownership of the effect via unique_ptr and transfers it to the
// target's combatant on Execute(). Completes instantly.
// ============================================================
#pragma once
#include "IAction.h"
#include "IBattler.h"
#include "IStatusEffect.h"
#include <memory>

class StatusEffectAction : public IAction
{
public:
    // Transfers ownership of effect to this action; later transferred to target.
    StatusEffectAction(IBattler* target,
                       std::unique_ptr<IStatusEffect> effect,
                       float chance = 1.0f);

    // Calls target->AddEffect(mEffect) when the chance roll succeeds.
    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    std::unique_ptr<IStatusEffect> mEffect;
    float mChance = 1.0f;
};
