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
//                       Modifies the target's stats (e.g. reduce atk/def).
//                       For DOT effects, Apply() does nothing — damage
//                       comes from OnTurnEnd().
//   OnTurnEnd(target) — called once at the end of the affected combatant's
//                       turn.  Decrements the duration counter.
//   Revert(target)    — called exactly once when the effect expires.
//                       Restores any stat changes made in Apply().
//   IsExpired()       — returns true when the effect has run its full duration.
//   GetName()         — short description for UI and debug logging.
// ============================================================
#pragma once
#include <string>

// Forward declaration — IStatusEffect does not need the full BattlerStats header
// here; only Apply/OnTurnEnd/Revert implementations need it.
struct BattlerStats;

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
    //           Stat-debuff effects reduce atk/def here.
    //           Duration-based effects start their countdown here.
    // Called:   Once, by Combatant::AddEffect().
    // ------------------------------------------------------------
    virtual void Apply(BattlerStats& target) = 0;

    // ------------------------------------------------------------
    // OnTurnEnd
    // Purpose:  Process per-turn logic (DOT damage, countdown decrement).
    //           Stat-buff/debuff effects only decrement their duration here.
    // Called:   Once at the end of the affected combatant's turn.
    // ------------------------------------------------------------
    virtual void OnTurnEnd(BattlerStats& target) = 0;

    // ------------------------------------------------------------
    // Revert
    // Purpose:  Undo any stat changes made during Apply().
    //           Called exactly once, just before the effect is destroyed.
    // Called:   By Combatant::PurgeExpiredEffects() when IsExpired()==true.
    // ------------------------------------------------------------
    virtual void Revert(BattlerStats& target) = 0;

    // Returns true when this effect has run its full duration.
    virtual bool IsExpired() const = 0;

    // Short display name for battle log and UI.
    virtual const char* GetName() const = 0;
};
