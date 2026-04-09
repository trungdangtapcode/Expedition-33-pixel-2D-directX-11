// ============================================================
// File: WeakenEffect.cpp
// Responsibility: Implement WeakenEffect — pushes flat ATK/DEF
//                 StatModifier entries on Apply and strips them on Revert.
// ============================================================
#include "WeakenEffect.h"
#include "IBattler.h"
#include "StatModifier.h"

WeakenEffect::WeakenEffect(int duration, int atkReduction, int defReduction)
    : mDuration(duration)
    , mAtkReduction(atkReduction)
    , mDefReduction(defReduction)
{}

// ------------------------------------------------------------
// Apply: push two flat-negative modifiers onto the target.
// Both share the same sourceId so Revert can strip them atomically
// via a single RemoveStatModifiersBySource call.
//
// No clamping here — StatResolver::Get floors resolved stats at 0,
// so even if basAtk + modifier drops below zero the final value the
// damage calculator sees is 0, never negative.
// ------------------------------------------------------------
void WeakenEffect::Apply(IBattler& target)
{
    // Allocate a fresh sourceId the first time Apply runs.  Guarding with
    // zero-check makes a second (mistaken) Apply a no-op rather than
    // leaving two uncorrelated modifier sets behind.
    if (mSourceId != 0) return;
    mSourceId = StatModifierIds::Next();

    StatModifier atkMod;
    atkMod.op       = StatModifier::Op::AddFlat;
    atkMod.target   = StatId::ATK;
    atkMod.value    = -static_cast<float>(mAtkReduction);
    atkMod.sourceId = mSourceId;
    target.AddStatModifier(atkMod);

    StatModifier defMod;
    defMod.op       = StatModifier::Op::AddFlat;
    defMod.target   = StatId::DEF;
    defMod.value    = -static_cast<float>(mDefReduction);
    defMod.sourceId = mSourceId;
    target.AddStatModifier(defMod);
}

// ------------------------------------------------------------
// OnTurnEnd: decrement the duration counter once per target turn.
// No stat changes here — state modifications live in Apply / Revert only.
// ------------------------------------------------------------
void WeakenEffect::OnTurnEnd(IBattler& /*target*/)
{
    if (mDuration > 0) --mDuration;
}

// ------------------------------------------------------------
// Revert: remove both modifiers from the target by sourceId.
// Safe to call multiple times — RemoveStatModifiersBySource is a no-op
// once the matching entries have already been erased.
// ------------------------------------------------------------
void WeakenEffect::Revert(IBattler& target)
{
    if (mSourceId == 0) return;
    target.RemoveStatModifiersBySource(mSourceId);
    mSourceId = 0;
}

bool WeakenEffect::IsExpired() const
{
    return mDuration <= 0;
}
