// ============================================================
// File: AttackSkill.h
// Responsibility: Basic attack - deals (atk - def) damage; grants rage to both sides.
//
// Rage formula (handled inside Combatant::TakeDamage):
//   Attacker rage += effective / 4
//   Defender rage += effective / 8
// ============================================================
#pragma once
#include "ISkill.h"
#include "../Utils/JsonLoader.h"
#include <string>


class AttackSkill : public ISkill
{
private:
    JsonLoader::SkillData mData;
    mutable int mLastBulletHellPatternIndex = -1;
    mutable int mNextBulletHellPatternIndex = 0;

    std::string SelectBulletHellPatternPath() const;
public:
    AttackSkill(const JsonLoader::SkillData& data) : mData(data) {}
    std::string GetName()        const override;
    std::string GetDescription() const override;
    std::string GetDebugName() const override;
    std::string GetDebugDescription() const override;

    // Always available because the basic attack has no resource cost.
    bool CanUse(const IBattler& caster, const BattleContext& ctx) const override;

    // Produces: LogAction + DamageAction
    std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets,
        const BattleContext& ctx) const override;
};
