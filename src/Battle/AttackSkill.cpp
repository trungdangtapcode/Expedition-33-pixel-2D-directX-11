// ============================================================
// File: AttackSkill.cpp
// ============================================================
#include "AttackSkill.h"
#include "IBattler.h"
#include "IAction.h"
#include "DamageAction.h"
#include "DataDrivenStatusEffect.h"
#include "LogAction.h"
#include "MoveAction.h"
#include "PlayAnimationAction.h"
#include "AnimDamageAction.h"
#include "QteAnimDamageAction.h"
#include "CameraPhaseAction.h"
#include "BulletHellAction.h"
#include "BattleContext.h"
#include "ConsumeMpAction.h"
#include "StatusEffectAction.h"
#include "StatusEffectRegistry.h"
#include "../Systems/LocalizationManager.h"
#include <random>

namespace
{
    SkillTargeting ParseTargeting(const std::string& value)
    {
        if (value == "single_ally") return SkillTargeting::SingleAlly;
        if (value == "single_ally_any") return SkillTargeting::SingleAllyAny;
        if (value == "all_enemies") return SkillTargeting::AllEnemies;
        if (value == "all_allies") return SkillTargeting::AllAllies;
        if (value == "self") return SkillTargeting::Self;
        return SkillTargeting::SingleEnemy;
    }

    DamageType ParseDamageType(const std::string& value)
    {
        if (value == "magical") return DamageType::Magical;
        if (value == "true") return DamageType::TrueDamage;
        return DamageType::Physical;
    }

    std::string EffectiveEffect(const JsonLoader::SkillData& data)
    {
        if (!data.effect.empty()) return data.effect;
        if (data.kind == "attack" || data.kind == "damage" || data.kind == "rage") return "damage";
        return data.kind;
    }

    class ConsumeRageAction final : public IAction
    {
    public:
        explicit ConsumeRageAction(IBattler* caster) : mCaster(caster) {}

        bool Execute(float) override
        {
            if (mCaster) mCaster->GetStats().rage = 0;
            return true;
        }

    private:
        IBattler* mCaster = nullptr;
    };
}

std::string AttackSkill::GetName() const
{
    return LocalizationManager::Get().TextOrFallback(mData.nameKey, "Attack");
}

std::string AttackSkill::GetDescription() const
{
    return LocalizationManager::Get().TextOrFallback(
        mData.descriptionKey,
        "Strike the enemy.");
}

std::string AttackSkill::GetId() const
{
    return mData.id.empty() ? "attack" : mData.id;
}

std::string AttackSkill::GetIconId() const
{
    return mData.iconId;
}

int AttackSkill::GetMpCost() const
{
    return mData.mpCost;
}

SkillResourceKind AttackSkill::GetResourceKind() const
{
    if (mData.mpCost > 0) return SkillResourceKind::MP;
    if (mData.requiresFullRage || mData.consumesAllRage) return SkillResourceKind::Rage;
    return SkillResourceKind::None;
}

SkillTargeting AttackSkill::GetTargeting() const
{
    return ParseTargeting(mData.targeting);
}

std::string AttackSkill::GetKind() const
{
    return mData.kind.empty() ? "attack" : mData.kind;
}

std::string AttackSkill::GetEffect() const
{
    return EffectiveEffect(mData);
}

std::string AttackSkill::GetDamageType() const
{
    return mData.damageType.empty() ? "physical" : mData.damageType;
}

std::string AttackSkill::GetStatusEffectId() const
{
    return mData.statusEffectId;
}

std::string AttackSkill::GetDamageGradeKey() const
{
    return mData.damageGradeKey;
}

std::vector<std::string> AttackSkill::GetExtraRuleKeys() const
{
    return mData.extraRuleKeys;
}

int AttackSkill::GetHitCount() const
{
    return mData.hitCount;
}

float AttackSkill::GetSkillMultiplier() const
{
    return mData.skillMultiplier;
}

std::string AttackSkill::GetDebugName() const
{
    return LocalizationManager::Get().TextOrFallbackEnglish(mData.nameKey, "Attack");
}

std::string AttackSkill::GetDebugDescription() const
{
    return LocalizationManager::Get().TextOrFallbackEnglish(
        mData.descriptionKey,
        "Strike the enemy.");
}

std::string AttackSkill::SelectBulletHellPatternPath() const
{
    const std::vector<std::string>& patterns = mData.bulletHellPatternPaths;
    if (patterns.empty()) return mData.bulletHellPatternPath;

    if (patterns.size() == 1) {
        mLastBulletHellPatternIndex = 0;
        return patterns[0];
    }

    int selectedIndex = 0;
    const int patternCount = static_cast<int>(patterns.size());
    if (mData.bulletHellPatternSelection == "cycle") {
        selectedIndex = mNextBulletHellPatternIndex % patternCount;
        mNextBulletHellPatternIndex = (selectedIndex + 1) % patternCount;
    } else if (mData.bulletHellPatternSelection == "random_no_repeat") {
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> distribution(0, patternCount - 1);
        selectedIndex = distribution(rng);
        if (selectedIndex == mLastBulletHellPatternIndex) {
            selectedIndex = (selectedIndex + 1) % patternCount;
        }
    } else if (mData.bulletHellPatternSelection == "random") {
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> distribution(0, patternCount - 1);
        selectedIndex = distribution(rng);
    }

    mLastBulletHellPatternIndex = selectedIndex;
    return patterns[selectedIndex];
}

bool AttackSkill::CanUse(const IBattler& caster, const BattleContext& /*ctx*/) const
{
    if (mData.mpCost > 0 && caster.GetStats().mp < mData.mpCost) return false;
    if (mData.requiresFullRage && !caster.GetStats().IsRageFull()) return false;
    return true;
}

std::vector<std::unique_ptr<IAction>> AttackSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets,
    const BattleContext& ctx) const
{
    std::vector<std::unique_ptr<IAction>> actions;
    if (targets.empty() || !CanUse(caster, ctx)) return actions;

    std::vector<IBattler*> validTargets;
    for (IBattler* target : targets)
    {
        if (target && target->IsAlive()) validTargets.push_back(target);
    }
    if (validTargets.empty()) return actions;

    // Log message first so it appears before the damage number.
    actions.push_back(std::make_unique<LogAction>(
        nullptr,    // BattleManager injects the log pointer when enqueuing
        LocalizationManager::Get().Format("battle.log.skill_use", {
            { "actor", caster.GetName() },
            { "skill", GetName() },
            { "target", validTargets[0]->GetName() }
        }),
        nullptr,
        LocalizationManager::Get().FormatEnglish("battle.log.skill_use", {
            { "actor", caster.GetDebugName() },
            { "skill", GetDebugName() },
            { "target", validTargets[0]->GetDebugName() }
        })
    ));

    actions.push_back(std::make_unique<ConsumeMpAction>(&caster, mData.mpCost));
    if (mData.consumesAllRage)
    {
        actions.push_back(std::make_unique<ConsumeRageAction>(&caster));
    }

    // 1. Enter fight state
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::FightState, false));

    // 2. Camera hooks to player before dash
    actions.push_back(std::make_unique<CameraPhaseAction>(BattleCameraPhase::DYNAMIC_FOLLOW, &caster, ctx.config.qteCameraZoom));

    const bool useMeleeMotion = mData.attackMotion != "stationary";
    const bool resolveTargetsTogether = validTargets.size() > 1 && !mData.bulletHellSupported;

    auto buildDamageRequest = [&](IBattler* target)
    {
        DamageRequest req;
        req.attacker = &caster;
        req.defender = target;
        req.type = ParseDamageType(mData.damageType);
        req.skillMultiplier = mData.skillMultiplier;
        req.flatBonus = mData.flatBonus;
        return req;
    };

    auto enqueueTimedDamage = [&](const std::vector<IBattler*>& damageTargets)
    {
        std::vector<DamageRequest> requests;
        requests.reserve(damageTargets.size());
        for (IBattler* target : damageTargets)
        {
            if (target && target->IsAlive()) requests.push_back(buildDamageRequest(target));
        }
        if (requests.empty()) return;

        if (mData.bulletHellSupported && requests.size() == 1) {
            const std::string patternPath = SelectBulletHellPatternPath();
            if (!patternPath.empty()) {
                actions.push_back(std::make_unique<BulletHellAction>(&caster, requests.front().defender, patternPath, &ctx));
            } else {
                actions.push_back(std::make_unique<AnimDamageAction>(
                    std::move(requests), CombatantAnim::Attack, mData.damageTakenOccurMoment, &ctx
                ));
            }
        } else if (mData.qteSupported) {
            actions.push_back(std::make_unique<QteAnimDamageAction>(
                std::move(requests), CombatantAnim::Attack, mData.qteStartMoment, mData.damageTakenOccurMoment, ctx.config.qteSlowMoScale,
                mData.qtePerfectMultiplier, mData.qteGoodMultiplier, mData.qteMissMultiplier,
                mData.qtePerfectThreshold, mData.qteGoodThreshold,
                mData.qteMinCount, mData.qteMaxCount, mData.bonusQteCount, mData.qteSpacing,
                ctx.config.qteFadeInRatio, ctx.config.qteFadeOutDuration,
                &ctx
            ));
        } else {
            actions.push_back(std::make_unique<AnimDamageAction>(
                std::move(requests), CombatantAnim::Attack, mData.damageTakenOccurMoment, &ctx
            ));
        }
    };

    auto enqueueStatusApplication = [&](const std::vector<IBattler*>& statusTargets)
    {
        if (!mData.statusEffectId.empty())
        {
            StatusEffectRegistry::Get().EnsureLoaded();
            if (const StatusEffectData* status = StatusEffectRegistry::Get().Find(mData.statusEffectId))
            {
                for (IBattler* target : statusTargets)
                {
                    if (!target || !target->IsAlive()) continue;
                    actions.push_back(std::make_unique<StatusEffectAction>(
                        target,
                        std::make_unique<DataDrivenStatusEffect>(*status),
                        mData.statusChance));
                }
            }
        }
    };

    if (resolveTargetsTogether)
    {
        if (useMeleeMotion)
        {
            actions.push_back(std::make_unique<MoveAction>(
                &caster,
                validTargets[0],
                MoveAction::TargetType::MeleeRange,
                mData.moveDuration,
                mData.meleeOffset));
        }
        enqueueTimedDamage(validTargets);
        enqueueStatusApplication(validTargets);
    }
    else
    {
        for (IBattler* target : validTargets)
        {
            if (useMeleeMotion)
            {
                // Multi-hit melee skills visit one target at a time so the
                // attack arc remains readable even with several enemies.
                actions.push_back(std::make_unique<MoveAction>(
                    &caster,
                    target,
                    MoveAction::TargetType::MeleeRange,
                    mData.moveDuration,
                    mData.meleeOffset));
            }

            enqueueTimedDamage(std::vector<IBattler*>{ target });
            enqueueStatusApplication(std::vector<IBattler*>{ target });
        }
    }

    // 6. Move back to origin (automatically manages BattleMove and BattleUnmove inside MoveAction)
    if (useMeleeMotion)
    {
        actions.push_back(std::make_unique<MoveAction>(&caster, nullptr, MoveAction::TargetType::Origin, mData.returnDuration, mData.meleeOffset));
    }

    // 7. Safely unhook camera back to wide overhead!
    actions.push_back(std::make_unique<CameraPhaseAction>(BattleCameraPhase::OVERVIEW));

    // 8. Return to idle state
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::Idle, true));

    return actions;
}
