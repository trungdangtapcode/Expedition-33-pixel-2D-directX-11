// ============================================================
// File: Inventory.cpp
// Responsibility: Implement count-per-item-id persistent storage and
//                 seed a starter bundle on first construction.
// ============================================================
#include "Inventory.h"
#include "../Utils/Log.h"

// ------------------------------------------------------------
// Constructor: seed a starter inventory so the first battle has
// something usable.  Values here are INTENTIONAL tuning — they
// are the only literal item counts in code.  Real saves overwrite
// these numbers on load.
//
// Ids must match ItemData::id in data/items/*.json.  A typo is
// silently tolerated (GetCount returns 0 for the misspelled key),
// but the item won't appear in battle.
// ------------------------------------------------------------
Inventory::Inventory()
{
    // Healing staples — the "bread and butter" of early combat.
    Add("potion_small",   5);
    Add("potion_medium",  3);
    Add("potion_large",   1);

    // MP restoration for magic users.
    Add("ether_small",    3);
    Add("ether_medium",   1);

    // Full-party tier reagent.
    Add("elixir",         1);

    // Revives — one is enough to teach the mechanic without trivializing fights.
    Add("phoenix_down",   2);

    // Utility — instantly top up rage so RageSkill is always a fallback.
    Add("rage_gem",       2);

    // Cleansers — minor stock so debuffs never feel permanent.
    Add("antidote",       3);

    // Stat boosters for tactical play.
    Add("power_tonic",    2);
    Add("iron_draft",     2);
    Add("swift_feather",  1);

    // Offensive consumables — a limited panic button.
    Add("bomb",           3);
    Add("fire_bomb",      1);
    Add("ice_shard",      2);

    // ---- Equipment starter bundle ----
    // Two weapons so the player can immediately swap between them and
    // see stat changes in the inventory preview.  One of each armor
    // slot so every slot has at least one valid item to equip.
    Add("short_sword",    1);
    Add("iron_sword",     1);
    Add("village_clothes",1);
    Add("leather_cap",    1);
    Add("copper_ring",    1);

    LOG("[Inventory] Seeded starter bundle — %zu distinct items.", mOrder.size());
}

int Inventory::GetCount(const std::string& id) const
{
    const auto it = mCounts.find(id);
    return (it == mCounts.end()) ? 0 : it->second;
}

// ------------------------------------------------------------
// Add
//   Appends new ids to mOrder so OwnedIds() preserves insertion order
//   the first time the player ever picks up an item.  Subsequent Adds
//   of the same id just increment the count.
// ------------------------------------------------------------
void Inventory::Add(const std::string& id, int count)
{
    if (count <= 0) return;

    auto it = mCounts.find(id);
    if (it == mCounts.end())
    {
        mCounts.emplace(id, count);
        mOrder.push_back(id);
    }
    else
    {
        it->second += count;
    }
}

// ------------------------------------------------------------
// Remove
//   Clamped at 0 so GetCount never returns negative values — the
//   battle system may call Remove speculatively during action
//   resolution and we want that to degrade gracefully.
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// OwnedIds
//   Filter mOrder by count > 0 so the battle UI never shows
//   "Potion x0" entries after the last one is consumed.
//   mOrder keeps zero-count entries on purpose — pickup order is
//   stable for the lifetime of the session.
// ------------------------------------------------------------
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
