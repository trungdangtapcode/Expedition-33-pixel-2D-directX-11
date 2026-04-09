// ============================================================
// File: ItemEffectAction.h
// Responsibility: Apply ONE item effect to ONE battler in the action
//                 queue.  Instantaneous — completes on first Execute.
//
// Scope — one action, one target:
//   AoE items (fire_bomb, elixir-on-party) expand at skill-build time
//   into N single-target ItemEffectActions, one per target.  This
//   keeps the single-target code path the only one that mutates
//   state, which matches how the rest of the action queue works.
//
// Dispatch:
//   The effect kind selects the code branch inside Execute:
//     HealHp         — target.hp += amount (clamped)
//     HealMp         — target.mp += amount (clamped)
//     FullHeal       — hp = maxHp, mp = maxMp
//     Revive         — no-op on alive; dead -> hp = amount
//     RestoreRage    — BattlerStats::AddRage(amount)
//     DealDamage     — build a DamageResult and call TakeDamage
//     StatBuff       — target.AddEffect(TimedStatBuffEffect(...))
//     Cleanse        — mEffects.clear() via custom combatant path
//
// Logging:
//   Every branch logs via LOG() so the debug console can show what
//   actually happened.  A separate LogAction can provide the pretty
//   battle-log line shown in the UI.
// ============================================================
#pragma once
#include "IAction.h"
#include "IBattler.h"
#include "ItemData.h"

class ItemEffectAction : public IAction
{
public:
    // target — single non-null battler this effect lands on
    // effect — copied subset of ItemData so the action is self-contained
    ItemEffectAction(IBattler* target, ItemData effect);

    // Runs the dispatch table on mEffect.effect.  Completes in one frame.
    bool Execute(float dt) override;

private:
    IBattler* mTarget;
    ItemData  mEffect;   // local value copy — decouples action from registry
};
