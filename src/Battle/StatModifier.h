// ============================================================
// File: StatModifier.h
// Responsibility: One buff/debuff entry folded into a stat by StatResolver.
//
// Design rationale:
//   Status effects used to mutate BattlerStats directly (WeakenEffect
//   did stats.atk -= N on Apply, stats.atk += N on Revert).  That
//   approach breaks as soon as you want:
//     - conditional buffs  ("+20% ATK while HP > 80%")
//     - multiplicative stacking
//     - buffs whose source is dispelled mid-battle
//     - knowing WHERE a stat change came from (for UI tooltips)
//
//   Instead, effects now push a StatModifier onto the battler's modifier
//   list and pop it on Revert.  BattlerStats stores only BASE values;
//   the EFFECTIVE value is recomputed on demand by StatResolver::Get().
//
// Modifier kinds (Op):
//   AddFlat    — final += value
//   AddPercent — final += base * (value / 100)   (percent of BASE, not final)
//   Multiply   — final *= value
//
// Ordering inside StatResolver:
//   1. Accumulate all AddFlat    entries
//   2. Accumulate all AddPercent entries (all relative to the BASE value)
//   3. Chain-apply all Multiply entries in insertion order
//   This order is a fixed convention — not configurable — so the exact
//   result of any buff combination is deterministic.
//
// Conditional modifiers:
//   condition is optional.  If set, the modifier is only active when
//   condition(battler, ctx) returns true.  Null condition == always on.
//   Example: [](const IBattler& b, const BattleContext&){
//              return b.GetStats().hp * 100 < b.GetStats().maxHp * 30;
//            }  // active when HP below 30%
//
// Source tracking:
//   sourceId uniquely identifies the effect instance that created the
//   modifier.  IBattler::RemoveStatModifiersBySource(id) uses this to
//   strip all modifiers belonging to a single effect on revert.
// ============================================================
#pragma once
#include <functional>
#include "StatId.h"

class IBattler;
struct BattleContext;

struct StatModifier
{
    enum class Op
    {
        AddFlat,
        AddPercent,
        Multiply
    };

    Op     op      = Op::AddFlat;
    StatId target  = StatId::ATK;
    float  value   = 0.0f;

    // Unique per source effect instance.
    // StatusEffect constructors call StatModifierIds::Next() to obtain one.
    int    sourceId = 0;

    // Optional predicate — empty std::function == always active.
    // Signature kept flexible so predicates can see both sides.
    using Predicate = std::function<bool(const IBattler&, const BattleContext&)>;
    Predicate condition;
};

// ------------------------------------------------------------
// Source ID allocator.
// Returns a fresh non-zero integer each call.  Not thread-safe; the
// entire battle system runs on the main thread so that's intentional.
// ------------------------------------------------------------
namespace StatModifierIds
{
    int Next();
}
