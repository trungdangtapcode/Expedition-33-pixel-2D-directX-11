// ============================================================
// File: WeakenSkill.cpp
// ============================================================
#include "WeakenSkill.h"
#include "IBattler.h"
#include "IAction.h"
#include "StatusEffectAction.h"
#include "WeakenEffect.h"
#include "LogAction.h"
#include <memory>

bool WeakenSkill::CanUse(const IBattler& /*caster*/) const
{
    return true;    // no MP cost in MVP
}

std::vector<std::unique_ptr<IAction>> WeakenSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets) const
{
    std::vector<std::unique_ptr<IAction>> actions;

    if (targets.empty()) return actions;

    IBattler* target = targets[0];

    actions.push_back(std::make_unique<LogAction>(
        nullptr,
        caster.GetName() + " weakens " + target->GetName() + "! (-15 ATK, -10 DEF for 2 turns)"
    ));

    // Tuning values: 2 turns, 15 ATK reduction, 10 DEF reduction.
    // In the full game these would be driven from data/skills/*.json.
    actions.push_back(std::make_unique<StatusEffectAction>(
        target,
        std::make_unique<WeakenEffect>(2, 15, 10)
    ));

    return actions;
}
