// ============================================================
// File: DamageAction.cpp
// ============================================================
#include "DamageAction.h"
#include "BattleResourceRules.h"
#include "DefaultDamageCalculator.h"
#include "BattleContext.h"

DamageAction::DamageAction(const DamageRequest& request, const BattleContext* ctx)
    : mRequest(request)
    , mCtx(ctx)
{}

bool DamageAction::Execute(float /*dt*/)
{
    if (mRequest.defender)
    {
        // Fall back to a default-constructed empty context if the action
        // was queued without one - keeps the calculator safe to run but
        // disables any predicate-gated modifiers for this single hit.
        BattleContext fallback;
        const BattleContext& ctxRef = mCtx ? *mCtx : fallback;

        DefaultDamageCalculator calculator;
        DamageResult result = calculator.Calculate(mRequest, ctxRef);
        const bool defenderWasAlive = mRequest.defender->IsAlive();
        mRequest.defender->TakeDamage(result, mRequest.attacker);
        const bool defenderWasKilled = defenderWasAlive && !mRequest.defender->IsAlive();
        if (mRequest.grantsRage)
        {
            BattleResourceRules::Get().EnsureLoaded();
            BattleResourceRules::Get().GrantDamageRage(
                mRequest.attacker,
                mRequest.defender,
                result,
                defenderWasKilled);
        }
    }
    return true;    // instantaneous - complete on first frame
}
