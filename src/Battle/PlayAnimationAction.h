// ============================================================
// File: PlayAnimationAction.h
// Responsibility: Fires an animation clip on a combatant and blocks the queue
//                 sequence until the sprite finishes playing it.
//
// Ownership: ActionQueue owns all actions via unique_ptr<IAction>.
// ============================================================
#pragma once

#include "IAction.h"
#include "IBattler.h"
#include "CombatantAnim.h"

class PlayAnimationAction : public IAction
{
public:
    PlayAnimationAction(IBattler* target, CombatantAnim animType, bool wait);

    bool Execute(float dt) override;

private:
    IBattler*     mTarget;
    CombatantAnim mAnimType;
    bool          mWait;
    bool          mHasStarted = false;
};
