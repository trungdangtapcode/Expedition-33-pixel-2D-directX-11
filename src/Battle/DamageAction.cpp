// ============================================================
// File: DamageAction.cpp
// ============================================================
#include "DamageAction.h"
#include "DefaultDamageCalculator.h"

DamageAction::DamageAction(const DamageRequest& request)
    : mRequest(request)
{}

bool DamageAction::Execute(float /*dt*/)
{
    if (mRequest.defender)
    {
        DefaultDamageCalculator calculator;
        DamageResult result = calculator.Calculate(mRequest);
        mRequest.defender->TakeDamage(result, mRequest.attacker);
    }
    return true;    // instantaneous — complete on first frame
}
