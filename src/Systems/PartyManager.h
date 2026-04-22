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
//   2. EFFECTIVE stats     (GetEffectiveVersoStats()):
//        Base stats with all currently-equipped item bonuses folded
//        in.  This is what BattleManager seeds the PlayerCombatant
//        with at battle start.
//
// SetVersoStats semantics — IMPORTANT:
//   The legacy version assigned the entire BattlerStats struct.  That
//   does not work with equipment because the input struct contains
//   effective values (base + equipment bonuses).  Assigning would
//   silently corrupt the base.  The new SetVersoStats saves ONLY
//   hp/mp/rage from the input — all other fields are ignored.
//
//   Result: equipment bonuses survive across battles automatically
//   because they live on PartyManager (slots), not on the saved stats.
//
// Equipment ownership:
//   Each equipped item is moved OUT of Inventory into the slot.
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

class PartyManager
{
public:
    // ------------------------------------------------------------
    // Get: Meyers' Singleton accessor.
    // Thread-safe in C++11+ (magic statics).
    // ------------------------------------------------------------
    static PartyManager& Get();

    // ------------------------------------------------------------
    // GetVersoStats: return the BASE stats for Verso.  These do NOT
    // include equipment bonuses.
    //
    // Most callers want GetEffectiveVersoStats() instead.  GetVersoStats
    // is preserved for code paths that need the underlying base values
    // (e.g. inventory UI showing "ATK: 25 (base) → 30 (with sword)").
    // ------------------------------------------------------------
    const BattlerStats& GetVersoStats() const { return mVersoStats; }

    // ------------------------------------------------------------
    // GetEffectiveVersoStats: BASE + every equipped item's bonuses.
    // This is what BattleManager seeds the PlayerCombatant with on
    // battle start, so equipment bonuses are visible in combat.
    // ------------------------------------------------------------
    BattlerStats GetEffectiveVersoStats() const;

    // ------------------------------------------------------------
    // PreviewEffectiveStats: hypothetical effective stats if the slot
    // were replaced with `itemId`.  Used by inventory UI to show
    // "ATK 25 → 37" before/after preview rows.
    //
    // Pass an empty itemId to preview unequipping the slot.
    // ------------------------------------------------------------
    BattlerStats PreviewEffectiveStats(EquipSlot slot, const std::string& itemId) const;

    // ------------------------------------------------------------
    // SetVersoStats: persist Verso's CURRENT resources at battle end.
    //
    // Only hp / mp are saved — atk/def/maxHp/etc are intentionally
    // ignored because the input struct may have equipment bonuses
    // baked in (BattleManager passes the effective stats).  Saving
    // them would mutate the base, which is wrong.
    //
    // Rage is reset to 0 — it is a per-battle resource by design.
    // ------------------------------------------------------------
    void SetVersoStats(const BattlerStats& stats);

    // ------------------------------------------------------------
    // RestoreFullHP: heal Verso to full and reset rage.
    // Used at rest sites and post-game-over screens.
    // ------------------------------------------------------------
    void RestoreFullHP();

    // ============================================================
    //  Equipment API
    // ============================================================

    // ------------------------------------------------------------
    // GetEquipped: id of the item in `slot`, empty string if none.
    // ------------------------------------------------------------
    std::string GetEquipped(EquipSlot slot) const;

    // ------------------------------------------------------------
    // IsEquipped: convenience predicate for UI gating.
    // ------------------------------------------------------------
    bool IsEquipped(EquipSlot slot) const { return !GetEquipped(slot).empty(); }

    // ------------------------------------------------------------
    // Equip: install `itemId` into `slot`.
    //
    // Validates:
    //   - itemId exists in ItemRegistry
    //   - item.equipSlot matches `slot`
    //   - item is currently in Inventory (count > 0)
    //
    // On success:
    //   - any previous item in the slot returns to Inventory (+1)
    //   - the new item leaves Inventory (-1)
    //   - mEquipped[SlotIndex(slot)] = itemId
    //
    // Returns false on any validation failure (logs and leaves state
    // unchanged).
    // ------------------------------------------------------------
    bool Equip(EquipSlot slot, const std::string& itemId);

    // ------------------------------------------------------------
    // Unequip: remove the item in `slot` and return it to Inventory.
    // No-op if the slot is empty.
    // ------------------------------------------------------------
    void Unequip(EquipSlot slot);

private:
    // Private constructor — only Get() may create the instance.
    PartyManager() = default;

    // Non-copyable / non-movable — singleton must not be duplicated.
    PartyManager(const PartyManager&)            = delete;
    PartyManager& operator=(const PartyManager&) = delete;

    // ------------------------------------------------------------
    // mVersoStats: Verso's BASE stats.
    //
    // hp/mp/rage change every battle (current resources).
    // atk/def/maxHp/etc are intrinsic and only change via leveling.
    //
    // Default values match the legacy PlayerCombatant MVP constants:
    //   HP=100  ATK=25  DEF=10  SPD=10  maxRage=100
    // In the full game these will be loaded from data/characters/verso.json.
    // ------------------------------------------------------------
    BattlerStats mVersoStats {
        100, // hp
        100, // maxHp
        50,  // mp
        50,  // maxMp
        25,  // atk
        10,  // def
        25,  // matk
        10,  // mdef
        10,  // spd
        0,   // rage
        100  // maxRage
    };

    // Equipment slots — one item id per slot, "" = empty.
    // Indexed by SlotIndex(EquipSlot).  Size is kEquipSlotCount.
    std::array<std::string, kEquipSlotCount> mEquipped{};
};
