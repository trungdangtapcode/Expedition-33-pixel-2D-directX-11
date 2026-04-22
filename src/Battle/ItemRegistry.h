// ============================================================
// File: ItemRegistry.h
// Responsibility: Singleton cache of every ItemData discovered under
//                 data/items/*.json.  One place to find an item by id,
//                 iterate the full catalog, or warn about missing files.
//
// Design — lazy on first access:
//   EnsureLoaded() walks data/items/ exactly once.  Subsequent calls
//   short-circuit on mLoaded.  This defers disk I/O past the title
//   screen and keeps startup cheap.
//
// Asset warnings:
//   Every item declares an iconPath.  If the path does not exist on
//   disk, Load() logs a "icon missing" warning but still registers
//   the item — the menu renders a placeholder until an artist adds
//   the sprite.  The game never crashes on missing art.
//
// Why a singleton:
//   Many systems need read-only access (UI menu, inventory lookup,
//   battle manager) and nobody owns "the list of all items".
//   PartyManager and StatResolver use the same Meyers-singleton pattern.
//
// Thread-safety:
//   Not thread-safe.  The battle runs on the main thread; any future
//   background loading must introduce its own synchronization.
// ============================================================
#pragma once
#include "ItemData.h"
#include <string>
#include <vector>

class ItemRegistry
{
public:
    // ------------------------------------------------------------
    // Get: Meyers' Singleton accessor.  Thread-safe in C++11+ (magic statics).
    // ------------------------------------------------------------
    static ItemRegistry& Get()
    {
        static ItemRegistry instance;
        return instance;
    }

    // ------------------------------------------------------------
    // EnsureLoaded: idempotent lazy loader.  Walks data/items/*.json
    // and parses each file into an ItemData.  Safe to call repeatedly.
    // ------------------------------------------------------------
    void EnsureLoaded();

    // ------------------------------------------------------------
    // Find: look up one item by id.
    // Returns nullptr if id is not in the registry.
    // ------------------------------------------------------------
    const ItemData* Find(const std::string& id) const;

    // All known items, stable registration order (order of discovery
    // inside the directory iterator).  UI lists render from this.
    const std::vector<ItemData>& All() const { return mItems; }

private:
    ItemRegistry() = default;

    // Non-copyable / non-movable — singleton must not be duplicated.
    ItemRegistry(const ItemRegistry&)            = delete;
    ItemRegistry& operator=(const ItemRegistry&) = delete;

    // ------------------------------------------------------------
    // LoadFile: parse one .json into an ItemData and push onto mItems.
    // Returns false on parse error (file still counts as discovered).
    // ------------------------------------------------------------
    bool LoadFile(const std::string& path);

    std::vector<ItemData> mItems;
    bool                  mLoaded = false;
};
