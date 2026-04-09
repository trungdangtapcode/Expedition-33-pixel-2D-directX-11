// ============================================================
// File: DamageAction.h
// Responsibility: Atomic action — deal damage from attacker to defender.
//
// Completes instantly (returns true on first Execute call).
// Rage is distributed inside Combatant::TakeDamage, not here.
//
// Context wiring:
//   The action stores a raw const BattleContext* pointing at
//   BattleManager::mContext (stable for the battle duration).  The
//   snapshot CONTENTS change each frame via BattleManager::RebuildContext,
//   so by the time Execute() runs the damage calculator sees the
//   CURRENT alive lists, turn count, and conditional modifiers — not
//   the values captured at queue time.
//
//   Never store a BattleContext by value inside this action — it would
//   freeze at queue time and miss every state change before execution.
// ============================================================
#pragma once
#include "IAction.h"
#include "IBattler.h"
#include "IDamageCalculator.h"

struct BattleContext;

class DamageAction : public IAction
{
public:
    // ctx is optional at construction — BattleManager may inject it later
    // when the action is enqueued.  If still null at Execute time, the
    // action falls back to a freshly-constructed empty context so the
    // calculator still runs (buffs with predicates won't activate).
    DamageAction(const DamageRequest& request, const BattleContext* ctx = nullptr);

    // Runs DefaultDamageCalculator::Calculate on mRequest, then calls TakeDamage.
    // Completes in one frame.
    bool Execute(float dt) override;

    // Injector used by BattleManager::EnqueueSkillActions when a skill
    // constructs its actions without access to the live context.
    void SetContext(const BattleContext* ctx) { mCtx = ctx; }

private:
    DamageRequest        mRequest;
    const BattleContext* mCtx = nullptr;   // non-owning; points into BattleManager
};
