// ============================================================
// File: QteAnimDamageAction.cpp
// ============================================================
#include "QteAnimDamageAction.h"
#include "BattleEvents.h"
#include "BattleContext.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "DefaultDamageCalculator.h"
#include "../Core/TimeSystem.h"
#include "../Core/InputManager.h"
#include <windows.h>
#include <cmath>
#include <algorithm>

QteAnimDamageAction::QteAnimDamageAction(const DamageRequest& request,
                                         CombatantAnim animType,
                                         float qteStartMoment,
                                         float damageMoment,
                                         float slowMoScale,
                                         float perfectMult,
                                         float goodMult,
                                         float missMult,
                                         float perfectThreshold,
                                         float goodThreshold,
                                         int minCount,
                                         int maxCount,
                                         float qteSpacing,
                                         float fadeInRatio,
                                         float fadeOutDuration,
                                         const BattleContext* ctx)
    : mRequest(request)
    , mAnimType(animType)
    , mQteStartMoment(qteStartMoment)
    , mDamageMoment(damageMoment)
    , mSlowMoScale(slowMoScale)
    , mPerfectMult(perfectMult)
    , mGoodMult(goodMult)
    , mMissMult(missMult)
    , mPerfectThreshold(perfectThreshold)
    , mGoodThreshold(goodThreshold)
    , mFadeInRatio(fadeInRatio)
    , mFadeOutDuration(fadeOutDuration)
    , mCtx(ctx)
{
    int count = minCount;
    if (maxCount > minCount) {
        count = minCount + (rand() % (maxCount - minCount + 1));
    }
    
    mNodes.resize(count);
    
    // Nodes now have an explicit duration. We will pull this from your JSON config via qteSpacing.
    float duration = qteSpacing; 
    
    // We must ensure the node completes completely before mDamageMoment fires.
    float maxWindowStart = mDamageMoment - duration;
    if (maxWindowStart < mQteStartMoment) maxWindowStart = mQteStartMoment;
    
    // Distribute nodes evenly with a small random jitter to prevent stacked overlaps!
    float totalWindow = maxWindowStart - mQteStartMoment;
    float segment = (count > 0) ? (totalWindow / static_cast<float>(count)) : 0.0f;
    
    for (int i = 0; i < count; ++i) {
        float randRatio = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        // Place it inside its segmented block to ensure rhythmic spacing, scaled to 70% to guarantee a gap
        float jitterOffset = randRatio * (segment * 0.7f);
        mNodes[i].startProg = mQteStartMoment + (i * segment) + jitterOffset;
        mNodes[i].perfectProg = mNodes[i].startProg + duration;
        mNodes[i].resolved = false;
        mNodes[i].result = QTEResult::None;
    }

    // Sort to guarantee the player conceptually hits them chronologically by startProg
    std::sort(mNodes.begin(), mNodes.end(), [](const QteNode& a, const QteNode& b) {
        return a.startProg < b.startProg;
    });
}

void QteAnimDamageAction::BroadcastQteFeedback(QTEResult result, float ratio)
{
    // A micro burst of UI via battle payload.
    // The primary user uses BattleState::OnQteFeedback for camera shakes.
    QTEStatePayload qteState;
    qteState.isActive = true;
    for (int i = 0; i < MAX_QTE_NODES && i < mNodes.size(); ++i) {
        qteState.results[i] = mNodes[i].result;
    }
    qteState.results[mActiveNodeIndex] = result;
    qteState.progressRatios[mActiveNodeIndex] = ratio;
    qteState.result = result; // maintain legacy compat for GameState flash
    qteState.target = mRequest.attacker;
    qteState.activeIndex = mActiveNodeIndex;
    qteState.totalCount = static_cast<int>(mNodes.size());
    qteState.fadeInRatio = mFadeInRatio;
    qteState.fadeOutDuration = mFadeOutDuration;
    
    EventData qteEvent;
    qteEvent.payload = &qteState;
    EventManager::Get().Broadcast("battler_qte_update", qteEvent);
}


bool QteAnimDamageAction::Execute(float /*dt*/)
{
    if (!mHasStarted)
    {
        PlayAnimPayload p = { mRequest.attacker, mAnimType };
        EventData e; e.payload = &p;
        EventManager::Get().Broadcast("battler_play_anim", e);
        mHasStarted = true;
    }

    // Measure real animation progression
    GetAnimProgressPayload pProg = { mRequest.attacker, 0.0f };
    EventData eProg; eProg.payload = &pProg;
    EventManager::Get().Broadcast("battler_get_anim_progress", eProg);

    float prog = pProg.progress;

    // Transition into QTE phase if we hit the marker.
    if (!mQteActive && !mActionResolved && prog >= mQteStartMoment && prog < mDamageMoment)
    {
        mQteActive = true;
        // Slow motion scale matches the config exactly. Unaffected by the amount of concurrent nodes!
        TimeSystem::Get().SetSlowMotion(mSlowMoScale);
    }

    if (mQteActive && !mActionResolved)
    {
        QTEStatePayload qteState;
        qteState.isActive = true;
        qteState.target = mRequest.attacker;
        qteState.activeIndex = mActiveNodeIndex;
        qteState.totalCount = static_cast<int>(mNodes.size());
        qteState.fadeInRatio = mFadeInRatio;
        qteState.fadeOutDuration = mFadeOutDuration;
        
        bool isKeyPressed = InputManager::Get().IsKeyPressed(VK_SPACE);
        
        // Evaluate every single concurrent Node for rendering updates
        for (int i = 0; i < mNodes.size() && i < MAX_QTE_NODES; ++i) {
            float start = mNodes[i].startProg;
            float end = mNodes[i].perfectProg;
            float ratio = 0.0f;
            
            if (prog >= start && end > start) {
                ratio = (prog - start) / (end - start);
            }
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            
            qteState.progressRatios[i] = ratio;
            qteState.results[i] = mNodes[i].result;
            
            // Only process logic if it is the target chronologically active Node
            if (i == mActiveNodeIndex && !mNodes[i].resolved) {
                bool timedOut = prog >= end;
                
                // Allow evaluation only if it hasn't completely timed out before we got here
                if ((isKeyPressed && prog >= start) || timedOut) {
                    QTEResult r = QTEResult::Miss;
                    if (isKeyPressed && ratio >= mPerfectThreshold) r = QTEResult::Perfect;
                    else if (isKeyPressed && ratio >= mGoodThreshold) r = QTEResult::Good;
                    
                    mNodes[i].result = r;
                    mNodes[i].resolved = true;
                    qteState.results[i] = r; // Update live payload immediately
                    
                    BroadcastQteFeedback(r, ratio);
                    
                    mActiveNodeIndex++;
                    isKeyPressed = false; // consume the input per frame!
                }
            }
        }
        
        EventData qteEvent;
        qteEvent.payload = &qteState;
        EventManager::Get().Broadcast("battler_qte_update", qteEvent);

        if (mActiveNodeIndex >= mNodes.size())
        {
            // All QTE nodes resolved.
            TimeSystem::Get().SetSlowMotion(1.0f);
            mActionResolved = true;
            mQteActive = false;
            
            // Average Multiplier logic (to prevent 3x damage scaling)
            float sum = 0.0f;
            for (const auto& n : mNodes) {
                if (n.result == QTEResult::Perfect) sum += mPerfectMult;
                else if (n.result == QTEResult::Good) sum += mGoodMult;
                else sum += mMissMult;
            }
            // Strict averaging based on user performance across all randomly spawned nodes
            mRequest.qteMultiplier = sum / static_cast<float>(mNodes.size());
        }
    }

    if (!mDamageApplied && prog >= mDamageMoment)
    {
        if (mRequest.defender)
        {
            // If lag spike somehow jumped over the entire QTE window, default it
            if (!mActionResolved) {
                mActionResolved = true;
                mQteActive = false;
                TimeSystem::Get().SetSlowMotion(1.0f);
            }
            
            BattleContext fallback;
            const BattleContext& ctxRef = mCtx ? *mCtx : fallback;

            DefaultDamageCalculator calculator;
            DamageResult result = calculator.Calculate(mRequest, ctxRef);
            mRequest.defender->TakeDamage(result, mRequest.attacker);
        }
        mDamageApplied = true;
    }

    IsAnimDonePayload pDone = { mRequest.attacker, false };
    EventData eDone; eDone.payload = &pDone;
    EventManager::Get().Broadcast("battler_is_anim_done", eDone);

    // Provide one final "ended" clear frame to ensure UI removes it
    if (pDone.isDone) {
        if (!mActionResolved) TimeSystem::Get().SetSlowMotion(1.0f);
        QTEStatePayload qteState; 
        qteState.isActive = false; 
        qteState.target = mRequest.attacker;
        EventData qteEvent; qteEvent.payload = &qteState;
        EventManager::Get().Broadcast("battler_qte_update", qteEvent);
    }

    return pDone.isDone;
}
