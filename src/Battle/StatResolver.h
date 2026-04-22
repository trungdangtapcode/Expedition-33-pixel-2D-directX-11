// ============================================================
// File: StatResolver.h
// Responsibility: Free-function facade that folds a battler's BASE
//                 stat plus every active StatModifier into an
//                 effective integer value.
//
// Design rationale:
//   Every combat system used to read stats.atk / stats.def directly.
//   That shortcut is banned from now on — all reads go through
//   StatResolver::Get so buffs/debuffs/conditional bonuses take effect
//   uniformly everywhere (damage calculation, UI display, AI scoring).
//
// Contract:
//   Get(battler, ctx, stat) returns an int ≥ 0.
//   Negative intermediate results are clamped at 0 before rounding.
//   Multiplicative modifiers are applied after flat + percent.
//
// Why a namespace and not a class:
//   StatResolver is stateless.  Making it a class would force a
//   pointless instance on the stack of every caller.
// ============================================================
#pragma once
#include "StatId.h"

class IBattler;
struct BattleContext;

namespace StatResolver
{
    // ------------------------------------------------------------
    // Get
    // Purpose:
    //   Resolve the effective value of `stat` for `battler` under
    //   the current `ctx`.
    // Steps:
    //   1. Read the BASE value from battler.GetStats() (switch on stat).
    //   2. Walk battler.GetStatModifiers() and, for every modifier
    //      whose condition is empty or evaluates true, fold it into
    //      the running value using the op-order documented in
    //      StatModifier.h.
    //   3. Clamp the final result to max(0, ...).
    // Parameters:
    //   battler — the combatant whose stat to read (non-null via ref).
    //   ctx     — read-only battle context (needed for predicates).
    //   stat    — which stat to resolve.
    // Returns:
    //   The effective, clamped integer value.
    // ------------------------------------------------------------
    int Get(const IBattler& battler, const BattleContext& ctx, StatId stat);
}
