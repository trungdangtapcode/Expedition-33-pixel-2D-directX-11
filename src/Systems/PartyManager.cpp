// ============================================================
// File: PartyManager.cpp
// Responsibility: Implement persistent party stats + equipment slots.
// ============================================================
#include "PartyManager.h"
#include "Inventory.h"
#include "../Battle/ItemRegistry.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"

PartyManager& PartyManager::Get()
{
    static PartyManager instance;
    return instance;
}

PartyManager::PartyManager()
{
    // Initialize base stats directly from JSON file on singleton spin-up
    if (!JsonLoader::LoadCharacterData("data/characters/verso.json", mVersoStats))
    {
        LOG("%s", "[PartyManager] WARNING: Failed to load Verso stat configuration! Defaulting memory struct.");
    }
}

// ------------------------------------------------------------
// FoldEquipmentBonuses (file-local helper)
//   Walks every slot in `equipped`, looks each id up in ItemRegistry,
//   and adds its bonus* fields to `out`.  Empty slot ids are skipped.
//
//   Splitting this out keeps GetEffectiveVersoStats and
//   PreviewEffectiveStats from duplicating the same loop.
// ------------------------------------------------------------
namespace
{
    void FoldEquipmentBonuses(BattlerStats& out,
                                const std::array<std::string, kEquipSlotCount>& equipped)
    {
        const ItemRegistry& reg = ItemRegistry::Get();
        for (const std::string& id : equipped)
        {
            if (id.empty()) continue;
            const ItemData* item = reg.Find(id);
            if (!item) continue;

            out.atk    += item->bonusAtk;
            out.def    += item->bonusDef;
            out.matk   += item->bonusMatk;
            out.mdef   += item->bonusMdef;
            out.spd    += item->bonusSpd;
            out.maxHp  += item->bonusMaxHp;
            out.maxMp  += item->bonusMaxMp;
        }

        // Floor every stat at 0 — equipment with negative bonuses
        // (e.g. "iron_sword bonusSpd: -1") must not push a final stat
        // below zero.  Use the same convention StatResolver does.
        if (out.atk   < 0) out.atk   = 0;
        if (out.def   < 0) out.def   = 0;
        if (out.matk  < 0) out.matk  = 0;
        if (out.mdef  < 0) out.mdef  = 0;
        if (out.spd   < 0) out.spd   = 0;
        if (out.maxHp < 1) out.maxHp = 1;   // never zero — would break IsAlive
        if (out.maxMp < 0) out.maxMp = 0;
    }
}

// ------------------------------------------------------------
// GetEffectiveVersoStats
//   BASE stats + every equipped item's bonuses, with hp/mp clamped
//   into the (potentially raised) maxHp/maxMp range.
//
//   The clamp is important: if the player equips a +5 maxHp item
//   while at 100/100 HP, they should jump to 105/105 — not stay at
//   100/105 with a hidden gap.  Conversely if they unequip and drop
//   maxHp by 5, current hp should clamp DOWN so it never exceeds max.
// ------------------------------------------------------------
BattlerStats PartyManager::GetEffectiveVersoStats() const
{
    BattlerStats result = mVersoStats;
    FoldEquipmentBonuses(result, mEquipped);

    // Clamp current hp/mp to the new maximums (could go either way
    // when equipping/unequipping max-hp gear).
    if (result.hp > result.maxHp) result.hp = result.maxHp;
    if (result.mp > result.maxMp) result.mp = result.maxMp;
    if (result.hp < 0)            result.hp = 0;
    if (result.mp < 0)            result.mp = 0;

    return result;
}

// ------------------------------------------------------------
// PreviewEffectiveStats
//   Same fold, but with one slot temporarily replaced by `itemId`
//   (or cleared, if itemId is empty).  Used by inventory UI to draw
//   "ATK 25 → 37 (+12)" hover preview rows.
// ------------------------------------------------------------
BattlerStats PartyManager::PreviewEffectiveStats(EquipSlot slot,
                                                   const std::string& itemId) const
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0)
    {
        // EquipSlot::None — nothing to preview, return current effective.
        return GetEffectiveVersoStats();
    }

    // Copy the slot array, mutate the one entry, fold from base.
    std::array<std::string, kEquipSlotCount> hypothetical = mEquipped;
    hypothetical[slotIdx] = itemId;

    BattlerStats result = mVersoStats;
    FoldEquipmentBonuses(result, hypothetical);

    if (result.hp > result.maxHp) result.hp = result.maxHp;
    if (result.mp > result.maxMp) result.mp = result.maxMp;
    if (result.hp < 0)            result.hp = 0;
    if (result.mp < 0)            result.mp = 0;

    return result;
}

// ------------------------------------------------------------
// SetVersoStats
//   Persist ONLY the resource fields (hp/mp).  Equipment-derived
//   atk/def/maxHp/etc are intentionally discarded — they live on
//   the equipment slots, not on the saved stats.
//
//   Rage resets to 0 by design (per-battle resource).
// ------------------------------------------------------------
void PartyManager::SetVersoStats(const BattlerStats& stats)
{
    // The input may be the EFFECTIVE stats from BattleManager (base
    // + equipment).  Saving them whole would corrupt mVersoStats with
    // equipment bonuses double-counted on the next battle.
    //
    // Take only what's actually a "current resource".
    mVersoStats.hp   = stats.hp;
    mVersoStats.mp   = stats.mp;
    mVersoStats.rage = 0;

    // Clamp hp/mp into the BASE max range (the effective max may be
    // larger but we don't store that in mVersoStats).
    if (mVersoStats.hp > mVersoStats.maxHp) mVersoStats.hp = mVersoStats.maxHp;
    if (mVersoStats.mp > mVersoStats.maxMp) mVersoStats.mp = mVersoStats.maxMp;
    if (mVersoStats.hp < 0) mVersoStats.hp = 0;
    if (mVersoStats.mp < 0) mVersoStats.mp = 0;
}

// ------------------------------------------------------------
// RestoreFullHP
//   Used after game over / rest sites.  Restores to BASE max — the
//   player's effective max (with equipment) may be slightly higher,
//   but the base store is what we save here.  GetEffectiveVersoStats
//   will then re-clamp at draw time and the player sees full HP
//   anyway because base hp == base maxHp.
// ------------------------------------------------------------
void PartyManager::RestoreFullHP()
{
    mVersoStats.hp   = mVersoStats.maxHp;
    mVersoStats.mp   = mVersoStats.maxMp;
    mVersoStats.rage = 0;
}

std::string PartyManager::GetEquipped(EquipSlot slot) const
{
    const int idx = SlotIndex(slot);
    if (idx < 0) return {};
    return mEquipped[idx];
}

// ------------------------------------------------------------
// Equip
//   Validation order:
//     1. Item id resolves in ItemRegistry
//     2. Item's equipSlot matches the requested slot
//     3. Inventory has at least one of the item
//
//   Slot exchange order:
//     a. Save current slot id (may be empty)
//     b. Inventory.Add(currentId, 1)   if non-empty
//     c. Inventory.Remove(itemId, 1)
//     d. mEquipped[slotIdx] = itemId
//
//   This sequence guarantees the item count is conserved across the
//   transition: if step (c) somehow fails (it can't, we just checked
//   the count, but defensive programming is cheap), the worst case is
//   we leave the previous item in inventory AND equipped.  The opposite
//   order would be worse — we could destroy the new item without
//   actually equipping it.
// ------------------------------------------------------------
bool PartyManager::Equip(EquipSlot slot, const std::string& itemId)
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0)
    {
        LOG("[PartyManager] Equip: invalid slot enum.");
        return false;
    }

    const ItemData* item = ItemRegistry::Get().Find(itemId);
    if (!item)
    {
        LOG("[PartyManager] Equip: unknown item id '%s'.", itemId.c_str());
        return false;
    }

    if (item->equipSlot != slot)
    {
        LOG("[PartyManager] Equip: item '%s' goes in slot %s, not %s.",
            itemId.c_str(),
            SlotLabel(item->equipSlot),
            SlotLabel(slot));
        return false;
    }

    if (Inventory::Get().GetCount(itemId) <= 0)
    {
        LOG("[PartyManager] Equip: '%s' not in inventory.", itemId.c_str());
        return false;
    }

    // -- Slot exchange --
    const std::string previousId = mEquipped[slotIdx];
    if (!previousId.empty())
    {
        // Return the displaced item to inventory.
        Inventory::Get().Add(previousId, 1);
    }

    Inventory::Get().Remove(itemId, 1);
    mEquipped[slotIdx] = itemId;

    LOG("[PartyManager] Equipped '%s' to %s%s.",
        itemId.c_str(),
        SlotLabel(slot),
        previousId.empty() ? "" : (std::string(" (replaced ") + previousId + ")").c_str());
    return true;
}

// ------------------------------------------------------------
// Unequip
//   Empty slot is a no-op.  Returns the item to inventory.
// ------------------------------------------------------------
void PartyManager::Unequip(EquipSlot slot)
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return;

    const std::string id = mEquipped[slotIdx];
    if (id.empty()) return;

    Inventory::Get().Add(id, 1);
    mEquipped[slotIdx].clear();
    LOG("[PartyManager] Unequipped '%s' from %s.", id.c_str(), SlotLabel(slot));
}
