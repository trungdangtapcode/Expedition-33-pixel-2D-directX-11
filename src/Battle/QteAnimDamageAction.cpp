// ============================================================
// File: QteAnimDamageAction.cpp
// ============================================================
#include "QteAnimDamageAction.h"
#include "BattleEvents.h"
#include "BattleContext.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "BattleResourceRules.h"
#include "DefaultDamageCalculator.h"
#include "../Core/TimeSystem.h"
#include "../Core/InputManager.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <utility>

namespace
{
    void BroadcastSfx(const std::string& groupId)
    {
        if (groupId.empty()) return;

        EventData sfxEvent;
        sfxEvent.payload = const_cast<char*>(groupId.c_str());
        EventManager::Get().Broadcast("sfx_play", sfxEvent);
    }
}

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
                                         float bonusQteCount,
                                         float qteSpacing,
                                         float qteNodeDuration,
                                         QteTimingFlow timingFlow,
                                         float qteLeadInSeconds,
                                         float fadeInRatio,
                                         float fadeOutDuration,
                                         const BattleContext* ctx)
    : QteAnimDamageAction(
        std::vector<DamageRequest>{ request },
        animType,
        qteStartMoment,
        damageMoment,
        slowMoScale,
        perfectMult,
        goodMult,
        missMult,
        perfectThreshold,
        goodThreshold,
        minCount,
        maxCount,
        bonusQteCount,
        qteSpacing,
        qteNodeDuration,
        timingFlow,
        qteLeadInSeconds,
        fadeInRatio,
        fadeOutDuration,
        ctx)
{
}

QteAnimDamageAction::QteAnimDamageAction(std::vector<DamageRequest> requests,
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
                                         float bonusQteCount,
                                         float qteSpacing,
                                         float qteNodeDuration,
                                         QteTimingFlow timingFlow,
                                         float qteLeadInSeconds,
                                         float fadeInRatio,
                                         float fadeOutDuration,
                                         const BattleContext* ctx)
    : mRequests(std::move(requests))
    , mAnimType(animType)
    , mQteStartMoment(qteStartMoment)
    , mDamageMoment(damageMoment)
    , mSlowMoScale(slowMoScale)
    , mPerfectMult(perfectMult)
    , mGoodMult(goodMult)
    , mMissMult(missMult)
    , mPerfectThreshold(perfectThreshold)
    , mGoodThreshold(goodThreshold)
    , mBonusQteCount(bonusQteCount)
    , mQteSpacingSeconds((std::max)(0.01f, qteSpacing))
    , mQteNodeDurationSeconds((std::max)(0.05f, qteNodeDuration))
    , mTimingFlow(timingFlow)
    , mQteLeadInSeconds((std::max)(0.0f, qteLeadInSeconds))
    , mFadeInRatio(fadeInRatio)
    , mFadeOutDuration(fadeOutDuration)
    , mCtx(ctx)
{
    const int configuredMax = mCtx ? mCtx->config.maxQteNodes : BattleEventLimits::MaxQteNodes;
    const int safetyMax = std::clamp(configuredMax, 1, BattleEventLimits::MaxQteNodes);
    minCount = std::clamp(minCount, 1, safetyMax);
    maxCount = std::clamp(maxCount, minCount, safetyMax);

    int count = minCount;
    if (maxCount > minCount) {
        count = minCount + (rand() % (maxCount - minCount + 1));
    }
    
    mNodes.resize(count);
    
    // QTE timing uses real UI seconds after the animation reaches
    // qteStartMoment. Animation progress is only the trigger gate; it must not
    // compress prompt duration when a skill has a short attack clip.
    for (int i = 0; i < count; ++i) {
        mNodes[i].startSeconds = mQteLeadInSeconds + static_cast<float>(i) * mQteSpacingSeconds;
        mNodes[i].endSeconds = mNodes[i].startSeconds + mQteNodeDurationSeconds;
        mNodes[i].resolved = false;
        mNodes[i].result = QTEResult::None;
    }

    // Sort to guarantee the player conceptually hits them chronologically.
    std::sort(mNodes.begin(), mNodes.end(), [](const QteNode& a, const QteNode& b) {
        return a.startSeconds < b.startSeconds;
    });
}

QteAnimDamageAction::~QteAnimDamageAction()
{
    if (mQteActive)
    {
        TimeSystem::Get().SetSlowMotion(1.0f);
    }
}

IBattler* QteAnimDamageAction::GetAttacker() const
{
    return mRequests.empty() ? nullptr : mRequests.front().attacker;
}

void QteAnimDamageAction::BroadcastQteFeedback(QTEResult result, float ratio)
{
    // A micro burst of UI via battle payload.
    // The primary user uses BattleState::OnQteFeedback for camera shakes.
    QTEStatePayload qteState{};
    qteState.isActive = true;
    for (int i = 0; i < BattleEventLimits::MaxQteNodes && i < mNodes.size(); ++i) {
        qteState.results[i] = mNodes[i].result;
    }
    qteState.results[mActiveNodeIndex] = result;
    qteState.progressRatios[mActiveNodeIndex] = ratio;
    qteState.result = result; // maintain legacy compat for GameState flash
    qteState.target = GetAttacker();
    qteState.activeIndex = mActiveNodeIndex;
    qteState.totalCount = static_cast<int>(mNodes.size());
    qteState.fadeInRatio = mFadeInRatio;
    qteState.fadeOutDuration = mFadeOutDuration;
    if (mTimingFlow == QteTimingFlow::Chain) qteState.presentationMode = QTEPresentationMode::Chain;
    else if (mTimingFlow == QteTimingFlow::Queued) qteState.presentationMode = QTEPresentationMode::Queued;
    else qteState.presentationMode = QTEPresentationMode::Staggered;
    const JsonLoader::BattleSystemConfig fallbackConfig;
    const JsonLoader::BattleSystemConfig& config = mCtx ? mCtx->config : fallbackConfig;
    qteState.queueVisibleAheadCount = config.qteQueueVisibleAheadCount;
    qteState.chainAnchorXRatio = config.qteChainAnchorXRatio;
    qteState.chainAnchorYRatio = config.qteChainAnchorYRatio;
    qteState.promptRadius = config.qtePromptRadius;
    qteState.frameTextureSize = config.qteFrameTextureSize;
    qteState.chainPreviewScale = config.qteChainPreviewScale;
    qteState.chainPreviewActiveScale = config.qteChainPreviewActiveScale;
    qteState.chainPreviewSpacing = config.qteChainPreviewSpacing;
    qteState.chainPreviewOffsetY = config.qteChainPreviewOffsetY;
    
    PlayQteResultSfx(result);

    EventData qteEvent;
    qteEvent.payload = &qteState;
    EventManager::Get().Broadcast("battler_qte_update", qteEvent);
}

void QteAnimDamageAction::PlayQteStartSfx() const
{
    const JsonLoader::BattleSystemConfig fallbackConfig;
    const JsonLoader::BattleSystemConfig& config = mCtx ? mCtx->config : fallbackConfig;
    BroadcastSfx(config.qteStartSfxId);
}

void QteAnimDamageAction::PlayQteResultSfx(QTEResult result) const
{
    const JsonLoader::BattleSystemConfig fallbackConfig;
    const JsonLoader::BattleSystemConfig& config = mCtx ? mCtx->config : fallbackConfig;

    switch (result)
    {
    case QTEResult::Perfect:
        BroadcastSfx(config.qtePerfectSfxId);
        break;
    case QTEResult::Good:
        BroadcastSfx(config.qteGoodSfxId);
        break;
    case QTEResult::Miss:
        BroadcastSfx(config.qteMissSfxId);
        break;
    default:
        break;
    }
}

void QteAnimDamageAction::ResolveCurrentNode(QTEResult result, float ratio)
{
    if (mActiveNodeIndex < 0 || mActiveNodeIndex >= static_cast<int>(mNodes.size())) return;

    mNodes[mActiveNodeIndex].result = result;
    mNodes[mActiveNodeIndex].resolved = true;
    BroadcastQteFeedback(result, ratio);
    ++mActiveNodeIndex;
}

void QteAnimDamageAction::ResolveCompletedQte()
{
    TimeSystem::Get().SetSlowMotion(1.0f);
    mActionResolved = true;
    mQteActive = false;

    float sum = 0.0f;
    int perfectCount = 0;
    int goodCount = 0;
    for (const auto& n : mNodes) {
        if (n.result == QTEResult::Perfect) { sum += mPerfectMult; ++perfectCount; }
        else if (n.result == QTEResult::Good) { sum += mGoodMult; ++goodCount; }
        else sum += mMissMult;
    }

    const float averagedMultiplier = sum / static_cast<float>(mNodes.size());
    const float earnedBonus =
        (perfectCount * mBonusQteCount) +
        (goodCount * (mBonusQteCount * 0.5f));
    for (DamageRequest& request : mRequests)
    {
        request.qteMultiplier = averagedMultiplier + earnedBonus;
    }

    if (!mQteRageGranted && !mRequests.empty() && mRequests.front().grantsRage)
    {
        BattleResourceRules::Get().EnsureLoaded();
        BattleResourceRules::Get().GrantQteRage(GetAttacker(), perfectCount, goodCount);
        mQteRageGranted = true;
    }
}

void QteAnimDamageAction::FillQtePayload(QTEStatePayload& qteState) const
{
    qteState.isActive = true;
    qteState.target = GetAttacker();
    qteState.activeIndex = mActiveNodeIndex;
    qteState.totalCount = static_cast<int>(mNodes.size());
    qteState.fadeInRatio = mFadeInRatio;
    qteState.fadeOutDuration = mFadeOutDuration;
    if (mTimingFlow == QteTimingFlow::Chain) qteState.presentationMode = QTEPresentationMode::Chain;
    else if (mTimingFlow == QteTimingFlow::Queued) qteState.presentationMode = QTEPresentationMode::Queued;
    else qteState.presentationMode = QTEPresentationMode::Staggered;
    const JsonLoader::BattleSystemConfig fallbackConfig;
    const JsonLoader::BattleSystemConfig& config = mCtx ? mCtx->config : fallbackConfig;
    qteState.queueVisibleAheadCount = config.qteQueueVisibleAheadCount;
    qteState.chainAnchorXRatio = config.qteChainAnchorXRatio;
    qteState.chainAnchorYRatio = config.qteChainAnchorYRatio;
    qteState.promptRadius = config.qtePromptRadius;
    qteState.frameTextureSize = config.qteFrameTextureSize;
    qteState.chainPreviewScale = config.qteChainPreviewScale;
    qteState.chainPreviewActiveScale = config.qteChainPreviewActiveScale;
    qteState.chainPreviewSpacing = config.qteChainPreviewSpacing;
    qteState.chainPreviewOffsetY = config.qteChainPreviewOffsetY;

    for (int i = 0; i < static_cast<int>(mNodes.size()) && i < BattleEventLimits::MaxQteNodes; ++i)
    {
        qteState.results[i] = mNodes[i].result;
    }
}

void QteAnimDamageAction::UpdateStaggered(float uiDt, bool isKeyPressed, QTEStatePayload& qteState)
{
    mQteElapsedSeconds += (std::max)(0.0f, uiDt);

    for (int i = 0; i < static_cast<int>(mNodes.size()) && i < BattleEventLimits::MaxQteNodes; ++i) {
        const float start = mNodes[i].startSeconds;
        const float end = mNodes[i].endSeconds;
        float ratio = 0.0f;

        if (mQteElapsedSeconds >= start && end > start) {
            ratio = (mQteElapsedSeconds - start) / (end - start);
        }
        ratio = std::clamp(ratio, 0.0f, 1.0f);

        qteState.progressRatios[i] = ratio;
        qteState.results[i] = mNodes[i].result;

        if (i == mActiveNodeIndex && !mNodes[i].resolved) {
            const bool timedOut = mQteElapsedSeconds >= end;
            if ((isKeyPressed && mQteElapsedSeconds >= start) || timedOut) {
                QTEResult result = QTEResult::Miss;
                if (isKeyPressed && ratio >= mPerfectThreshold) result = QTEResult::Perfect;
                else if (isKeyPressed && ratio >= mGoodThreshold) result = QTEResult::Good;

                ResolveCurrentNode(result, ratio);
                qteState.results[i] = result;
                isKeyPressed = false;
            }
        }
    }
}

void QteAnimDamageAction::UpdateChain(float uiDt, bool isKeyPressed, QTEStatePayload& qteState)
{
    if (mActiveNodeIndex < 0 || mActiveNodeIndex >= static_cast<int>(mNodes.size())) return;

    mQteElapsedSeconds += (std::max)(0.0f, uiDt);
    mActiveNodeElapsedSeconds += (std::max)(0.0f, uiDt);

    const bool isWindowOpen = mActiveNodeElapsedSeconds >= 0.0f;
    const float ratio = isWindowOpen
        ? std::clamp(mActiveNodeElapsedSeconds / mQteNodeDurationSeconds, 0.0f, 1.0f)
        : 0.0f;

    qteState.activeIndex = mActiveNodeIndex;
    qteState.progressRatios[mActiveNodeIndex] = ratio;

    const bool timedOut = isWindowOpen && mActiveNodeElapsedSeconds >= mQteNodeDurationSeconds;
    if ((isKeyPressed && isWindowOpen) || timedOut)
    {
        QTEResult result = QTEResult::Miss;
        if (isKeyPressed && ratio >= mPerfectThreshold) result = QTEResult::Perfect;
        else if (isKeyPressed && ratio >= mGoodThreshold) result = QTEResult::Good;

        const int resolvedIndex = mActiveNodeIndex;
        ResolveCurrentNode(result, ratio);
        qteState.results[resolvedIndex] = result;
        qteState.progressRatios[resolvedIndex] = ratio;

        if (mActiveNodeIndex < static_cast<int>(mNodes.size()))
        {
            mActiveNodeElapsedSeconds = -mQteSpacingSeconds;
        }
    }
}

bool QteAnimDamageAction::Execute(float /*dt*/)
{
    if (!mHasStarted)
    {
        PlayAnimPayload p = { GetAttacker(), mAnimType };
        EventData e; e.payload = &p;
        EventManager::Get().Broadcast("battler_play_anim", e);
        mHasStarted = true;
    }

    // Measure real animation progression
    GetAnimProgressPayload pProg = { GetAttacker(), 0.0f };
    EventData eProg; eProg.payload = &pProg;
    EventManager::Get().Broadcast("battler_get_anim_progress", eProg);

    float prog = pProg.progress;

    // Transition into QTE phase if we hit the marker.
    if (!mQteActive && !mActionResolved && prog >= mQteStartMoment)
    {
        mQteActive = true;
        mQteElapsedSeconds = 0.0f;
        mActiveNodeElapsedSeconds = -mQteLeadInSeconds;
        // Slow motion scale is global battle feel, while node readability is
        // controlled by the selected timing flow and UI-clock durations.
        TimeSystem::Get().SetSlowMotion(mSlowMoScale);
        PlayQteStartSfx();
    }

    if (mQteActive && !mActionResolved)
    {
        const float uiDt = TimeSystem::Get().GetUIClock().GetDeltaTime();
        QTEStatePayload qteState{};
        FillQtePayload(qteState);
        
        const bool isKeyPressed = InputManager::Get().IsKeyPressed(VK_SPACE);
        if (mTimingFlow == QteTimingFlow::Chain) UpdateChain(uiDt, isKeyPressed, qteState);
        else UpdateStaggered(uiDt, isKeyPressed, qteState);
        
        EventData qteEvent;
        qteEvent.payload = &qteState;
        EventManager::Get().Broadcast("battler_qte_update", qteEvent);

        if (mActiveNodeIndex >= mNodes.size())
        {
            ResolveCompletedQte();
        }
    }

    if (!mDamageApplied && prog >= mDamageMoment && mActionResolved)
    {
        if (!mRequests.empty())
        {
            BattleContext fallback;
            const BattleContext& ctxRef = mCtx ? *mCtx : fallback;

            DefaultDamageCalculator calculator;
            for (DamageRequest& request : mRequests)
            {
                if (!request.defender) continue;
                DamageResult result = calculator.Calculate(request, ctxRef);
                const bool defenderWasAlive = request.defender->IsAlive();
                request.defender->TakeDamage(result, request.attacker);
                const bool defenderWasKilled = defenderWasAlive && !request.defender->IsAlive();
                if (request.grantsRage)
                {
                    BattleResourceRules::Get().EnsureLoaded();
                    BattleResourceRules::Get().GrantDamageRage(
                        request.attacker,
                        request.defender,
                        result,
                        defenderWasKilled);
                }
            }
        }
        mDamageApplied = true;
    }

    IsAnimDonePayload pDone = { GetAttacker(), false };
    EventData eDone; eDone.payload = &pDone;
    EventManager::Get().Broadcast("battler_is_anim_done", eDone);

    // Provide one final "ended" clear frame to ensure UI removes it
    if (pDone.isDone && mDamageApplied) {
        if (!mActionResolved) TimeSystem::Get().SetSlowMotion(1.0f);
        QTEStatePayload qteState{}; 
        qteState.isActive = false; 
        qteState.target = GetAttacker();
        EventData qteEvent; qteEvent.payload = &qteState;
        EventManager::Get().Broadcast("battler_qte_update", qteEvent);
    }

    return pDone.isDone && mDamageApplied;
}
