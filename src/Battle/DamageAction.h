// ============================================================
// File: DamageAction.h
// Responsibility: Atomic action — deal damage from attacker to defender.
//
// Completes instantly (returns true on first Execute call).
// Rage is distributed inside Combatant::TakeDamage, not here.
// ============================================================
#pragma once
#include "IAction.h"
#include "IBattler.h"
#include "IDamageCalculator.h"

class DamageAction : public IAction
{
public:
    // Takes a full request to be evaluated at the exact execution frame.
    DamageAction(const DamageRequest& request);

    // Runs DamageCalculator::Calculate on mRequest, then calls TakeDamage. Completes in one frame.
    bool Execute(float dt) override;

private:
    DamageRequest mRequest;
};
