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
#include "AnimDamageAction.h"
#include "QteAnimDamageAction.h"
#include "CameraPhaseAction.h"
#include "BattleContext.h"

bool AttackSkill::CanUse(const IBattler& /*caster*/, const BattleContext& /*ctx*/) const
{
    return true;    // basic attack is always available
}

std::vector<std::unique_ptr<IAction>> AttackSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets,
    const BattleContext& ctx) const
{
    std::vector<std::unique_ptr<IAction>> actions;

    if (targets.empty()) return actions;

    IBattler* target = targets[0];  // single-target skill

    DamageRequest req;
    req.attacker = &caster;
    req.defender = target;
    req.type = DamageType::Physical;
    req.skillMultiplier = 1.0f;     // basic attack is 1.0x

    // Log message first so it appears before the damage number.
    actions.push_back(std::make_unique<LogAction>(
        nullptr,    // BattleManager injects the log pointer when enqueuing
        caster.GetName() + " attacks " + target->GetName() + "!"
    ));

    // 1. Enter fight state
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::FightState, false));

    // 2. Camera hooks to player before dash
    actions.push_back(std::make_unique<CameraPhaseAction>(BattleCameraPhase::DYNAMIC_FOLLOW, &caster, ctx.config.qteCameraZoom));

    // 3. Move to target's melee range (automatically manages BattleMove and BattleUnmove inside MoveAction)
    actions.push_back(std::make_unique<MoveAction>(&caster, target, MoveAction::TargetType::MeleeRange, mData.moveDuration, mData.meleeOffset));

    // 4. Play attack animation and apply damage simultaneously.
    if (mData.qteSupported) {
        actions.push_back(std::make_unique<QteAnimDamageAction>(
            req, CombatantAnim::Attack, mData.qteStartMoment, mData.damageTakenOccurMoment, ctx.config.qteSlowMoScale,
            mData.qtePerfectMultiplier, mData.qteGoodMultiplier, mData.qteMissMultiplier, 
            mData.qtePerfectThreshold, mData.qteGoodThreshold, 
            mData.qteMinCount, mData.qteMaxCount, mData.bonusQteCount, mData.qteSpacing,
            ctx.config.qteFadeInRatio, ctx.config.qteFadeOutDuration, 
            &ctx
        ));
    } else {
        actions.push_back(std::make_unique<AnimDamageAction>(
            req, CombatantAnim::Attack, mData.damageTakenOccurMoment, &ctx
        ));
    }

    // 6. Move back to origin (automatically manages BattleMove and BattleUnmove inside MoveAction)
    actions.push_back(std::make_unique<MoveAction>(&caster, nullptr, MoveAction::TargetType::Origin, mData.returnDuration, mData.meleeOffset));

    // 7. Safely unhook camera back to wide overhead!
    actions.push_back(std::make_unique<CameraPhaseAction>(BattleCameraPhase::OVERVIEW));

    // 8. Return to idle state
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::Idle, true));

    return actions;
}
