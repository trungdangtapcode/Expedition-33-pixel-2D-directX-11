// ============================================================
// File: MoveAction.h
// Responsibility: Interpolates a combatant's position over time without blocking the main loop.
//
// Ownership: ActionQueue owns all actions via unique_ptr<IAction>.
// ============================================================
#pragma once

#include "IAction.h"
#include "IBattler.h"
#include "CombatantAnim.h"

class MoveAction : public IAction
{
public:
    enum class TargetType { MeleeRange, Origin };

    MoveAction(IBattler* mover, IBattler* target, TargetType type, float duration,
               float meleeOffset = 80.0f,
               CombatantAnim movingAnim = CombatantAnim::BattleMove,
               CombatantAnim stopAnim = CombatantAnim::BattleUnmove);

    bool Execute(float dt) override;

private:
    IBattler*   mMover;
    IBattler*   mTarget;
    TargetType  mType;
    float       mDuration;
    float       mTimer = 0.0f;
    bool        mHasStarted = false;

    float       mStartX = 0.f;
    float       mStartY = 0.f;
    float       mTargetOffsetX = 0.f;
    float       mTargetOffsetY = 0.f;
    float       mMeleeOffset   = 80.0f;

    CombatantAnim mMovingAnim;
    CombatantAnim mStopAnim;
};
