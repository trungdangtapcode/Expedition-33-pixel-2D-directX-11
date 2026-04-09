// ============================================================
// File: DefaultDamageCalculator.h
// Responsibility: Concrete IDamageCalculator implemented as an
//                 ordered pipeline of IDamageStep folds.
//
// Composition:
//   The constructor seeds mPipeline with the default JRPG order:
//     1. BaseFormulaStep   — power*mult - resistance, via StatResolver
//     2. StatusBonusStep   — +20% if defender is afflicted
//     3. CritRollStep      — 10% chance, doubles effective on hit
//     4. FinalClampStep    — floor at 1
//
//   Adding a new term = inserting one entry in the constructor.
//
// Per-call cost:
//   Calculate() walks the vector once.  Each step is a few arithmetic
//   ops, so the chain is O(steps) — currently 4.
// ============================================================
#pragma once
#include "IDamageCalculator.h"
#include "IDamageStep.h"
#include <vector>
#include <memory>

class DefaultDamageCalculator : public IDamageCalculator
{
public:
    // Constructs the default 4-step pipeline.  Callers that want a
    // custom pipeline can subclass and replace mPipeline in their ctor.
    DefaultDamageCalculator();

    DamageResult Calculate(const DamageRequest& request,
                            const BattleContext& ctx) const override;

private:
    // Owned step list.  unique_ptr because every step is a small
    // virtual class with no shared state.
    std::vector<std::unique_ptr<IDamageStep>> mPipeline;
};
