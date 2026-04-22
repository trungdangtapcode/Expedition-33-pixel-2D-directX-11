// ============================================================
// File: PlayAnimationAction.cpp
// ============================================================
#include "PlayAnimationAction.h"
#include "BattleEvents.h"
#include "../Events/EventManager.h"

PlayAnimationAction::PlayAnimationAction(IBattler* target, CombatantAnim animType, bool wait)
    : mTarget(target), mAnimType(animType), mWait(wait)
{}

bool PlayAnimationAction::Execute(float /*dt*/)
{
    if (!mHasStarted)
    {
        PlayAnimPayload p = { mTarget, mAnimType };
        EventData e; e.payload = &p;
        EventManager::Get().Broadcast("battler_play_anim", e);
        mHasStarted = true;
    }

    if (!mWait) return true;

    // Check if done
    IsAnimDonePayload pDone = { mTarget, false };
    EventData eDone; eDone.payload = &pDone;
    EventManager::Get().Broadcast("battler_is_anim_done", eDone);
    
    return pDone.isDone; 
}
