// ============================================================
// File: IBattler.h
// Responsibility: Pure virtual interface for any participant in a battle.
//
// Implemented by:
//   Combatant (base) → PlayerCombatant, EnemyCombatant
//
// Purpose:
//   BattleManager works exclusively through this interface so it never
//   needs to know whether a slot is player-controlled or AI-driven.
//   Skills and Actions also receive IBattler* to stay decoupled.
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "BattlerStats.h"
#include "IStatusEffect.h"
#include "IDamageCalculator.h"  // For DamageResult
#include "StatModifier.h"       // Stat modifier pipeline

class IBattler
{
public:
    virtual ~IBattler() = default;

    // --------------------------------------------------------
    // Identity
    // --------------------------------------------------------
    virtual const std::string& GetName() const = 0;
    virtual const std::wstring& GetTurnViewPath() const = 0;

    // --------------------------------------------------------
    // Stats — mutable ref allows skills/actions to modify directly.
    // --------------------------------------------------------
    virtual       BattlerStats& GetStats()       = 0;
    virtual const BattlerStats& GetStats() const = 0;

    // --------------------------------------------------------
    // TakeDamage: apply calculated damage; also grants rage.
    //   result    - encapsulated post-evaluation damage amount.
    //   source    - the attacker (may be nullptr).
    //               Source gains rage from dealing damage.
    //               Target gains rage from receiving damage.
    // --------------------------------------------------------
    virtual void TakeDamage(const DamageResult& result, IBattler* source) = 0;

    // --------------------------------------------------------
    // Effect management — AddEffect transfers ownership immediately.
    // --------------------------------------------------------
    virtual void AddEffect(std::unique_ptr<IStatusEffect> effect) = 0;

    // Remove every attached status effect at once — used by Cleanse items.
    // Implementations MUST call Revert() on each effect before releasing
    // it so any StatModifier entries the effect pushed get stripped too.
    virtual void ClearAllStatusEffects() = 0;

    // True if the battler currently has at least one status effect.
    // Read by damage pipeline steps that grant bonuses against
    // afflicted targets without caring which effect is attached.
    virtual bool HasAnyStatusEffect() const = 0;

    // --------------------------------------------------------
    // Stat modifier pipeline (StatResolver / StatModifier.h).
    //
    // IStatusEffect implementations push a StatModifier onto the target
    // on Apply and call RemoveStatModifiersBySource in Revert with the
    // sourceId they pushed.  StatResolver::Get walks GetStatModifiers()
    // every time a combat formula needs an effective stat value.
    //
    // BASE values stay in BattlerStats unchanged — modifiers are the
    // ONLY way to change what ATK/DEF/etc. return from StatResolver.
    // --------------------------------------------------------
    virtual void AddStatModifier(const StatModifier& mod) = 0;
    virtual void RemoveStatModifiersBySource(int sourceId) = 0;
    virtual const std::vector<StatModifier>& GetStatModifiers() const = 0;

    // --------------------------------------------------------
    // Turn hooks — BattleManager calls these each turn.
    // --------------------------------------------------------
    virtual void OnTurnStart() = 0;   // trigger any start-of-turn effects
    virtual void OnTurnEnd()   = 0;   // decrement effect durations + purge expired

    // --------------------------------------------------------
    // Query
    // --------------------------------------------------------
    virtual bool IsAlive()            const = 0;
    virtual bool IsPlayerControlled() const = 0;
};
