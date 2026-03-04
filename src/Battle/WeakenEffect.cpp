// ============================================================
// File: WeakenEffect.cpp
// Responsibility: Implement WeakenEffect — ATK/DEF debuff for N turns.
// ============================================================
#include "WeakenEffect.h"
#include "BattlerStats.h"

WeakenEffect::WeakenEffect(int duration, int atkReduction, int defReduction)
    : mDuration(duration)
    , mAtkReduction(atkReduction)
    , mDefReduction(defReduction)
{}

// ------------------------------------------------------------
// Apply: subtract reductions immediately from the target's live stats.
// The original values are NOT stored here — Revert adds them back.
// If the reduction drives a stat below 0, it is clamped at 0 to prevent
// negative ATK or DEF (which could heal instead of reduce damage).
// ------------------------------------------------------------
void WeakenEffect::Apply(BattlerStats& target)
{
    target.atk -= mAtkReduction;
    target.def -= mDefReduction;

    // Prevent negative stats — a stat floored at 0 means no contribution,
    // but never inverts the formula (e.g. negative DEF adding damage).
    if (target.atk < 0) target.atk = 0;
    if (target.def < 0) target.def = 0;

    mApplied = true;
}

// ------------------------------------------------------------
// OnTurnEnd: decrement the duration counter once per target turn.
// No stat changes here — stat changes happen in Apply / Revert only.
// ------------------------------------------------------------
void WeakenEffect::OnTurnEnd(BattlerStats& /*target*/)
{
    if (mDuration > 0) --mDuration;
}

// ------------------------------------------------------------
// Revert: restore the stat reductions when the effect expires.
// Called exactly once by Combatant::PurgeExpiredEffects().
// Guard mApplied prevents double-revert if something goes wrong.
// ------------------------------------------------------------
void WeakenEffect::Revert(BattlerStats& target)
{
    if (!mApplied) return;
    target.atk += mAtkReduction;
    target.def += mDefReduction;
    mApplied = false;
}

bool WeakenEffect::IsExpired() const
{
    return mDuration <= 0;
}
