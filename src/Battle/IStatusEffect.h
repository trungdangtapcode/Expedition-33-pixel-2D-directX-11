// ============================================================
// File: IStatusEffect.h
// Responsibility: Pure virtual interface for all battle status effects.
//
// Design — Open/Closed Principle:
//   New effects (Poison, Burn, Stun, Shield, Regeneration...) are added
//   by creating new subclasses of IStatusEffect.  The effect pipeline
//   in Combatant never changes — it just calls Apply() / OnTurnEnd()
//   through the interface.
//
// Lifetime model:
//   Effects are owned by the combatant they are applied to, stored as
//   vector<unique_ptr<IStatusEffect>>.  When IsExpired() returns true,
//   Combatant::PurgeExpiredEffects() removes and destroys them.
//
// Effect contract:
//   Apply(target)     — called immediately when the effect is attached.
//                       Stat-buff/debuff effects push a StatModifier via
//                       target.AddStatModifier(...) and remember the
//                       sourceId they used so Revert can strip it.
//                       DOT effects do nothing here — their damage
//                       is applied from OnTurnEnd().
//   OnTurnEnd(target) — called once at the end of the affected combatant's
//                       turn.  Decrement duration, apply DOT damage, etc.
//   Revert(target)    — called exactly once when the effect expires.
//                       Removes any StatModifier entries added by Apply()
//                       (typically via target.RemoveStatModifiersBySource).
//   IsExpired()       — returns true when the effect has run its full duration.
//   GetName()         — short description for UI and debug logging.
//
// Interface widening note:
//   Apply/OnTurnEnd/Revert take IBattler& (not BattlerStats&) so effects
//   can reach the stat modifier pipeline.  DOT damage still needs live
//   HP via target.GetStats().
// ============================================================
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "IAction.h"
#include "StatusEffectView.h"

class IBattler;
struct BattleContext;

// ============================================================
// Interface: IStatusEffect
// ============================================================
class IStatusEffect
{
public:
    virtual ~IStatusEffect() = default;

    // ------------------------------------------------------------
    // Apply
    // Purpose:  Attach this effect to the target immediately.
    //           Stat-debuff effects push a StatModifier here.
    //           Duration-based effects start their countdown here.
    // Called:   Once, by Combatant::AddEffect().
    // ------------------------------------------------------------
    virtual void Apply(IBattler& target) = 0;

    // ------------------------------------------------------------
    // OnTurnEnd
    // Purpose:  Process per-turn logic (DOT damage, countdown decrement).
    //           Stat-buff/debuff effects only decrement their duration here.
    // Called:   Once at the end of the affected combatant's turn.
    // ------------------------------------------------------------
    virtual void OnTurnEnd(IBattler& target) = 0;

    // Build actions that run at the start of the owner's turn.
    // Most effects return none; damage-over-time effects return actions
    // so combat mutations still flow through ActionQueue.
    virtual std::vector<std::unique_ptr<IAction>> BuildTurnStartActions(
        IBattler& target,
        const BattleContext& ctx)
    {
        (void)target;
        (void)ctx;
        return {};
    }

    // ------------------------------------------------------------
    // Revert
    // Purpose:  Undo any state changes made during Apply().
    //           Typically removes StatModifier entries by sourceId.
    //           Called exactly once, just before the effect is destroyed.
    // Called:   By Combatant::PurgeExpiredEffects() when IsExpired()==true.
    // ------------------------------------------------------------
    virtual void Revert(IBattler& target) = 0;

    // Returns true when this effect has run its full duration.
    virtual bool IsExpired() const = 0;

    // Short display name for battle log and UI.
    virtual const char* GetName() const = 0;

    // Stable id used for stacking/refresh rules.
    virtual std::string GetId() const { return GetName(); }

    // UI snapshot.  Legacy effects use a neutral fallback until migrated.
    virtual StatusEffectView GetView() const
    {
        StatusEffectView view;
        view.id = GetId();
        view.nameKey = GetName();
        view.descriptionKey = GetName();
        return view;
    }

    // Merge a new incoming effect into this active effect when ids match.
    // Returns true when the incoming effect was consumed and should not be
    // stored separately.
    virtual bool TryMergeFrom(IBattler& target, const IStatusEffect& incoming)
    {
        (void)target;
        (void)incoming;
        return false;
    }
};
