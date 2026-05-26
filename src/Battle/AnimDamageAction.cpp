// ============================================================
// File: AnimDamageAction.cpp
// ============================================================
#include "AnimDamageAction.h"
#include "BattleEvents.h"
#include "BattleContext.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "DefaultDamageCalculator.h"
#include <utility>

AnimDamageAction::AnimDamageAction(const DamageRequest& request,
                                    CombatantAnim animType,
                                    float damageMoment,
                                    const BattleContext* ctx)
    : mRequests{ request }
    , mAnimType(animType)
    , mDamageMoment(damageMoment)
    , mCtx(ctx)
{}

AnimDamageAction::AnimDamageAction(std::vector<DamageRequest> requests,
                                    CombatantAnim animType,
                                    float damageMoment,
                                    const BattleContext* ctx)
    : mRequests(std::move(requests))
    , mAnimType(animType)
    , mDamageMoment(damageMoment)
    , mCtx(ctx)
{}

bool AnimDamageAction::Execute(float /*dt*/)
{
    if (!mHasStarted)
    {
        IBattler* attacker = mRequests.empty() ? nullptr : mRequests.front().attacker;
        PlayAnimPayload p = { attacker, mAnimType };
        EventData e; e.payload = &p;
        EventManager::Get().Broadcast("battler_play_anim", e);
        mHasStarted = true;
    }

    // Check progress
    IBattler* attacker = mRequests.empty() ? nullptr : mRequests.front().attacker;
    GetAnimProgressPayload pProg = { attacker, 0.0f };
    EventData eProg; eProg.payload = &pProg;
    EventManager::Get().Broadcast("battler_get_anim_progress", eProg);

    if (!mDamageApplied && pProg.progress >= mDamageMoment)
    {
        if (!mRequests.empty())
        {
            // Fall back to an empty context if BattleManager never injected
            // one; predicate modifiers will be skipped for this single hit.
            BattleContext fallback;
            const BattleContext& ctxRef = mCtx ? *mCtx : fallback;

            DefaultDamageCalculator calculator;
            for (DamageRequest& request : mRequests)
            {
                if (!request.defender) continue;
                DamageResult result = calculator.Calculate(request, ctxRef);
                request.defender->TakeDamage(result, request.attacker);
            }
        }
        mDamageApplied = true;
    }

    // Check if done
    IsAnimDonePayload pDone = { attacker, false };
    EventData eDone; eDone.payload = &pDone;
    EventManager::Get().Broadcast("battler_is_anim_done", eDone);

    return pDone.isDone;
}
