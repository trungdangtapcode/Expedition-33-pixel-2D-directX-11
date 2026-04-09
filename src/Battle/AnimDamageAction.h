// ============================================================
// File: AnimDamageAction.h
// Responsibility: Plays an animation and applies damage at a specific
//                 normalized time progress.
//
// Context wiring:
//   Stores a non-owning const BattleContext* (same rules as DamageAction).
//   Injected at enqueue time so the calculator sees the live state when
//   Execute fires on the exact animation frame.
// ============================================================
#pragma once

#include "IAction.h"
#include "IBattler.h"
#include "CombatantAnim.h"
#include "IDamageCalculator.h"

struct BattleContext;

class AnimDamageAction : public IAction
{
public:
    AnimDamageAction(const DamageRequest& request,
                     CombatantAnim animType,
                     float damageMoment,
                     const BattleContext* ctx = nullptr);

    bool Execute(float dt) override;

    // Injector used by BattleManager::EnqueueSkillActions.
    void SetContext(const BattleContext* ctx) { mCtx = ctx; }

private:
    DamageRequest        mRequest;
    CombatantAnim        mAnimType;
    float                mDamageMoment;
    const BattleContext* mCtx = nullptr;

    bool mHasStarted = false;
    bool mDamageApplied = false;
};
