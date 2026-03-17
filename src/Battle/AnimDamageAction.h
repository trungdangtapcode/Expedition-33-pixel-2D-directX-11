// ============================================================
// File: AnimDamageAction.h
// Responsibility: Plays an animation and applies damage at a specific normalized time progress.
// ============================================================
#pragma once

#include "IAction.h"
#include "IBattler.h"
#include "CombatantAnim.h"

class AnimDamageAction : public IAction
{
public:
    AnimDamageAction(IBattler* attacker, IBattler* defender, int rawDamage, CombatantAnim animType, float damageMoment);

    bool Execute(float dt) override;

private:
    IBattler*     mAttacker;
    IBattler*     mDefender;
    int           mRawDamage;
    CombatantAnim mAnimType;
    float         mDamageMoment;

    bool mHasStarted = false;
    bool mDamageApplied = false;
};
