// ============================================================
// File: WeakenEffect.h
// Responsibility: Debuff that reduces a target's ATK and DEF for N turns.
//
// Implements IStatusEffect.
//
// Mechanic (new, modifier-based):
//   On Apply():      push two StatModifier entries (AddFlat -atk / -def)
//                    tagged with a unique sourceId.
//   On OnTurnEnd():  mDuration decrements; IsExpired() flips when it
//                    reaches 0.
//   On Revert():     target.RemoveStatModifiersBySource(mSourceId) strips
//                    both modifiers in one call.
//
// Why modifiers instead of mutating BattlerStats directly:
//   - Stacking two Weakens produces -30 ATK total (two modifiers), not
//     arithmetic cancellation.
//   - Healing/dispel can remove the debuff cleanly via sourceId.
//   - StatResolver::Get is the single site that folds buffs into a
//     final stat, keeping formulas consistent across every system.
//
// Reduction values come from WeakenSkill's data — no hardcoding here.
// ============================================================
#pragma once
#include "IStatusEffect.h"

class WeakenEffect : public IStatusEffect
{
public:
    // ------------------------------------------------------------
    // Constructor
    // Parameters:
    //   duration     — how many of the TARGET's turns this effect lasts
    //   atkReduction — flat ATK reduction pushed as a StatModifier on Apply
    //   defReduction — flat DEF reduction pushed as a StatModifier on Apply
    // ------------------------------------------------------------
    WeakenEffect(int duration, int atkReduction, int defReduction);

    // IStatusEffect interface (new signature: takes IBattler&).
    void Apply(IBattler& target)     override;
    void OnTurnEnd(IBattler& target) override;
    void Revert(IBattler& target)    override;
    bool IsExpired()    const        override;
    const char* GetName() const      override { return "Weaken"; }

private:
    int mDuration;       // turns remaining (decremented in OnTurnEnd)
    int mAtkReduction;   // magnitude of the flat ATK modifier pushed on Apply
    int mDefReduction;   // magnitude of the flat DEF modifier pushed on Apply
    int mSourceId = 0;   // identifies the modifiers we pushed, for Revert
};
