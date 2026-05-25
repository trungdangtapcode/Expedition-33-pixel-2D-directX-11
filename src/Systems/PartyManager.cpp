// ============================================================
// File: PartyManager.cpp
// Responsibility: Implement persistent party stats, equipment slots,
//                 level progression, and save/load snapshots.
// ============================================================
#define NOMINMAX
#include "PartyManager.h"
#include "Inventory.h"
#include "../Battle/ItemRegistry.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>

namespace
{
    // ------------------------------------------------------------
    // Function: ClampCurrentResources
    // Purpose:
    //   Keep volatile resources inside valid bounds after save/load or battle.
    // Why:
    //   Corrupt saves, equipment changes, or combat actions must never leave
    //   current HP above max HP or below zero.
    // ------------------------------------------------------------
    void ClampCurrentResources(BattlerStats& stats)
    {
        if (stats.maxHp < 1) stats.maxHp = 1;
        if (stats.maxMp < 0) stats.maxMp = 0;
        if (stats.maxRage < 0) stats.maxRage = 0;

        if (stats.hp < 0) stats.hp = 0;
        if (stats.hp > stats.maxHp) stats.hp = stats.maxHp;

        if (stats.mp < 0) stats.mp = 0;
        if (stats.mp > stats.maxMp) stats.mp = stats.maxMp;

        if (stats.rage < 0) stats.rage = 0;
        if (stats.maxRage == 0) stats.rage = 0;
        if (stats.rage > stats.maxRage) stats.rage = stats.maxRage;
    }

    // ------------------------------------------------------------
    // Function: FoldEquipmentBonuses
    // Purpose:
    //   Add every equipped item's flat stat bonus into the output stats.
    // Why:
    //   Equipment is durable and unconditional, so it belongs in a compact
    //   fold rather than the temporary combat stat-modifier pipeline.
    // ------------------------------------------------------------
    void FoldEquipmentBonuses(BattlerStats& out,
                              const std::array<std::string, kEquipSlotCount>& equipped)
    {
        const ItemRegistry& reg = ItemRegistry::Get();
        for (const std::string& id : equipped)
        {
            if (id.empty()) continue;

            const ItemData* item = reg.Find(id);
            if (!item) continue;

            out.atk   += item->bonusAtk;
            out.def   += item->bonusDef;
            out.matk  += item->bonusMatk;
            out.mdef  += item->bonusMdef;
            out.spd   += item->bonusSpd;
            out.maxHp += item->bonusMaxHp;
            out.maxMp += item->bonusMaxMp;
        }

        if (out.atk < 0) out.atk = 0;
        if (out.def < 0) out.def = 0;
        if (out.matk < 0) out.matk = 0;
        if (out.mdef < 0) out.mdef = 0;
        if (out.spd < 0) out.spd = 0;
        if (out.maxHp < 1) out.maxHp = 1;
        if (out.maxMp < 0) out.maxMp = 0;
    }

    // ------------------------------------------------------------
    // Function: BuildMember
    // Purpose:
    //   Create one PartyMember from stable metadata and a character JSON file.
    // Why:
    //   ResetToDefaults and the constructor need the same data-driven member
    //   bootstrapping without duplicating load logic.
    // ------------------------------------------------------------
    PartyMember BuildMember(const std::string& id,
                            const std::string& name,
                            const std::wstring& animationPath,
                            const std::string& animJsonPath,
                            const std::wstring& hpFramePath,
                            const std::wstring& turnViewPath,
                            const std::string& dataPath)
    {
        PartyMember member{};
        member.id = id;
        member.name = name;
        member.animationPath = animationPath;
        member.animJsonPath = animJsonPath;
        member.hpFramePath = hpFramePath;
        member.turnViewPath = turnViewPath;

        if (!JsonLoader::LoadCharacterData(dataPath, member.baseStats))
        {
            LOG("[PartyManager] WARNING: Failed to load character data '%s'.", dataPath.c_str());
        }
        member.skillPaths = JsonLoader::LoadStringArrayFromFile(dataPath, "skillPaths");
        ClampCurrentResources(member.baseStats);
        return member;
    }
}

PartyManager& PartyManager::Get()
{
    static PartyManager instance;
    return instance;
}

PartyManager::PartyManager()
{
    ResetToDefaults();
}

void PartyManager::ResetToDefaults()
{
    mActiveParty.clear();
    mActiveParty.push_back(BuildMember(
        "verso",
        "Verso",
        L"assets/animations/verso.png",
        "assets/animations/verso.json",
        L"assets/UI/UI_verso_hp.png",
        L"assets/UI/turn-view-verso.png",
        "data/characters/verso.json"));

    mActiveParty.push_back(BuildMember(
        "maelle",
        "Maelle",
        L"assets/animations/maelle.png",
        "assets/animations/maelle.json",
        L"assets/UI/UI_maelle_hp.png",
        L"assets/UI/turn-view-maelle.png",
        "data/characters/maelle.json"));
}

BattlerStats PartyManager::GetEffectiveStats(size_t index) const
{
    BattlerStats stats = mActiveParty[index].baseStats;
    FoldEquipmentBonuses(stats, mActiveParty[index].equipped);
    ClampCurrentResources(stats);
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
    ClampCurrentResources(stats);
    return stats;
}

void PartyManager::SetMemberStats(size_t index, const BattlerStats& stats)
{
    if (index >= mActiveParty.size()) return;

    BattlerStats& stored = mActiveParty[index].baseStats;
    stored.hp = std::min(std::max(stats.hp, 0), stored.maxHp);
    stored.mp = std::min(std::max(stats.mp, 0), stored.maxMp);

    // Rage currently resets after battle by design so stocked rage cannot
    // trivialize the next encounter.
    stored.rage = 0;
}

void PartyManager::RestoreFullHP()
{
    for (auto& member : mActiveParty)
    {
        member.baseStats.hp = member.baseStats.maxHp;
        member.baseStats.mp = member.baseStats.maxMp;
        member.baseStats.rage = 0;
    }
}

void PartyManager::AddExp(int amount)
{
    if (amount <= 0) return;

    for (PartyMember& member : mActiveParty)
    {
        member.baseStats.exp += amount;

        while (member.baseStats.exp >= GetExpCurve(member.baseStats.level))
        {
            member.baseStats.exp -= GetExpCurve(member.baseStats.level);
            ++member.baseStats.level;

            member.baseStats.maxHp += member.baseStats.growth.maxHp;
            member.baseStats.maxMp += member.baseStats.growth.maxMp;
            member.baseStats.atk += member.baseStats.growth.atk;
            member.baseStats.def += member.baseStats.growth.def;
            member.baseStats.matk += member.baseStats.growth.matk;
            member.baseStats.mdef += member.baseStats.growth.mdef;
            member.baseStats.spd += member.baseStats.growth.spd;

            member.baseStats.hp = member.baseStats.maxHp;
            member.baseStats.mp = member.baseStats.maxMp;

            LOG("[PartyManager] LEVEL UP: %s reached level %d. Max HP: %d. ATK: %d.",
                member.name.c_str(),
                member.baseStats.level,
                member.baseStats.maxHp,
                member.baseStats.atk);
        }
    }
}

std::vector<PartyMemberProgress> PartyManager::CaptureProgress() const
{
    std::vector<PartyMemberProgress> progress;
    progress.reserve(mActiveParty.size());

    for (const PartyMember& member : mActiveParty)
    {
        PartyMemberProgress entry{};
        entry.id = member.id;
        entry.baseStats = member.baseStats;
        entry.equipped = member.equipped;
        progress.push_back(entry);
    }
    return progress;
}

void PartyManager::ApplyProgress(const std::vector<PartyMemberProgress>& progress)
{
    for (const PartyMemberProgress& entry : progress)
    {
        auto it = std::find_if(mActiveParty.begin(), mActiveParty.end(),
            [&entry](const PartyMember& member)
            {
                return member.id == entry.id;
            });

        if (it == mActiveParty.end())
        {
            LOG("[PartyManager] WARNING: Save referenced unknown party member '%s'.", entry.id.c_str());
            continue;
        }

        it->baseStats = entry.baseStats;
        ClampCurrentResources(it->baseStats);
        it->equipped = entry.equipped;
    }
}

std::string PartyManager::GetEquippedItem(size_t index, EquipSlot slot) const
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0 || index >= mActiveParty.size()) return "";
    return mActiveParty[index].equipped[slotIdx];
}

bool PartyManager::EquipItem(size_t index, EquipSlot slot, const std::string& itemId)
{
    if (index >= mActiveParty.size()) return false;

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
    if (index >= mActiveParty.size()) return false;

    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return false;

    const std::string id = mActiveParty[index].equipped[slotIdx];
    if (id.empty()) return false;

    Inventory::Get().Add(id, 1);
    mActiveParty[index].equipped[slotIdx].clear();
    return true;
}
