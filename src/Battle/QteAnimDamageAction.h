// ============================================================
// File: QteAnimDamageAction.h
// Responsibility: Extends standard anim-damage with slow motion QTE mechanics.
// ============================================================
#pragma once
#include "IAction.h"
#include "IDamageCalculator.h"
#include "CombatantAnim.h"
#include "BattleEvents.h"
#include <vector>

struct BattleContext;

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
                        int minCount, int maxCount, float bonusQteCount, float qteSpacing,
                        float fadeInRatio, float fadeOutDuration,
                        const BattleContext* ctx);

    bool Execute(float dt) override;

private:
    DamageRequest mRequest;
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
    float mFadeInRatio = 0.15f;
    float mFadeOutDuration = 0.20f;
    
    struct QteNode {
        float startProg = -1.0f;
        float perfectProg = 0.0f;
        QTEResult result = QTEResult::None;
        bool resolved = false;
    };
    std::vector<QteNode> mNodes;
    int mActiveNodeIndex = 0;
    
    // The context pointer is stable because it points into BattleManager
    const BattleContext* mCtx;

    void BroadcastQteFeedback(QTEResult result, float ratio);

    bool mHasStarted = false;
    bool mQteActive = false;
    bool mActionResolved = false;
    bool mDamageApplied = false;
    bool mKeyWasDown = false;
};
