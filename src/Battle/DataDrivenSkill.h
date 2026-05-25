// ============================================================
// File: DataDrivenSkill.h
// Responsibility: Generic player skill loaded from data/skills/*.json.
// ============================================================
#pragma once
#include "ISkill.h"
#include "../Utils/JsonLoader.h"

class DataDrivenSkill : public ISkill
{
public:
    explicit DataDrivenSkill(JsonLoader::SkillData data);

    std::string GetName() const override;
    std::string GetDescription() const override;
    std::string GetId() const override;
    std::string GetIconId() const override;
    int GetMpCost() const override;
    SkillResourceKind GetResourceKind() const override;
    SkillTargeting GetTargeting() const override;
    std::string GetDebugName() const override;
    std::string GetDebugDescription() const override;
    bool CanUse(const IBattler& caster, const BattleContext& ctx) const override;
    std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets,
        const BattleContext& ctx) const override;

private:
    JsonLoader::SkillData mData;
    SkillTargeting mTargeting = SkillTargeting::SingleEnemy;
    SkillResourceKind mResourceKind = SkillResourceKind::None;
};
