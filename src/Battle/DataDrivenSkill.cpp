// ============================================================
// File: DataDrivenSkill.cpp
// Responsibility: Build action sequences for data-driven skills.
// ============================================================
#define NOMINMAX
#include "DataDrivenSkill.h"
#include "BattleContext.h"
#include "ConsumeMpAction.h"
#include "CombatantAnim.h"
#include "DamageAction.h"
#include "DataDrivenStatusEffect.h"
#include "IBattler.h"
#include "IAction.h"
#include "LogAction.h"
#include "PlayAnimationAction.h"
#include "StatusEffectAction.h"
#include "StatusEffectRegistry.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/Log.h"

namespace
{
    SkillTargeting ParseTargeting(const std::string& value)
    {
        if (value == "single_ally") return SkillTargeting::SingleAlly;
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

    const bool hasDamage =
        mData.kind == "damage" ||
        mData.kind == "rage";

    if (hasDamage)
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

    if (!mData.statusEffectId.empty())
    {
        StatusEffectRegistry::Get().EnsureLoaded();
        const StatusEffectData* status = StatusEffectRegistry::Get().Find(mData.statusEffectId);
        if (status)
        {
            for (IBattler* target : targets)
            {
                if (!target || !target->IsAlive()) continue;
                actions.push_back(std::make_unique<StatusEffectAction>(
                    target,
                    std::make_unique<DataDrivenStatusEffect>(*status)));
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
