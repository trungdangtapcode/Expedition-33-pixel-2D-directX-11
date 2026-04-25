// ============================================================
// File: PartyManager.h
// Responsibility: Persist the player party's BattlerStats AND
//                 equipment slots across multiple BattleState
//                 instances and overworld traversal.
//
// Problem solved:
//   BattleState and BattleManager are destroyed and recreated for every
//   encounter.  Without a persistent store, Verso's HP would reset to
//   full at the start of every battle and equipment would be lost on
//   transition.  PartyManager holds Verso's BASE stats and equipment
//   slots between battles so wounds and gear carry forward.
//
// Two stat planes — IMPORTANT:
//   1. BASE stats          (mVersoStats):
//        Hardcoded seeds — atk/def/maxHp/etc are character-intrinsic
//        values that only change via leveling (not implemented yet).
//        hp/mp/rage are CURRENT resources that change every battle.
//   2. EFFECTIVE stats     (GetEffectiveStats(index)):
//        Base stats with all currently-equipped item bonuses folded
//        in.  This is what BattleManager seeds the PlayerCombatant
//        with at battle start.
//
// SetMemberStats semantics — IMPORTANT:
//   The new SetMemberStats saves ONLY hp/mp/rage from the input — 
//   all other fields are ignored to prevent base mutation.
//
// Equipment ownership:
//   Each equipped item is moved OUT of Inventory into a member's slot.
//   Unequip moves it back.  This prevents the player from "duping"
//   an item by equipping it and then using the inventory copy.
//
// Lifetime:
//   Created on first Get() call (Meyers' singleton).
//   Destroyed at program exit (static storage duration).
// ============================================================
#pragma once
#include "../Battle/BattlerStats.h"
#include "../Battle/ItemData.h"   // ItemKind, EquipSlot, kEquipSlotCount, SlotIndex
#include <array>
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
    BattlerStats baseStats;
    std::array<std::string, kEquipSlotCount> equipped;
};

class PartyManager
{
public:
    // ------------------------------------------------------------
    // Get: Meyers' Singleton accessor.
    // Thread-safe in C++11+ (magic statics).
    // ------------------------------------------------------------
    static PartyManager& Get();

    // ------------------------------------------------------------
    // GetActiveParty: return the entire party vector securely.
    // ------------------------------------------------------------
    const std::vector<PartyMember>& GetActiveParty() const { return mActiveParty; }
    std::vector<PartyMember>& GetActiveParty() { return mActiveParty; }

    // ------------------------------------------------------------
    // GetMemberStats: return BASE stats for an index.
    // ------------------------------------------------------------
    const BattlerStats& GetMemberStats(size_t index) const { return mActiveParty[index].baseStats; }

    // ------------------------------------------------------------
    // GetEffectiveStats: BASE + every equipped item's bonuses for member.
    // ------------------------------------------------------------
    BattlerStats GetEffectiveStats(size_t index) const;

    // ------------------------------------------------------------
    // PreviewEffectiveStats: layout hypothetical effective stats.
    // ------------------------------------------------------------
    BattlerStats PreviewEffectiveStats(size_t index, EquipSlot slot, const std::string& itemId) const;

    // ------------------------------------------------------------
    // SetMemberStats: persist member's CURRENT resources at battle end.
    // Only hp / mp are saved (base stats remain untouched).
    // ------------------------------------------------------------
    void SetMemberStats(size_t index, const BattlerStats& stats);

    // ------------------------------------------------------------
    // RestoreFullHP: heal Verso to full and reset rage.
    // Used at rest sites and post-game-over screens.
    // ------------------------------------------------------------
    void RestoreFullHP();

    // ============================================================
    //  Equipment API
    // ============================================================

    // ------------------------------------------------------------
    bool EquipItem(size_t index, EquipSlot slot, const std::string& itemId);

    // ------------------------------------------------------------
    // UnequipItem: remove the item in `slot` and return it to Inventory.
    // No-op if the slot is empty.
    // ------------------------------------------------------------
    bool UnequipItem(size_t index, EquipSlot slot);
    
    // ------------------------------------------------------------
    std::string GetEquippedItem(size_t index, EquipSlot slot) const;

private:
    // Private constructor — only Get() may create the instance.
    PartyManager();

    // Non-copyable / non-movable — singleton must not be duplicated.
    PartyManager(const PartyManager&)            = delete;
    PartyManager& operator=(const PartyManager&) = delete;

    std::vector<PartyMember> mActiveParty;
};
