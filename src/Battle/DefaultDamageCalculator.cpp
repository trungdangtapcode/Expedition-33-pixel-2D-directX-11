// ============================================================
// File: DefaultDamageCalculator.cpp
// Responsibility: Build the default damage pipeline and run it.
//
// All actual stat math lives inside the IDamageStep classes — this
// file only orders them.  The order IS the formula; rearranging the
// constructor's push_back sequence changes how every hit is computed.
// ============================================================
#include "DefaultDamageCalculator.h"
#include "DamageSteps.h"

// ------------------------------------------------------------
// Constructor: seed the canonical 4-step pipeline.
//
// Order rationale:
//   Base must come first — every later step folds onto its output.
//   StatusBonus must run BEFORE CritRoll so the multiplier compounds:
//     base 100  ->  status 120  ->  crit 240
//   FinalClamp is always last so any negative intermediate floors
//   to 1 instead of leaking through to the receiver.
// ------------------------------------------------------------
DefaultDamageCalculator::DefaultDamageCalculator()
{
    mPipeline.push_back(std::make_unique<BaseFormulaStep>());
    mPipeline.push_back(std::make_unique<StatusBonusStep>());
    mPipeline.push_back(std::make_unique<CritRollStep>());
    mPipeline.push_back(std::make_unique<FinalClampStep>());
}

DamageResult DefaultDamageCalculator::Calculate(const DamageRequest& request,
                                                  const BattleContext& ctx) const
{
    DamageResult result;
    if (!request.attacker || !request.defender)
    {
        // Invalid request — return a zeroed result rather than crashing.
        return result;
    }

    // Walk the pipeline in registration order.  Each step mutates
    // result in place.  No early-out: every step is responsible for
    // doing nothing when it has nothing to add.
    for (const auto& step : mPipeline)
    {
        step->Apply(request, result, ctx);
    }

    return result;
}
