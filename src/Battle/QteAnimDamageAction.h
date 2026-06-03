// ============================================================
// File: QteAnimDamageAction.h
// Responsibility: Extends animation-timed damage with slow-motion QTE
//                 mechanics for one target or a grouped AoE hit.
// ============================================================
#pragma once
#include "IAction.h"
#include "IDamageCalculator.h"
#include "CombatantAnim.h"
#include "BattleEvents.h"
#include <vector>

struct BattleContext;

enum class QteTimingFlow
{
    Staggered,
    Queued,
    Chain
};

class QteAnimDamageAction : public IAction
{
public:
    QteAnimDamageAction(const DamageRequest& request,
                        CombatantAnim animType,
                        float qteStartMoment,
                        float damageMoment,
                        float slowMoScale,
                        float perfectMult, float goodMult, float missMult,
                        float perfectThreshold, float goodThreshold,
                        int minCount, int maxCount, float bonusQteCount, float qteSpacing, float qteNodeDuration,
                        QteTimingFlow timingFlow, float qteLeadInSeconds,
                        float fadeInRatio, float fadeOutDuration,
                        const BattleContext* ctx);
    QteAnimDamageAction(std::vector<DamageRequest> requests,
                        CombatantAnim animType,
                        float qteStartMoment,
                        float damageMoment,
                        float slowMoScale,
                        float perfectMult, float goodMult, float missMult,
                        float perfectThreshold, float goodThreshold,
                        int minCount, int maxCount, float bonusQteCount, float qteSpacing, float qteNodeDuration,
                        QteTimingFlow timingFlow, float qteLeadInSeconds,
                        float fadeInRatio, float fadeOutDuration,
                        const BattleContext* ctx);

    ~QteAnimDamageAction() override;

    bool Execute(float dt) override;

private:
    std::vector<DamageRequest> mRequests;
    CombatantAnim mAnimType;

    float mQteStartMoment;
    float mDamageMoment;
    float mSlowMoScale;
    float mPerfectMult;
    float mGoodMult;
    float mMissMult;
    
    float mPerfectThreshold = 0.85f;
    float mGoodThreshold = 0.6f;
    float mBonusQteCount = 0.0f;
    float mQteSpacingSeconds = 0.15f;
    float mQteNodeDurationSeconds = 0.45f;
    QteTimingFlow mTimingFlow = QteTimingFlow::Staggered;
    float mQteLeadInSeconds = 0.0f;
    float mQteElapsedSeconds = 0.0f;
    float mActiveNodeElapsedSeconds = 0.0f;
    float mFadeInRatio = 0.15f;
    float mFadeOutDuration = 0.20f;
    
    struct QteNode {
        float startSeconds = -1.0f;
        float endSeconds = 0.0f;
        QTEResult result = QTEResult::None;
        bool resolved = false;
    };
    std::vector<QteNode> mNodes;
    int mActiveNodeIndex = 0;
    
    // The context pointer is stable because it points into BattleManager
    const BattleContext* mCtx;

    IBattler* GetAttacker() const;
    void BroadcastQteFeedback(QTEResult result, float ratio);
    void PlayQteStartSfx() const;
    void PlayQteResultSfx(QTEResult result) const;
    void ResolveCurrentNode(QTEResult result, float ratio);
    void ResolveCompletedQte();
    void FillQtePayload(QTEStatePayload& qteState) const;
    int GetQueuedVisibleAheadCount() const;
    void EnsureQueuedWindow();
    void UpdateStaggered(float uiDt, bool isKeyPressed, QTEStatePayload& qteState);
    void UpdateQueued(float uiDt, bool isKeyPressed, QTEStatePayload& qteState);
    void UpdateChain(float uiDt, bool isKeyPressed, QTEStatePayload& qteState);

    bool mHasStarted = false;
    bool mQteActive = false;
    bool mActionResolved = false;
    bool mDamageApplied = false;
    bool mKeyWasDown = false;
    bool mQteRageGranted = false;
};
