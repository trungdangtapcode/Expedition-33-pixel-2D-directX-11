// ============================================================
// File: RageSkill.h
// Responsibility: Legacy rage burst skill interface.
//
// Important:
//   New data-driven rage finishers should use AttackSkill through
//   SkillFactory when their JSON sets "mechanism": "attack".
// ============================================================
#pragma once

#include "ISkill.h"

class RageSkill : public ISkill
{
public:
    std::string GetName() const override;
    std::string GetDescription() const override;
    std::string GetDebugName() const override;
    std::string GetDebugDescription() const override;

    bool CanUse(const IBattler& caster, const BattleContext& ctx) const override;

    std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets,
        const BattleContext& ctx) const override;
};
