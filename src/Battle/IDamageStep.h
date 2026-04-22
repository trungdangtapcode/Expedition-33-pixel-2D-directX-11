// ============================================================
// File: IDamageStep.h
// Responsibility: One folding step in the damage calculator pipeline.
//
// Design — chain of responsibility, ordered:
//   DefaultDamageCalculator owns a vector<unique_ptr<IDamageStep>>.
//   Calculate() walks the vector in order, passing the same
//   DamageRequest + accumulating DamageResult to each step.
//
//   The ORDER is the formula:
//     1. BaseFormulaStep   — power*mult - resistance
//     2. StatusBonusStep   — bonuses based on attached status effects
//     3. CritRollStep      — random crit, doubles result
//     4. FinalClampStep    — floor at 1
//
//   Adding a new term (e.g. "+10% if caster HP full") = one new step
//   inserted at the right index.  Zero edits to existing steps.
//
// Contract:
//   Apply mutates DamageResult in place.  It must NEVER mutate the
//   request, and it must read all stat values through StatResolver
//   so conditional StatModifiers fold in correctly.
// ============================================================
#pragma once

struct DamageRequest;
struct DamageResult;
struct BattleContext;

class IDamageStep
{
public:
    virtual ~IDamageStep() = default;

    // ------------------------------------------------------------
    // Apply
    // Purpose:
    //   Fold one term into the running DamageResult.
    // Parameters:
    //   request — fully populated input (attacker, defender, type, mult)
    //   result  — accumulator; mutated in place
    //   ctx     — live battle context for predicate-driven reads
    // ------------------------------------------------------------
    virtual void Apply(const DamageRequest& request,
                        DamageResult& result,
                        const BattleContext& ctx) const = 0;
};
