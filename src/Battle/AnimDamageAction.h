// ============================================================
// File: AnimDamageAction.h
// Responsibility: Plays an animation and applies damage at a specific normalized time progress.
// ============================================================
#pragma once

#include "IAction.h"
#include "IBattler.h"
#include "CombatantAnim.h"
#include "IDamageCalculator.h"

class AnimDamageAction : public IAction
{
public:
    AnimDamageAction(const DamageRequest& request, CombatantAnim animType, float damageMoment);

    bool Execute(float dt) override;

private:
    DamageRequest mRequest;
    CombatantAnim mAnimType;
    float         mDamageMoment;

    bool mHasStarted = false;
    bool mDamageApplied = false;
};
