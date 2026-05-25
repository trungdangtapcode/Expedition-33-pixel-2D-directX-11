// ============================================================
// File: PartyManager.h
// Responsibility: Persist player party stats and equipment across
//                 battles, overworld traversal, and save/load.
//
// Problem solved:
//   BattleState and BattleManager are destroyed and recreated for every
//   encounter. Without a persistent store, party HP, MP, EXP, levels, and
//   equipment would reset whenever a battle ends.
//
// Stat planes:
//   1. Base stats:
//      Character-intrinsic values loaded from data/characters/*.json and
//      advanced by the leveling system. Current HP, MP, and rage live here
//      because they must survive across battles.
//   2. Effective stats:
//      Base stats plus equipped item bonuses. BattleManager seeds
//      PlayerCombatant instances from this folded view.
//
// Save/load:
//   PartyManager exposes PartyMemberProgress snapshots so SaveManager can
//   serialize durable progress without knowing how party metadata, sprite
//   paths, or equipment folding work internally.
//
// Equipment ownership:
//   Equipped items are moved out of Inventory into a member slot. Unequip
//   returns the item to Inventory, preventing duplicated usable copies.
//
// Lifetime:
//   Created on first Get() call as a Meyers singleton.
//   Destroyed at program exit by static storage duration cleanup.
// ============================================================
#pragma once

#include "../Battle/BattlerStats.h"
#include "../Battle/ItemData.h"
#include <array>
#include <cmath>
#include <string>
#include <vector>

struct PartyMember
{
    std::string id;
    std::string name;
    std::wstring animationPath;
    std::string animJsonPath;
    std::wstring hpFramePath;
    std::wstring turnViewPath;
    std::vector<std::string> skillPaths;
    BattlerStats baseStats;
    std::array<std::string, kEquipSlotCount> equipped;
};

struct PartyMemberProgress
{
    std::string id;
    BattlerStats baseStats;
    std::array<std::string, kEquipSlotCount> equipped;
};

class PartyManager
{
public:
    // ------------------------------------------------------------
    // Function: Get
    // Purpose:
    //   Return the single process-wide party manager.
    // Why:
    //   Game states are short-lived, but party progress must survive state
    //   pushes, pops, and full scene changes.
    // ------------------------------------------------------------
    static PartyManager& Get();

    static int ExpToNextLevel(int level)
    {
        return GetExpCurve(level);
    }

    // ------------------------------------------------------------
    // Function: ResetToDefaults
    // Purpose:
    //   Rebuild party members from character data files.
    // Why:
    //   New Game and failed save loads need a clean, data-driven baseline
    //   without restarting the process.
    // ------------------------------------------------------------
    void ResetToDefaults();

    const std::vector<PartyMember>& GetActiveParty() const { return mActiveParty; }
    std::vector<PartyMember>& GetActiveParty() { return mActiveParty; }

    const BattlerStats& GetMemberStats(size_t index) const { return mActiveParty[index].baseStats; }

    // ------------------------------------------------------------
    // Function: GetEffectiveStats
    // Purpose:
    //   Return base stats plus equipped item bonuses for one member.
    // Why:
    //   Battle and UI surfaces need the same folded stat view, and the
    //   folding rule must stay centralized.
    // ------------------------------------------------------------
    BattlerStats GetEffectiveStats(size_t index) const;

    BattlerStats PreviewEffectiveStats(size_t index, EquipSlot slot, const std::string& itemId) const;

    // ------------------------------------------------------------
    // Function: SetMemberStats
    // Purpose:
    //   Persist current battle resources after an encounter.
    // Why:
    //   PlayerCombatant is temporary, while PartyManager is the durable
    //   authority for HP and MP between battles.
    // Caveats:
    //   - Only HP, MP, and rage are read from stats.
    //   - Base attack, defense, level, and EXP are intentionally left alone.
    // ------------------------------------------------------------
    void SetMemberStats(size_t index, const BattlerStats& stats);

    // ------------------------------------------------------------
    // Function: RestoreFullHP
    // Purpose:
    //   Heal every active member and reset rage.
    // Why:
    //   Rest sites, game-over recovery, and debug flows need one durable
    //   party-wide recovery hook.
    // ------------------------------------------------------------
    void RestoreFullHP();

    // ------------------------------------------------------------
    // Function: AddExp
    // Purpose:
    //   Grant cumulative EXP to active party members and apply level-ups.
    // Why:
    //   PartyManager owns persistent character progression, so the level
    //   curve and stat growth should not live in BattleManager.
    // ------------------------------------------------------------
    void AddExp(int amount);

    // ------------------------------------------------------------
    // Function: CaptureProgress
    // Purpose:
    //   Return a save-friendly snapshot of durable party progress.
    // Why:
    //   SaveManager should not reach into PartyMember internals or serialize
    //   render metadata that can be reloaded from data files.
    // ------------------------------------------------------------
    std::vector<PartyMemberProgress> CaptureProgress() const;

    // ------------------------------------------------------------
    // Function: ApplyProgress
    // Purpose:
    //   Overlay saved stats and equipment onto the current data-loaded party.
    // Why:
    //   Save files store stable member ids, while the game still owns current
    //   metadata such as sprite paths and UI frame paths.
    // ------------------------------------------------------------
    void ApplyProgress(const std::vector<PartyMemberProgress>& progress);

    bool EquipItem(size_t index, EquipSlot slot, const std::string& itemId);
    bool UnequipItem(size_t index, EquipSlot slot);
    std::string GetEquippedItem(size_t index, EquipSlot slot) const;

private:
    static int GetExpCurve(int level)
    {
        // The threshold scales faster than linearly so early levels arrive
        // quickly while later progression stretches out.
        return static_cast<int>(100.0f * std::pow(level, 1.5f));
    }

    PartyManager();

    PartyManager(const PartyManager&)            = delete;
    PartyManager& operator=(const PartyManager&) = delete;

    std::vector<PartyMember> mActiveParty;
};
