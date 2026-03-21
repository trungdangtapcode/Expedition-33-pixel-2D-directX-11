// ============================================================
// File: EnemyCombatant.h
// Responsibility: AI-driven IBattler — attacks the living player with most HP.
//
// Stats (MVP):  HP=50  ATK=15  DEF=5  SPD=8  maxRage=0 (no rage)
//
// AI strategy:
//   TakeTurn(players) → attack the first alive player (simplest + consistent).
//   Returns the chosen IBattler* target so BattleManager can build the queue.
// ============================================================
#pragma once
#include "Combatant.h"
#include "AttackSkill.h"
#include "BattlerStats.h"
#include <vector>

class EnemyCombatant : public Combatant
{
public:
    // Legacy constructor — uses hardcoded MVP stats.
    explicit EnemyCombatant(std::string name, std::wstring turnViewPath, std::string attackJsonPath = "data/skills/attack.json");

    // Data-driven constructor — stats come from EnemySlotData (loaded from JSON).
    // Use this when building enemies from EnemyEncounterData::battleParty.
    EnemyCombatant(std::string name, std::wstring turnViewPath, const BattlerStats& stats, std::string attackJsonPath = "data/skills/attack.json");

    bool IsPlayerControlled() const override { return false; }

    // ------------------------------------------------------------
    // ChooseTarget: returns the first alive player in the list.
    //   Returns nullptr if all players are defeated.
    // ------------------------------------------------------------
    IBattler* ChooseTarget(const std::vector<IBattler*>& players) const;

    // Expose the shared attack skill for BattleManager to Execute.
    ISkill* GetAttackSkill() const { return mAttack.get(); }

private:
    std::unique_ptr<AttackSkill> mAttack;
};
