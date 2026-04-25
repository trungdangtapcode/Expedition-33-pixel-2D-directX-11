// ============================================================
// File: PartyManager.cpp
// Responsibility: Implement persistent party stats + equipment slots.
// ============================================================
#define NOMINMAX
#include <algorithm>
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
    // Initialize base members!
    PartyMember verso{};
    verso.id = "verso";
    verso.name = "Verso";
    verso.animationPath = L"assets/animations/verso.png";
    verso.animJsonPath = "assets/animations/verso.json";
    verso.hpFramePath = L"assets/UI/UI_verso_hp.png";
    verso.turnViewPath = L"assets/UI/turn-view-verso.png";
    
    if (!JsonLoader::LoadCharacterData("data/characters/verso.json", verso.baseStats)) {
        LOG("[PartyManager] WARNING: Failed to load Verso stat config.");
    }
    mActiveParty.push_back(verso);

    // Maelle
    PartyMember maelle{};
    maelle.id = "maelle";
    maelle.name = "Maelle";
    maelle.animationPath = L"assets/animations/maelle.png";
    maelle.animJsonPath = "assets/animations/maelle.json";
    maelle.hpFramePath = L"assets/UI/UI_maelle_hp.png";
    maelle.turnViewPath = L"assets/UI/turn-view-maelle.png";

    if (!JsonLoader::LoadCharacterData("data/characters/maelle.json", maelle.baseStats)) {
        LOG("[PartyManager] WARNING: Failed to load Maelle stat config.");
    }
    mActiveParty.push_back(maelle);
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
BattlerStats PartyManager::GetEffectiveStats(size_t index) const
{
    BattlerStats stats = mActiveParty[index].baseStats;
    FoldEquipmentBonuses(stats, mActiveParty[index].equipped);

    if (stats.hp > stats.maxHp) stats.hp = stats.maxHp;
    if (stats.mp > stats.maxMp) stats.mp = stats.maxMp;
    if (stats.hp < 0) stats.hp = 0;
    if (stats.mp < 0) stats.mp = 0;

    return stats;
}

BattlerStats PartyManager::PreviewEffectiveStats(size_t index, EquipSlot slot, const std::string& itemId) const
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return GetEffectiveStats(index);

    BattlerStats stats = mActiveParty[index].baseStats;
    std::array<std::string, kEquipSlotCount> previewSlots = mActiveParty[index].equipped;
    previewSlots[slotIdx] = itemId;

    FoldEquipmentBonuses(stats, previewSlots);

    if (stats.hp > stats.maxHp) stats.hp = stats.maxHp;
    if (stats.mp > stats.maxMp) stats.mp = stats.maxMp;
    if (stats.hp < 0) stats.hp = 0;
    if (stats.mp < 0) stats.mp = 0;

    return stats;
}

void PartyManager::SetMemberStats(size_t index, const BattlerStats& stats)
{
    mActiveParty[index].baseStats.hp = std::min(stats.hp, mActiveParty[index].baseStats.maxHp);
    mActiveParty[index].baseStats.mp = std::min(stats.mp, mActiveParty[index].baseStats.maxMp);
    mActiveParty[index].baseStats.rage = 0; 
}

void PartyManager::RestoreFullHP()
{
    for (auto& member : mActiveParty) {
        member.baseStats.hp = member.baseStats.maxHp;
        member.baseStats.rage = 0;
    }
}

std::string PartyManager::GetEquippedItem(size_t index, EquipSlot slot) const
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return "";
    return mActiveParty[index].equipped[slotIdx];
}

bool PartyManager::EquipItem(size_t index, EquipSlot slot, const std::string& itemId)
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return false;

    const ItemData* item = ItemRegistry::Get().Find(itemId);
    if (!item || item->equipSlot != slot) return false;

    if (Inventory::Get().GetCount(itemId) <= 0) return false;

    const std::string previousId = mActiveParty[index].equipped[slotIdx];
    if (!previousId.empty())
    {
        Inventory::Get().Add(previousId, 1);
    }

    Inventory::Get().Remove(itemId, 1);
    mActiveParty[index].equipped[slotIdx] = itemId;
    return true;
}

bool PartyManager::UnequipItem(size_t index, EquipSlot slot)
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return false;

    const std::string id = mActiveParty[index].equipped[slotIdx];
    if (id.empty()) return false;

    Inventory::Get().Add(id, 1);
    mActiveParty[index].equipped[slotIdx].clear();
    return true;
}

