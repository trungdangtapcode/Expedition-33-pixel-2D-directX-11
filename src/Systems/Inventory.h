// ============================================================
// File: Inventory.h
// Responsibility: Persistent count-per-item-id storage for the player
//                 party.  Lives for the whole process.
//
// Design — Meyers' Singleton:
//   Matches PartyManager.  One Inventory exists for the duration of
//   the program; BattleState, overworld, menus, and save/load all
//   read and mutate the same instance.
//
// Seeding:
//   On first Get() the default constructor seeds a small starter
//   bundle so a new game has testable inventory without a save file.
//   Real saves later will call LoadFromSave() / SaveToBlob().
//
// Stack limits:
//   None enforced at MVP — count is a signed int so underflow is
//   a programming error, not a normal state.  Add kMaxStackSize
//   later if needed.
//
// Contract:
//   - GetCount(id) returns 0 for unknown ids (never throws).
//   - Remove() is a no-op on ids with count 0 — callers check first.
//   - The inventory holds only item IDs; it never touches ItemRegistry.
// ============================================================
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class Inventory
{
public:
    static Inventory& Get()
    {
        static Inventory instance;
        return instance;
    }

    // ------------------------------------------------------------
    // GetCount: how many of item `id` the player owns.
    // Returns 0 for ids the player has never held.
    // ------------------------------------------------------------
    int GetCount(const std::string& id) const;

    // Convenience predicate for UI gating.
    bool Has(const std::string& id) const { return GetCount(id) > 0; }

    // ------------------------------------------------------------
    // Add: increment by `count`.  count <= 0 is a no-op (never decrements).
    // ------------------------------------------------------------
    void Add(const std::string& id, int count = 1);

    // ------------------------------------------------------------
    // Remove: decrement by `count`, clamped at 0.  count <= 0 is a no-op.
    // Returns the actual number removed (may be less than requested when
    // the stack is smaller than `count`).
    // ------------------------------------------------------------
    int Remove(const std::string& id, int count = 1);

    // ------------------------------------------------------------
    // OwnedIds: every id with count > 0, in insertion order of first add.
    // Used by battle UI to list what the player can actually use right now.
    // ------------------------------------------------------------
    std::vector<std::string> OwnedIds() const;

private:
    Inventory();                                   // seeds defaults

    // Non-copyable / non-movable — singleton must not be duplicated.
    Inventory(const Inventory&)            = delete;
    Inventory& operator=(const Inventory&) = delete;

    // Per-id counts.  Missing key == 0.
    std::unordered_map<std::string, int> mCounts;

    // Insertion-ordered id list so OwnedIds() output is deterministic.
    // Appended to the first time an id is Add()'d; never removed, so UIs
    // that want to hide zero-count items must filter OwnedIds().
    std::vector<std::string> mOrder;
};
