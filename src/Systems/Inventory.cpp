// ============================================================
// File: Inventory.cpp
// Responsibility: Implement persistent item counts, starter seeding,
//                 and save/load snapshots.
// ============================================================
#include "Inventory.h"
#include "../Utils/Log.h"

Inventory::Inventory()
{
    ResetToDefaults();
}

void Inventory::ResetToDefaults()
{
    mCounts.clear();
    mOrder.clear();

    // Healing staples for early combat.
    Add("potion_small", 5);
    Add("potion_medium", 3);
    Add("potion_large", 1);

    // MP restoration for magic users.
    Add("ether_small", 3);
    Add("ether_medium", 1);

    // Full-party tier reagent.
    Add("elixir", 1);

    // Revives teach recovery without trivializing early encounters.
    Add("phoenix_down", 2);

    // Rage support gives the player a limited emergency burst option.
    Add("rage_gem", 2);

    // Cleansers prevent early debuffs from feeling permanent.
    Add("antidote", 3);

    // Stat boosters support tactical item use.
    Add("power_tonic", 2);
    Add("iron_draft", 2);
    Add("swift_feather", 1);

    // Offensive consumables give the player a limited panic button.
    Add("bomb", 3);
    Add("fire_bomb", 1);
    Add("ice_shard", 2);

    // Equipment starter bundle for immediate preview and swapping tests.
    Add("short_sword", 1);
    Add("iron_sword", 1);
    Add("village_clothes", 1);
    Add("leather_cap", 1);
    Add("copper_ring", 1);

    LOG("[Inventory] Starter bundle ready: %zu distinct items.", mOrder.size());
}

int Inventory::GetCount(const std::string& id) const
{
    const auto it = mCounts.find(id);
    return (it == mCounts.end()) ? 0 : it->second;
}

void Inventory::Add(const std::string& id, int count)
{
    if (id.empty() || count <= 0) return;

    auto it = mCounts.find(id);
    if (it == mCounts.end())
    {
        mCounts.emplace(id, count);
        mOrder.push_back(id);
        return;
    }

    it->second += count;
}

int Inventory::Remove(const std::string& id, int count)
{
    if (count <= 0) return 0;

    const auto it = mCounts.find(id);
    if (it == mCounts.end()) return 0;

    const int removed = (it->second >= count) ? count : it->second;
    it->second -= removed;
    if (it->second < 0) it->second = 0;
    return removed;
}

std::vector<std::string> Inventory::OwnedIds() const
{
    std::vector<std::string> out;
    out.reserve(mOrder.size());

    for (const std::string& id : mOrder)
    {
        if (GetCount(id) > 0) out.push_back(id);
    }
    return out;
}

std::vector<InventoryEntry> Inventory::CaptureEntries() const
{
    std::vector<InventoryEntry> entries;
    entries.reserve(mOrder.size());

    for (const std::string& id : mOrder)
    {
        const int count = GetCount(id);
        if (count <= 0) continue;

        InventoryEntry entry{};
        entry.id = id;
        entry.count = count;
        entries.push_back(entry);
    }
    return entries;
}

void Inventory::ReplaceAll(const std::vector<InventoryEntry>& entries)
{
    mCounts.clear();
    mOrder.clear();

    for (const InventoryEntry& entry : entries)
    {
        Add(entry.id, entry.count);
    }
}
