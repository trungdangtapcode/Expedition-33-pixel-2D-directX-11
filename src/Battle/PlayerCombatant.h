// ============================================================
// File: PlayerCombatant.h
// Responsibility: IBattler for a player-controlled hero.
//
// Owns:
//   vector<unique_ptr<ISkill>> mSkills — available skills (loaded at construction)
//
// Turn flow:
//   BattleManager calls RequestAction(targets) each player turn.
//   PlayerCombatant stores the pending choice from SetPendingAction().
//   BattleManager reads it via ConsumePendingAction() to build the queue.
//
// Stats (data-driven in production; constructed inline for MVP):
//   HP=100  ATK=25  DEF=10  SPD=10  maxRage=100
// ============================================================
#pragma once
#include "Combatant.h"
#include "ISkill.h"
#include <vector>
#include <memory>

class PlayerCombatant : public Combatant
{
public:
    // ------------------------------------------------------------
    // Constructor (name only): creates Verso with default MVP stats.
    //   Used when no persisted stats are available.
    // Constructor (name + stats): seeds the combatant from saved data —
    //   used by BattleManager::Initialize() to restore persistent HP.
    // In the full game stats always come from data/characters/verso.json
    // via PartyManager; the name-only overload is kept for testing.
    // ------------------------------------------------------------
    explicit PlayerCombatant(std::string name, std::wstring turnViewPath, std::string attackJsonPath = "data/skills/verso_attack.json");
    PlayerCombatant(std::string name, std::wstring turnViewPath, const BattlerStats& seedStats, std::string attackJsonPath = "data/skills/verso_attack.json");

    bool IsPlayerControlled() const override { return true; }

    // -- Skill access --
    int                    GetSkillCount() const;
    ISkill*                GetSkill(int index) const;   // returns nullptr if out of range

    // --------------------------------------------------------
    // Pending action — set by BattleState UI, consumed by BattleManager.
    //
    // Two flavors of pending action:
    //   - Skill: SetPendingAction(skillIndex, target)
    //   - Item:  SetPendingItem(itemId, target)
    // Exactly one is queued per turn.  HasPendingAction() returns true
    // for either; GetPendingItemId() is empty when the queued action
    // is a skill, and non-empty when it is an item.
    // --------------------------------------------------------
    void    SetPendingAction(int skillIndex, IBattler* target);
    void    SetPendingItem(const std::string& itemId, IBattler* target);
    bool    HasPendingAction()   const;
    int     GetPendingSkillIndex() const;
    const std::string& GetPendingItemId() const;
    IBattler* GetPendingTarget()   const;
    void    ClearPendingAction();

private:
    std::vector<std::unique_ptr<ISkill>> mSkills;

    // Pending selection — only valid when mHasPendingAction == true.
    bool        mHasPendingAction  = false;
    int         mPendingSkillIndex = -1;
    std::string mPendingItemId;                  // empty == skill action, non-empty == item action
    IBattler*   mPendingTarget     = nullptr;    // non-owning observer pointer
};
