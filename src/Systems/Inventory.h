// ============================================================
// File: Inventory.h
// Responsibility: Persist count-per-item-id storage for the player party.
//
// Design:
//   Inventory is a Meyers singleton because item counts must survive
//   battles, menus, overworld traversal, and save/load operations.
//
// Save/load:
//   SaveManager reads and writes InventoryEntry snapshots. Inventory owns
//   ordering and count normalization so save files cannot bypass invariants.
//
// Seeding:
//   ResetToDefaults seeds a small starter bundle for New Game and tests.
//   Loading a checkpoint replaces that bundle with saved counts.
//
// Contract:
//   - GetCount(id) returns 0 for unknown ids.
//   - Add(id, count <= 0) is a no-op.
//   - Remove(id, count) clamps at zero and returns the actual removed count.
//   - Inventory stores item ids only; ItemRegistry owns item metadata.
// ============================================================
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct InventoryEntry
{
    std::string id;
    int count = 0;
};

class Inventory
{
public:
    static Inventory& Get()
    {
        static Inventory instance;
        return instance;
    }

    int GetCount(const std::string& id) const;
    bool Has(const std::string& id) const { return GetCount(id) > 0; }

    void Add(const std::string& id, int count = 1);
    int Remove(const std::string& id, int count = 1);
    std::vector<std::string> OwnedIds() const;

    // ------------------------------------------------------------
    // Function: ResetToDefaults
    // Purpose:
    //   Clear all counts and seed the New Game starter bundle.
    // Why:
    //   Starting a new game inside the same process must not retain items
    //   from a previous loaded checkpoint.
    // ------------------------------------------------------------
    void ResetToDefaults();

    // ------------------------------------------------------------
    // Function: CaptureEntries
    // Purpose:
    //   Return save-friendly item counts in deterministic menu order.
    // Why:
    //   SaveManager should serialize the durable bag state without knowing
    //   about the internal unordered_map plus order-vector pairing.
    // ------------------------------------------------------------
    std::vector<InventoryEntry> CaptureEntries() const;

    // ------------------------------------------------------------
    // Function: ReplaceAll
    // Purpose:
    //   Replace the current inventory with parsed save entries.
    // Why:
    //   Loading a checkpoint must overwrite the starter bundle instead of
    //   adding saved counts on top of it.
    // ------------------------------------------------------------
    void ReplaceAll(const std::vector<InventoryEntry>& entries);

private:
    Inventory();

    Inventory(const Inventory&)            = delete;
    Inventory& operator=(const Inventory&) = delete;

    std::unordered_map<std::string, int> mCounts;
    std::vector<std::string> mOrder;
};
