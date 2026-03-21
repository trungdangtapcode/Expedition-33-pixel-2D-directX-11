// ============================================================
// File: Combatant.h
// Responsibility: Base IBattler implementation shared by all battle participants.
//
// Owns:
//   BattlerStats          — current combat numerics (value member, no heap alloc)
//   vector<unique_ptr<IStatusEffect>> — active effects; purged on expiry
//
// Subclasses:
//   PlayerCombatant — adds skill list, awaits UI input for skill selection
//   EnemyCombatant  — adds simple AI to choose target + skill
//
// Lifetime:
//   Owned by BattleManager in two fixed-size arrays (teams).
//   Lives for the duration of one BattleState session.
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "IBattler.h"
#include "BattlerStats.h"
#include "IStatusEffect.h"

class Combatant : public IBattler
{
public:
    // ------------------------------------------------------------
    // Constructor: name + fully initialised stats struct.
    // ------------------------------------------------------------
    explicit Combatant(std::string name, std::wstring turnViewPath, BattlerStats stats);
    virtual ~Combatant() = default;

    // -- IBattler --
    const std::string& GetName() const override;
    const std::wstring& GetTurnViewPath() const override;
          BattlerStats& GetStats()       override;
    const BattlerStats& GetStats() const override;

    // ------------------------------------------------------------
    // TakeDamage:
    //   effective = max(1, rawDamage - target.def)  ← always deals at least 1
    //   source.rage += effective / 4                ← attacker gains rage
    //   target.rage += effective / 8                ← receiver gains rage from pain
    // ------------------------------------------------------------
    void TakeDamage(int rawDamage, IBattler* source) override;

    void AddEffect(std::unique_ptr<IStatusEffect> effect) override;

    // OnTurnStart: currently a no-op base — subclasses may override.
    void OnTurnStart() override;

    // OnTurnEnd: call OnTurnEnd(stats) on every effect, then purge expired.
    void OnTurnEnd() override;

    bool IsAlive() const override;

    // IsPlayerControlled is pure — subclasses declare their team.
    bool IsPlayerControlled() const override = 0;

protected:
    // ------------------------------------------------------------
    // PurgeExpiredEffects:
    //   For each expired effect: call Revert(mStats), then erase.
    //   Must be called at the END of OnTurnEnd — after all effects have
    //   decremented their duration — so an effect that expires this turn
    //   still had its OnTurnEnd logic run before being reverted.
    // ------------------------------------------------------------
    void PurgeExpiredEffects();

    std::string                              mName;
    std::wstring                             mTurnViewPath;
    BattlerStats                             mStats;
    std::vector<std::unique_ptr<IStatusEffect>> mEffects;
};
