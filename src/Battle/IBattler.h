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
#include <memory>
#include "BattlerStats.h"
#include "IStatusEffect.h"

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
    // TakeDamage: apply raw damage reduced by DEF; also grants rage.
    //   rawDamage — pre-reduction damage amount.
    //   source    — the attacker (may be nullptr for environmental dmg).
    //               Source gains rage from dealing damage.
    //               Target gains rage from receiving damage.
    // --------------------------------------------------------
    virtual void TakeDamage(int rawDamage, IBattler* source) = 0;

    // --------------------------------------------------------
    // Effect management — AddEffect transfers ownership immediately.
    // --------------------------------------------------------
    virtual void AddEffect(std::unique_ptr<IStatusEffect> effect) = 0;

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
