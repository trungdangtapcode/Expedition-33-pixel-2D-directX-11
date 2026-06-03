// ============================================================
// File: Combatant.h
// Responsibility: Base IBattler implementation shared by all battle participants.
//
// Owns:
//   BattlerStats - current combat numerics stored by value.
//   vector<unique_ptr<IStatusEffect>> - active effects, purged on expiry.
//
// Subclasses:
//   PlayerCombatant - adds skill list and awaits UI input.
//   EnemyCombatant  - adds simple AI to choose target and skill.
//
// Lifetime:
//   Owned by BattleManager in two fixed-size arrays.
//   Lives for the duration of one BattleState session.
// ============================================================
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "IBattler.h"
#include "BattlerStats.h"
#include "IStatusEffect.h"
#include "StatModifier.h"

class Combatant : public IBattler
{
public:
    // ------------------------------------------------------------
    // Constructor: name + fully initialized stats struct.
    // ------------------------------------------------------------
    explicit Combatant(std::string name,
                       std::wstring turnViewPath,
                       BattlerStats stats,
                       std::string debugName = std::string());
    virtual ~Combatant() = default;

    // -- IBattler --
    const std::string& GetName() const override;
    const std::string& GetDebugName() const override;
    const std::wstring& GetTurnViewPath() const override;
          BattlerStats& GetStats()       override;
    const BattlerStats& GetStats() const override;

    // ------------------------------------------------------------
    // TakeDamage:
    //   Applies HP loss, death animation, and damage events only.
    //   Resource gains are handled by the calling damage action after
    //   final damage is known.
    // ------------------------------------------------------------
    void TakeDamage(const DamageResult& result, IBattler* source) override;

    void AddEffect(std::unique_ptr<IStatusEffect> effect) override;
    void ClearAllStatusEffects() override;
    bool HasAnyStatusEffect() const override { return !mEffects.empty(); }
    std::vector<StatusEffectView> GetStatusEffectViews() const override;

    // Stat modifier storage - see IBattler for the pipeline contract.
    void AddStatModifier(const StatModifier& mod) override;
    void RemoveStatModifiersBySource(int sourceId) override;
    const std::vector<StatModifier>& GetStatModifiers() const override;

    // OnTurnStart: currently a no-op base; subclasses may override.
    void OnTurnStart() override;

    // OnTurnEnd: call OnTurnEnd(*this) on every effect, then purge expired.
    void OnTurnEnd() override;
    std::vector<std::unique_ptr<IAction>> BuildTurnStartActions(const BattleContext& ctx) override;

    bool IsAlive() const override;

    // IsPlayerControlled is pure; subclasses declare their team.
    bool IsPlayerControlled() const override = 0;

protected:
    // ------------------------------------------------------------
    // PurgeExpiredEffects:
    //   For each expired effect: call Revert(mStats), then erase.
    //   Must be called at the end of OnTurnEnd so an effect that expires
    //   this turn still runs its final tick logic before being reverted.
    // ------------------------------------------------------------
    void PurgeExpiredEffects();

    std::string mName;
    std::string mDebugName;
    std::wstring mTurnViewPath;
    BattlerStats mStats;
    std::vector<std::unique_ptr<IStatusEffect>> mEffects;

    // Active stat modifiers. Effects push these through AddStatModifier,
    // and StatResolver reads them for every formula stat lookup.
    std::vector<StatModifier> mStatModifiers;
};
