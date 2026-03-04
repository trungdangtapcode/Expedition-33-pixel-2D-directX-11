// ============================================================
// File: WeakenSkill.h
// Responsibility: Apply a 2-turn ATK/DEF debuff to one enemy target.
//
// Effect: WeakenEffect(duration=2, atkReduction=15, defReduction=10)
// ============================================================
#pragma once
#include "ISkill.h"

class WeakenSkill : public ISkill
{
public:
    const char* GetName()        const override { return "Weaken"; }
    const char* GetDescription() const override { return "Reduce target ATK and DEF for 2 turns."; }

    // Always available — no MP cost in MVP.
    bool CanUse(const IBattler& caster) const override;

    // Produces: LogAction + StatusEffectAction(WeakenEffect)
    std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets) const override;
};
