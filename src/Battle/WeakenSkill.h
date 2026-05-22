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
    std::string GetName()        const override;
    std::string GetDescription() const override;

    // Always available — no MP cost in MVP.
    bool CanUse(const IBattler& caster, const BattleContext& ctx) const override;

    // Produces: LogAction + StatusEffectAction(WeakenEffect)
    std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets,
        const BattleContext& ctx) const override;
};
