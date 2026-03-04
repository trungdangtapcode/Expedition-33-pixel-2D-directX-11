// ============================================================
// File: StatusEffectAction.h
// Responsibility: Atomic action — attach one IStatusEffect to a target.
//
// Takes ownership of the effect via unique_ptr and transfers it to the
// target's combatant on Execute().  Completes instantly.
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
    StatusEffectAction(IBattler* target, std::unique_ptr<IStatusEffect> effect);

    // Calls target->AddEffect(mEffect). Completes in one frame.
    bool Execute(float dt) override;

private:
    IBattler*                    mTarget;   // non-owning
    std::unique_ptr<IStatusEffect> mEffect; // owned until transferred
};
