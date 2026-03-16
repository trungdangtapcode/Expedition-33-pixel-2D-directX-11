// ============================================================
// File: AttackSkill.cpp
// ============================================================
#include "AttackSkill.h"
#include "IBattler.h"
#include "IAction.h"
#include "DamageAction.h"
#include "LogAction.h"
#include "MoveAction.h"
#include "PlayAnimationAction.h"

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

    // 1. Enter fight state
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::FightState, false));

    // 2. Move to target's melee range (automatically manages BattleMove and BattleUnmove inside MoveAction)
    actions.push_back(std::make_unique<MoveAction>(&caster, target, MoveAction::TargetType::MeleeRange, mData.moveDuration));

    // 4. Play attack animation
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::Attack, true));

    // 5. Apply damage
    actions.push_back(std::make_unique<DamageAction>(&caster, target, rawDamage));

    // 6. Move back to origin (automatically manages BattleMove and BattleUnmove inside MoveAction)
    actions.push_back(std::make_unique<MoveAction>(&caster, nullptr, MoveAction::TargetType::Origin, mData.returnDuration));

    // 8. Return to idle state
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::Idle, true));

    return actions;
}
