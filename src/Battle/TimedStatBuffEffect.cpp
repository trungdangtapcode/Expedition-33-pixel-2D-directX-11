// ============================================================
// File: TimedStatBuffEffect.cpp
// Responsibility: Implement TimedStatBuffEffect — one StatModifier
//                 wrapped in an IStatusEffect with a turn counter.
// ============================================================
#include "TimedStatBuffEffect.h"
#include "IBattler.h"

namespace
{
    // ------------------------------------------------------------
    // StatLabel: short human label for battle-log naming.
    // Matches StatId entries.  Kept as a file-local helper so no
    // header is forced to include a to_string() overload.
    // ------------------------------------------------------------
    const char* StatLabel(StatId s)
    {
        switch (s)
        {
        case StatId::ATK:    return "ATK";
        case StatId::DEF:    return "DEF";
        case StatId::MATK:   return "MATK";
        case StatId::MDEF:   return "MDEF";
        case StatId::SPD:    return "SPD";
        case StatId::MAX_HP: return "MaxHP";
        case StatId::MAX_MP: return "MaxMP";
        }
        return "STAT";
    }
}

TimedStatBuffEffect::TimedStatBuffEffect(StatId stat,
                                          StatModifier::Op op,
                                          float value,
                                          int durationTurns)
    : mStat(stat)
    , mOp(op)
    , mValue(value)
    , mDuration(durationTurns)
{
    // Build the display name once so GetName() is O(1) and non-allocating.
    // Examples:  "Buff ATK +30%"  /  "Debuff DEF -15"
    const bool isDebuff = (value < 0.0f);
    const bool isPercent = (op == StatModifier::Op::AddPercent);

    char sign   = isDebuff ? '-' : '+';
    float abs   = isDebuff ? -value : value;
    char buf[64];
    // Use %g for compactness; 0.0f would print as "0" not "0.000000".
    _snprintf_s(buf, sizeof(buf), _TRUNCATE,
                isPercent ? "%s %s %c%g%%" : "%s %s %c%g",
                isDebuff ? "Debuff" : "Buff",
                StatLabel(stat),
                sign, abs);
    mDisplayName = buf;
}

// ------------------------------------------------------------
// Apply: push one StatModifier; store the id so Revert can strip it.
// No-op on a second Apply — prevents duplicate modifiers if an upstream
// bug double-attaches the effect.
// ------------------------------------------------------------
void TimedStatBuffEffect::Apply(IBattler& target)
{
    if (mSourceId != 0) return;
    mSourceId = StatModifierIds::Next();

    StatModifier mod;
    mod.op       = mOp;
    mod.target   = mStat;
    mod.value    = mValue;
    mod.sourceId = mSourceId;
    target.AddStatModifier(mod);
}

// ------------------------------------------------------------
// OnTurnEnd: countdown only; no stat mutation.
// ------------------------------------------------------------
void TimedStatBuffEffect::OnTurnEnd(IBattler& /*target*/)
{
    if (mDuration > 0) --mDuration;
}

// ------------------------------------------------------------
// Revert: strip the modifier by sourceId.  Safe to call multiple times.
// ------------------------------------------------------------
void TimedStatBuffEffect::Revert(IBattler& target)
{
    if (mSourceId == 0) return;
    target.RemoveStatModifiersBySource(mSourceId);
    mSourceId = 0;
}

bool TimedStatBuffEffect::IsExpired() const
{
    return mDuration <= 0;
}

const char* TimedStatBuffEffect::GetName() const
{
    return mDisplayName.c_str();
}
