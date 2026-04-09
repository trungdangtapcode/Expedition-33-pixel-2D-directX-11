// ============================================================
// File: AnimDamageAction.cpp
// ============================================================
#include "AnimDamageAction.h"
#include "BattleEvents.h"
#include "BattleContext.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "DefaultDamageCalculator.h"

AnimDamageAction::AnimDamageAction(const DamageRequest& request,
                                    CombatantAnim animType,
                                    float damageMoment,
                                    const BattleContext* ctx)
    : mRequest(request)
    , mAnimType(animType)
    , mDamageMoment(damageMoment)
    , mCtx(ctx)
{}

bool AnimDamageAction::Execute(float /*dt*/)
{
    if (!mHasStarted)
    {
        PlayAnimPayload p = { mRequest.attacker, mAnimType };
        EventData e; e.payload = &p;
        EventManager::Get().Broadcast("battler_play_anim", e);
        mHasStarted = true;
    }

    // Check progress
    GetAnimProgressPayload pProg = { mRequest.attacker, 0.0f };
    EventData eProg; eProg.payload = &pProg;
    EventManager::Get().Broadcast("battler_get_anim_progress", eProg);

    if (!mDamageApplied && pProg.progress >= mDamageMoment)
    {
        if (mRequest.defender)
        {
            // Fall back to an empty context if BattleManager never injected
            // one; predicate modifiers will be skipped for this single hit.
            BattleContext fallback;
            const BattleContext& ctxRef = mCtx ? *mCtx : fallback;

            DefaultDamageCalculator calculator;
            DamageResult result = calculator.Calculate(mRequest, ctxRef);
            mRequest.defender->TakeDamage(result, mRequest.attacker);
        }
        mDamageApplied = true;
    }

    // Check if done
    IsAnimDonePayload pDone = { mRequest.attacker, false };
    EventData eDone; eDone.payload = &pDone;
    EventManager::Get().Broadcast("battler_is_anim_done", eDone);

    return pDone.isDone;
}
