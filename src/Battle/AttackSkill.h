// ============================================================
// File: AttackSkill.h
// Responsibility: Basic attack — deals (atk - def) damage; grants rage to both sides.
//
// Rage formula (handled inside Combatant::TakeDamage):
//   Attacker rage += effective / 4
//   Defender rage += effective / 8
// ============================================================
#pragma once
#include "ISkill.h"

class AttackSkill : public ISkill
{
public:
    const char* GetName()        const override { return "Attack"; }
    const char* GetDescription() const override { return "Strike the enemy."; }

    // Always available — basic attack has no resource cost.
    bool CanUse(const IBattler& caster) const override;

    // Produces: LogAction + DamageAction
    std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets) const override;
};
