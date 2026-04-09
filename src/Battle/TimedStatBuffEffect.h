// ============================================================
// File: TimedStatBuffEffect.h
// Responsibility: Generic stat buff/debuff that lasts N turns.
//
// Implements IStatusEffect.
//
// Design — data-driven companion to WeakenEffect:
//   WeakenEffect hardcodes "reduce ATK and DEF by a flat amount".
//   TimedStatBuffEffect accepts any (stat, op, value, duration)
//   combination so item and skill systems can share ONE effect class
//   for every buff/debuff that just pushes a StatModifier on Apply
//   and strips it on Revert.
//
// Mechanic:
//   On Apply():     push a single StatModifier (stat, op, value)
//                   tagged with a fresh sourceId.
//   On OnTurnEnd(): duration decrements; IsExpired() flips at 0.
//   On Revert():    strip the modifier by sourceId.
//
// Where this is used:
//   - power_tonic:  ATK +30% for 3 turns (AddPercent, value=30, duration=3)
//   - iron_draft:   DEF +30% for 3 turns (AddPercent, value=30, duration=3)
//   - swift_feather:SPD +50% for 3 turns (AddPercent, value=50, duration=3)
//
// Naming:
//   The effect name string is built from the stat ("Buff ATK", "Buff DEF")
//   so the battle log and debug HUD can identify it without a per-item
//   custom class.
// ============================================================
#pragma once
#include "IStatusEffect.h"
#include "StatId.h"
#include "StatModifier.h"

class TimedStatBuffEffect : public IStatusEffect
{
public:
    // ------------------------------------------------------------
    // Constructor
    // Parameters:
    //   stat        — which stat the modifier targets (ATK, DEF, SPD…)
    //   op          — how the modifier folds (AddFlat / AddPercent / Multiply)
    //   value       — magnitude (e.g. 30.0 for +30%, -15.0 for a debuff)
    //   durationTurns — how many of the target's turns the modifier lasts
    // ------------------------------------------------------------
    TimedStatBuffEffect(StatId stat,
                        StatModifier::Op op,
                        float value,
                        int durationTurns);

    // IStatusEffect interface
    void Apply(IBattler& target)     override;
    void OnTurnEnd(IBattler& target) override;
    void Revert(IBattler& target)    override;
    bool IsExpired()    const        override;
    const char* GetName() const      override;

private:
    StatId            mStat;
    StatModifier::Op  mOp;
    float             mValue;
    int               mDuration;       // turns remaining
    int               mSourceId = 0;   // id of the pushed modifier (for Revert)

    // Cached display name so GetName() can return a const char*.
    // Built once in the constructor so we don't allocate per-call.
    mutable std::string mDisplayName;
};
