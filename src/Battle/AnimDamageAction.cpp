// ============================================================
// File: AnimDamageAction.cpp
// ============================================================
#include "AnimDamageAction.h"
#include "BattleEvents.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"

AnimDamageAction::AnimDamageAction(IBattler* attacker, IBattler* defender, int rawDamage, CombatantAnim animType, float damageMoment)
    : mAttacker(attacker), mDefender(defender), mRawDamage(rawDamage), mAnimType(animType), mDamageMoment(damageMoment)
{}

bool AnimDamageAction::Execute(float /*dt*/)
{
    if (!mHasStarted)
    {
        PlayAnimPayload p = { mAttacker, mAnimType };
        EventData e; e.payload = &p;
        EventManager::Get().Broadcast("battler_play_anim", e);
        mHasStarted = true;
    }

    // Check progress
    GetAnimProgressPayload pProg = { mAttacker, 0.0f };
    EventData eProg; eProg.payload = &pProg;
    EventManager::Get().Broadcast("battler_get_anim_progress", eProg);

    if (!mDamageApplied && pProg.progress >= mDamageMoment)
    {
        mDefender->TakeDamage(mRawDamage, mAttacker);
        mDamageApplied = true;
    }

    // Check if done
    IsAnimDonePayload pDone = { mAttacker, false };
    EventData eDone; eDone.payload = &pDone;
    EventManager::Get().Broadcast("battler_is_anim_done", eDone);

    return pDone.isDone;
}
