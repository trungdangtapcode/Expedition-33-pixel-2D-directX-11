// ============================================================
// File: DataDrivenStatusEffect.cpp
// Responsibility: Apply, merge, tick, and expire data-driven effects.
// ============================================================
#define NOMINMAX
#include "DataDrivenStatusEffect.h"
#include "IBattler.h"
#include "StatusDamageAction.h"
#include "../Systems/LocalizationManager.h"
#include <algorithm>

DataDrivenStatusEffect::DataDrivenStatusEffect(StatusEffectData data)
    : mData(std::move(data))
    , mRemainingTurns(std::max(1, mData.durationTurns))
    , mDisplayName(mData.id)
{
}

void DataDrivenStatusEffect::Apply(IBattler& target)
{
    if (mSourceId == 0)
    {
        mSourceId = StatModifierIds::Next();
    }
    RebuildStatModifiers(target);
}

void DataDrivenStatusEffect::OnTurnEnd(IBattler& /*target*/)
{
    if (mSkipFirstTurnEnd)
    {
        mSkipFirstTurnEnd = false;
        return;
    }

    if (mRemainingTurns > 0) --mRemainingTurns;
}

void DataDrivenStatusEffect::Revert(IBattler& target)
{
    if (mSourceId == 0) return;
    target.RemoveStatModifiersBySource(mSourceId);
    mSourceId = 0;
}

bool DataDrivenStatusEffect::IsExpired() const
{
    return mRemainingTurns <= 0;
}

const char* DataDrivenStatusEffect::GetName() const
{
    return mDisplayName.c_str();
}

std::string DataDrivenStatusEffect::GetId() const
{
    return mData.id;
}

StatusEffectView DataDrivenStatusEffect::GetView() const
{
    StatusEffectView view;
    view.id = mData.id;
    view.iconId = mData.iconId;
    view.nameKey = mData.nameKey;
    view.descriptionKey = mData.descriptionKey;
    view.category = mData.category;
    view.remainingTurns = mRemainingTurns;
    view.stackCount = mStackCount;
    view.maxStacks = mData.maxStacks;
    view.dispellable = mData.dispellable;
    return view;
}

bool DataDrivenStatusEffect::TryMergeFrom(IBattler& target, const IStatusEffect& incoming)
{
    const auto* incomingData = dynamic_cast<const DataDrivenStatusEffect*>(&incoming);
    if (!incomingData || incomingData->mData.id != mData.id) return false;

    switch (mData.stackPolicy)
    {
    case StatusStackPolicy::StackIntensity:
        mStackCount = std::min(mData.maxStacks, mStackCount + 1);
        mRemainingTurns = std::max(mRemainingTurns, incomingData->mData.durationTurns);
        RebuildStatModifiers(target);
        return true;
    case StatusStackPolicy::ExtendDuration:
        mRemainingTurns += incomingData->mData.durationTurns;
        return true;
    case StatusStackPolicy::Refresh:
    default:
        mRemainingTurns = std::max(mRemainingTurns, incomingData->mData.durationTurns);
        return true;
    }
}

std::vector<std::unique_ptr<IAction>> DataDrivenStatusEffect::BuildTurnStartActions(
    IBattler& target,
    const BattleContext& /*ctx*/)
{
    std::vector<std::unique_ptr<IAction>> actions;
    const int damage = mData.tickDamage + (mData.tickDamagePerStack * std::max(0, mStackCount - 1));
    if (damage > 0 && target.IsAlive())
    {
        actions.push_back(std::make_unique<StatusDamageAction>(&target, damage, mData.id));
    }
    return actions;
}

void DataDrivenStatusEffect::RebuildStatModifiers(IBattler& target)
{
    if (mSourceId == 0) return;

    target.RemoveStatModifiersBySource(mSourceId);
    const float stackScale = static_cast<float>(std::max(1, mStackCount));
    for (const StatusModifierData& modData : mData.modifiers)
    {
        StatModifier mod;
        mod.op = modData.op;
        mod.target = modData.stat;
        mod.value = modData.value * stackScale;
        mod.sourceId = mSourceId;
        target.AddStatModifier(mod);
    }
}
