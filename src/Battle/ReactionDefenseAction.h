// ============================================================
// File: ReactionDefenseAction.h
// Responsibility: Resolve one animation-timed defensive reaction window.
//
// Owns:
//   DamageRequest value data and a ReactionWindowData tuning snapshot.
//
// Lifetime:
//   Created in  -> Skill action builders for enemy attacks.
//   Destroyed in -> ActionQueue after Execute() returns true.
//
// Important:
//   - Uses the existing QTE overlay event so this foundation adds no new UI
//     renderer for V1.
//   - Applies damage only inside Execute(), preserving the action queue as
//     the combat mutation boundary.
// ============================================================
#pragma once

#include "BattleEvents.h"
#include "CombatantAnim.h"
#include "IAction.h"
#include "IDamageCalculator.h"
#include "../Utils/JsonLoader.h"

struct BattleContext;

class ReactionDefenseAction final : public IAction
{
public:
    ReactionDefenseAction(const DamageRequest& request,
                          const JsonLoader::ReactionWindowData& window,
                          CombatantAnim animType,
                          const BattleContext* context);

    bool Execute(float dt) override;

private:
    int ResolveInputKey() const;
    float CurrentPromptProgress(float animationProgress) const;
    void PublishPrompt(bool isActive, QTEResult result, float progressRatio) const;
    void ResolvePrompt(QTEResult result);
    void ApplyDamage();

    DamageRequest mRequest;
    JsonLoader::ReactionWindowData mWindow;
    CombatantAnim mAnimType = CombatantAnim::Attack;
    const BattleContext* mContext = nullptr;

    bool mHasStarted = false;
    bool mPromptResolved = false;
    bool mDamageApplied = false;
    QTEResult mResult = QTEResult::None;
};
