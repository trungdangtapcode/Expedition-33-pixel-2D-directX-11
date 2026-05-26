// ============================================================
// File: DataDrivenSkill.cpp
// Responsibility: Build action sequences for data-driven skills.
// ============================================================
#define NOMINMAX
#include "DataDrivenSkill.h"
#include "BattleContext.h"
#include "CleanseAction.h"
#include "ConsumeMpAction.h"
#include "CombatantAnim.h"
#include "DamageAction.h"
#include "DataDrivenStatusEffect.h"
#include "HealAction.h"
#include "IBattler.h"
#include "IAction.h"
#include "LogAction.h"
#include "PlayAnimationAction.h"
#include "RestoreMpAction.h"
#include "ReviveAction.h"
#include "StatusEffectAction.h"
#include "StatusEffectRegistry.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/Log.h"

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

    std::string EffectiveEffect(const JsonLoader::SkillData& data)
    {
        if (!data.effect.empty()) return data.effect;
        if (data.kind == "attack" || data.kind == "damage" || data.kind == "rage") return "damage";
        if (data.kind == "heal" || data.kind == "heal_hp") return "heal_hp";
        if (data.kind == "heal_mp") return "heal_mp";
        if (data.kind == "revive") return "revive";
        if (data.kind == "cleanse") return "cleanse";
        if (data.kind == "status" || data.kind == "support") return "status";
        return data.kind;
    }

    bool EffectDealsDamage(const std::string& effect)
    {
        return effect == "damage";
    }

    bool EffectUsesLivingTarget(const std::string& effect)
    {
        return effect != "revive";
    }
}

DataDrivenSkill::DataDrivenSkill(JsonLoader::SkillData data)
    : mData(std::move(data))
    , mTargeting(ParseTargeting(mData.targeting))
    , mResourceKind(mData.mpCost > 0 ? SkillResourceKind::MP :
        (mData.requiresFullRage || mData.consumesAllRage ? SkillResourceKind::Rage : SkillResourceKind::None))
{
}

std::string DataDrivenSkill::GetName() const
{
    return LocalizationManager::Get().TextOrFallback(mData.nameKey, mData.id);
}

std::string DataDrivenSkill::GetDescription() const
{
    return LocalizationManager::Get().TextOrFallback(mData.descriptionKey, mData.id);
}

std::string DataDrivenSkill::GetId() const
{
    return mData.id;
}

std::string DataDrivenSkill::GetIconId() const
{
    return mData.iconId;
}

int DataDrivenSkill::GetMpCost() const
{
    return mData.mpCost;
}

SkillResourceKind DataDrivenSkill::GetResourceKind() const
{
    return mResourceKind;
}

SkillTargeting DataDrivenSkill::GetTargeting() const
{
    return mTargeting;
}

std::string DataDrivenSkill::GetKind() const
{
    return mData.kind;
}

std::string DataDrivenSkill::GetEffect() const
{
    return EffectiveEffect(mData);
}

std::string DataDrivenSkill::GetDamageType() const
{
    return mData.damageType;
}

std::string DataDrivenSkill::GetStatusEffectId() const
{
    return mData.statusEffectId;
}

std::string DataDrivenSkill::GetDamageGradeKey() const
{
    return mData.damageGradeKey;
}

std::vector<std::string> DataDrivenSkill::GetExtraRuleKeys() const
{
    return mData.extraRuleKeys;
}

int DataDrivenSkill::GetHitCount() const
{
    return mData.hitCount;
}

float DataDrivenSkill::GetSkillMultiplier() const
{
    return mData.skillMultiplier;
}

std::string DataDrivenSkill::GetDebugName() const
{
    return LocalizationManager::Get().TextOrFallbackEnglish(mData.nameKey, mData.id);
}

std::string DataDrivenSkill::GetDebugDescription() const
{
    return LocalizationManager::Get().TextOrFallbackEnglish(mData.descriptionKey, mData.id);
}

bool DataDrivenSkill::CanUse(const IBattler& caster, const BattleContext& /*ctx*/) const
{
    if (mData.mpCost > 0 && caster.GetStats().mp < mData.mpCost) return false;
    if (mData.requiresFullRage && !caster.GetStats().IsRageFull()) return false;
    return true;
}

std::vector<std::unique_ptr<IAction>> DataDrivenSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets,
    const BattleContext& ctx) const
{
    std::vector<std::unique_ptr<IAction>> actions;
    if (!CanUse(caster, ctx)) return actions;

    actions.push_back(std::make_unique<ConsumeMpAction>(&caster, mData.mpCost));
    if (mData.consumesAllRage)
    {
        actions.push_back(std::make_unique<ConsumeRageAction>(&caster));
    }

    const std::string targetName = targets.empty() || !targets[0]
        ? caster.GetName()
        : targets[0]->GetName();
    const std::string debugTargetName = targets.empty() || !targets[0]
        ? caster.GetDebugName()
        : targets[0]->GetDebugName();

    actions.push_back(std::make_unique<LogAction>(
        nullptr,
        LocalizationManager::Get().Format("battle.log.skill_use", {
            { "actor", caster.GetName() },
            { "skill", GetName() },
            { "target", targetName }
        }),
        nullptr,
        LocalizationManager::Get().FormatEnglish("battle.log.skill_use", {
            { "actor", caster.GetDebugName() },
            { "skill", GetDebugName() },
            { "target", debugTargetName }
        })
    ));

    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::Attack, false));

    const std::string effect = EffectiveEffect(mData);
    if (EffectDealsDamage(effect))
    {
        for (IBattler* target : targets)
        {
            if (!target || !target->IsAlive()) continue;

            DamageRequest request;
            request.attacker = &caster;
            request.defender = target;
            request.type = ParseDamageType(mData.damageType);
            request.skillMultiplier = mData.skillMultiplier;
            actions.push_back(std::make_unique<DamageAction>(request, &ctx));
        }
    }

    if (effect == "heal_hp")
    {
        for (IBattler* target : targets)
        {
            if (!target || !target->IsAlive()) continue;
            actions.push_back(std::make_unique<HealAction>(target, mData.amount));
        }
    }
    else if (effect == "heal_mp")
    {
        for (IBattler* target : targets)
        {
            if (!target || !target->IsAlive()) continue;
            actions.push_back(std::make_unique<RestoreMpAction>(target, mData.amount));
        }
    }
    else if (effect == "revive")
    {
        for (IBattler* target : targets)
        {
            if (!target) continue;
            actions.push_back(std::make_unique<ReviveAction>(target, mData.amount));
        }
    }
    else if (effect == "cleanse")
    {
        for (IBattler* target : targets)
        {
            if (!target) continue;
            actions.push_back(std::make_unique<CleanseAction>(target));
        }
    }

    if (!mData.statusEffectId.empty())
    {
        StatusEffectRegistry::Get().EnsureLoaded();
        const StatusEffectData* status = StatusEffectRegistry::Get().Find(mData.statusEffectId);
        if (status)
        {
            for (IBattler* target : targets)
            {
                if (!target) continue;
                if (EffectUsesLivingTarget(effect) && !target->IsAlive()) continue;
                actions.push_back(std::make_unique<StatusEffectAction>(
                    target,
                    std::make_unique<DataDrivenStatusEffect>(*status),
                    mData.statusChance));
            }
        }
        else
        {
            LOG("[DataDrivenSkill] WARNING: missing status effect '%s'.", mData.statusEffectId.c_str());
        }
    }

    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::Idle, true));
    return actions;
}
