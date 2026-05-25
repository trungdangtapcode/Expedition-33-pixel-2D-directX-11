// ============================================================
// File: DataDrivenStatusEffect.h
// Responsibility: Runtime IStatusEffect backed by StatusEffectData.
// ============================================================
#pragma once
#include "IStatusEffect.h"
#include "StatusEffectData.h"

class DataDrivenStatusEffect : public IStatusEffect
{
public:
    explicit DataDrivenStatusEffect(StatusEffectData data);

    void Apply(IBattler& target) override;
    void OnTurnEnd(IBattler& target) override;
    void Revert(IBattler& target) override;
    bool IsExpired() const override;
    const char* GetName() const override;
    std::string GetId() const override;
    StatusEffectView GetView() const override;
    bool TryMergeFrom(IBattler& target, const IStatusEffect& incoming) override;
    std::vector<std::unique_ptr<IAction>> BuildTurnStartActions(
        IBattler& target,
        const BattleContext& ctx) override;

private:
    void RebuildStatModifiers(IBattler& target);

    StatusEffectData mData;
    int mRemainingTurns = 1;
    int mStackCount = 1;
    int mSourceId = 0;
    bool mSkipFirstTurnEnd = true;
    std::string mDisplayName;
};
