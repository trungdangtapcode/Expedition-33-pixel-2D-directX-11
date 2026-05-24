// ============================================================
// File: WeakenSkill.cpp
// ============================================================
#include "WeakenSkill.h"
#include "IBattler.h"
#include "IAction.h"
#include "StatusEffectAction.h"
#include "WeakenEffect.h"
#include "LogAction.h"
#include "BattleContext.h"
#include "../Systems/LocalizationManager.h"
#include <memory>

std::string WeakenSkill::GetName() const
{
    return LocalizationManager::Get().Text("skill.weaken.name");
}

std::string WeakenSkill::GetDescription() const
{
    return LocalizationManager::Get().Text("skill.weaken.description");
}

std::string WeakenSkill::GetDebugName() const
{
    return LocalizationManager::Get().TextEnglish("skill.weaken.name");
}

std::string WeakenSkill::GetDebugDescription() const
{
    return LocalizationManager::Get().TextEnglish("skill.weaken.description");
}

bool WeakenSkill::CanUse(const IBattler& /*caster*/, const BattleContext& /*ctx*/) const
{
    return true;    // no MP cost in MVP
}

std::vector<std::unique_ptr<IAction>> WeakenSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets,
    const BattleContext& /*ctx*/) const
{
    std::vector<std::unique_ptr<IAction>> actions;

    if (targets.empty()) return actions;

    IBattler* target = targets[0];

    actions.push_back(std::make_unique<LogAction>(
        nullptr,
        LocalizationManager::Get().Format("battle.log.weaken", {
            { "actor", caster.GetName() },
            { "target", target->GetName() }
        }),
        nullptr,
        LocalizationManager::Get().FormatEnglish("battle.log.weaken", {
            { "actor", caster.GetDebugName() },
            { "target", target->GetDebugName() }
        })
    ));

    // Tuning values: 2 turns, 15 ATK reduction, 10 DEF reduction.
    // In the full game these would be driven from data/skills/*.json.
    actions.push_back(std::make_unique<StatusEffectAction>(
        target,
        std::make_unique<WeakenEffect>(2, 15, 10)
    ));

    return actions;
}
