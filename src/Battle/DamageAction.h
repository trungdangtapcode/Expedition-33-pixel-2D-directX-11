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

class DamageAction : public IAction
{
public:
    // rawDamage — pre-DEF-reduction damage. DEF subtraction happens in TakeDamage.
    DamageAction(IBattler* attacker, IBattler* defender, int rawDamage);

    // Calls defender->TakeDamage(mRawDamage, mAttacker). Completes in one frame.
    bool Execute(float dt) override;

private:
    IBattler* mAttacker;   // non-owning — BattleManager owns the combatants
    IBattler* mDefender;
    int       mRawDamage;
};
