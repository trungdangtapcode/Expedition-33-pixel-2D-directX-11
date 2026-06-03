// ============================================================
// File: RageSkill.cpp
// Responsibility: Legacy rage burst skill implementation.
// ============================================================
#include "RageSkill.h"
#include "BattleResourceRules.h"
#include "BattleContext.h"
#include "DamageAction.h"
#include "IBattler.h"
#include "IAction.h"
#include "LogAction.h"
#include "RageSpendAction.h"
#include "../Systems/LocalizationManager.h"

std::string RageSkill::GetName() const
{
    return LocalizationManager::Get().Text("skill.rage_burst.name");
}

std::string RageSkill::GetDescription() const
{
    return LocalizationManager::Get().Text("skill.rage_burst.description");
}

std::string RageSkill::GetDebugName() const
{
    return LocalizationManager::Get().TextEnglish("skill.rage_burst.name");
}

std::string RageSkill::GetDebugDescription() const
{
    return LocalizationManager::Get().TextEnglish("skill.rage_burst.description");
}

bool RageSkill::CanUse(const IBattler& caster, const BattleContext& /*ctx*/) const
{
    BattleResourceRules::Get().EnsureLoaded();
    return caster.GetStats().rage >= BattleResourceRules::Get().RageCostForSkill("rage_burst");
}

std::vector<std::unique_ptr<IAction>> RageSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets,
    const BattleContext& ctx) const
{
    std::vector<std::unique_ptr<IAction>> actions;
    if (targets.empty()) return actions;

    IBattler* target = targets[0];

    DamageRequest req;
    req.attacker = &caster;
    req.defender = target;
    req.type = DamageType::Physical;
    req.skillMultiplier = 2.0f;
    req.grantsRage = false;

    actions.push_back(std::make_unique<LogAction>(
        nullptr,
        LocalizationManager::Get().Format("battle.log.rage_burst", {
            { "actor", caster.GetName() },
            { "target", target->GetName() }
        }),
        nullptr,
        LocalizationManager::Get().FormatEnglish("battle.log.rage_burst", {
            { "actor", caster.GetDebugName() },
            { "target", target->GetDebugName() }
        })
    ));

    BattleResourceRules::Get().EnsureLoaded();
    actions.push_back(std::make_unique<RageSpendAction>(
        &caster,
        BattleResourceRules::Get().RageCostForSkill("rage_burst"),
        true));
    actions.push_back(std::make_unique<DamageAction>(req, &ctx));

    return actions;
}
