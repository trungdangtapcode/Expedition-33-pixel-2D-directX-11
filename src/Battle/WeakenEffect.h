// ============================================================
// File: WeakenEffect.h
// Responsibility: Debuff that reduces a target's ATK and DEF for N turns.
//
// Implements IStatusEffect.
//
// Mechanic:
//   On Apply():    target.atk -= mAtkReduction;  target.def -= mDefReduction;
//   On Revert():   target.atk += mAtkReduction;  target.def += mDefReduction;
//   On each turn end: mDuration decrements; IsExpired() when duration == 0.
//
// Reduction values come from WeakenSkill's data — no hardcoding here.
// The skill controls how strong the debuff is; this class just applies it.
//
// Example: WeakenEffect(2, 15, 10) → reduce ATK by 15, DEF by 10 for 2 turns.
// ============================================================
#pragma once
#include "IStatusEffect.h"

// ============================================================
// Class: WeakenEffect
// ============================================================
class WeakenEffect : public IStatusEffect
{
public:
    // ------------------------------------------------------------
    // Constructor
    // Parameters:
    //   duration     — how many of the TARGET's turns this effect lasts
    //   atkReduction — flat ATK reduction applied on attach
    //   defReduction — flat DEF reduction applied on attach
    // ------------------------------------------------------------
    WeakenEffect(int duration, int atkReduction, int defReduction);

    // IStatusEffect interface
    void Apply(BattlerStats& target)     override;
    void OnTurnEnd(BattlerStats& target) override;
    void Revert(BattlerStats& target)    override;
    bool IsExpired()    const            override;
    const char* GetName() const          override { return "Weaken"; }

private:
    int mDuration;       // turns remaining (decremented in OnTurnEnd)
    int mAtkReduction;   // amount subtracted from target.atk on Apply
    int mDefReduction;   // amount subtracted from target.def on Apply
    bool mApplied = false; // guard: Revert is a no-op if Apply never ran
};
