// ============================================================
// File: DamageAction.cpp
// ============================================================
#include "DamageAction.h"

DamageAction::DamageAction(IBattler* attacker, IBattler* defender, int rawDamage)
    : mAttacker(attacker)
    , mDefender(defender)
    , mRawDamage(rawDamage)
{}

bool DamageAction::Execute(float /*dt*/)
{
    // TakeDamage handles DEF reduction, rage distribution, and logging internally.
    mDefender->TakeDamage(mRawDamage, mAttacker);
    return true;    // instantaneous — complete on first frame
}
