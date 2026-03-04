// ============================================================
// File: AttackSkill.cpp
// ============================================================
#include "AttackSkill.h"
#include "IBattler.h"
#include "IAction.h"
#include "DamageAction.h"
#include "LogAction.h"

bool AttackSkill::CanUse(const IBattler& /*caster*/) const
{
    return true;    // basic attack is always available
}

std::vector<std::unique_ptr<IAction>> AttackSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets) const
{
    std::vector<std::unique_ptr<IAction>> actions;

    if (targets.empty()) return actions;

    IBattler* target = targets[0];  // single-target skill

    // Raw damage = attacker ATK; DEF subtraction happens inside TakeDamage.
    const int rawDamage = caster.GetStats().atk;

    // Log message first so it appears before the damage number.
    actions.push_back(std::make_unique<LogAction>(
        nullptr,    // BattleManager injects the log pointer when enqueuing
        caster.GetName() + " attacks " + target->GetName() + "!"
    ));

    actions.push_back(std::make_unique<DamageAction>(&caster, target, rawDamage));

    return actions;
}
