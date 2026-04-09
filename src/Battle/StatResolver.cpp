// ============================================================
// File: StatResolver.cpp
// Responsibility: Implement StatResolver::Get — folds BASE stats +
//                 active StatModifier entries into an effective int.
// ============================================================
#include "StatResolver.h"
#include "StatModifier.h"
#include "BattleContext.h"
#include "IBattler.h"
#include "BattlerStats.h"

#define NOMINMAX           // prevent Windows.h min/max macro shadowing
#include <algorithm>       // std::max
#include <cmath>           // std::lroundf

namespace
{
    // ------------------------------------------------------------
    // BaseValue
    // Purpose:
    //   Read the raw (unmodified) stat value directly from BattlerStats.
    //   This is the "seed" that modifiers fold over.
    // Why a helper:
    //   Switching on StatId in multiple places would drift.  One switch
    //   in one function keeps the mapping authoritative.
    // Returns:
    //   0 for any stat ID not covered — a safe default, and an invalid
    //   StatId is treated as "nothing to resolve" rather than a crash.
    // ------------------------------------------------------------
    float BaseValue(const BattlerStats& s, StatId stat)
    {
        switch (stat)
        {
        case StatId::ATK:    return static_cast<float>(s.atk);
        case StatId::DEF:    return static_cast<float>(s.def);
        case StatId::MATK:   return static_cast<float>(s.matk);
        case StatId::MDEF:   return static_cast<float>(s.mdef);
        case StatId::SPD:    return static_cast<float>(s.spd);
        case StatId::MAX_HP: return static_cast<float>(s.maxHp);
        case StatId::MAX_MP: return static_cast<float>(s.maxMp);
        }
        return 0.0f;
    }
}

// ------------------------------------------------------------
// StatResolver::Get
//
// Two-pass fold:
//   Pass 1 — accumulate flat + percent contributions (both relative
//            to the BASE value, so order of insertion is irrelevant).
//   Pass 2 — chain-apply multiplicative modifiers on top of the sum.
//
// Why this order:
//   Flat-then-percent-then-multiply is the JRPG standard.  It makes
//   "Berserk (+50%) + Strength Elixir (+20 ATK)" produce the expected
//   (base * 1.5 + 20) not ((base + 20) * 1.5), which punishes stacking.
//
// Clamping:
//   Resolved stats never go below 0.  Negative ATK is meaningless and
//   would produce healing on attacks — ClampHp style floor at 0.
// ------------------------------------------------------------
int StatResolver::Get(const IBattler& battler,
                       const BattleContext& ctx,
                       StatId stat)
{
    const BattlerStats& stats = battler.GetStats();
    const float base = BaseValue(stats, stat);

    // Pass 1: sum flat + percent contributions (both relative to BASE).
    float flatSum    = 0.0f;
    float percentSum = 0.0f;  // accumulates percentage points (e.g. +30 = +30%)

    // Pass 2 will multiply; collect multipliers as we walk so we only
    // iterate once.  Small on-stack vector is overkill — a running
    // product works because multiply order doesn't matter for scalars.
    float product = 1.0f;

    const auto& mods = battler.GetStatModifiers();
    for (const StatModifier& m : mods)
    {
        if (m.target != stat) continue;

        // Evaluate the predicate.  Empty std::function == always active.
        if (m.condition && !m.condition(battler, ctx)) continue;

        switch (m.op)
        {
        case StatModifier::Op::AddFlat:
            flatSum += m.value;
            break;
        case StatModifier::Op::AddPercent:
            percentSum += m.value;
            break;
        case StatModifier::Op::Multiply:
            product *= m.value;
            break;
        }
    }

    // Apply flat + percent (both seeded on BASE), then multiplicative.
    float resolved = (base + flatSum + base * (percentSum / 100.0f)) * product;

    // Floor at 0 — negative stats break the damage formula semantics.
    if (resolved < 0.0f) resolved = 0.0f;

    // Round to nearest int.  lroundf handles the halfway case correctly;
    // static_cast would truncate toward zero and produce off-by-one jitter
    // on values like 12.9999f from floating-point imprecision.
    return static_cast<int>(std::lroundf(resolved));
}

// ------------------------------------------------------------
// StatModifierIds::Next
//   Monotonic non-zero counter.  sourceId == 0 is reserved as "no
//   source" so effects can distinguish an uninitialized modifier
//   from a real one.
// ------------------------------------------------------------
int StatModifierIds::Next()
{
    static int sCounter = 0;
    return ++sCounter;
}
