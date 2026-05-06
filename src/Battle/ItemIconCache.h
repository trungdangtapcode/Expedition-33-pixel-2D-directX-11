// ============================================================
// File: ItemIconCache.h
// Responsibility: Lazy, deduplicating cache of item icon textures
//                 keyed by ItemData::iconPath.
//
// Why a separate cache (not a field on ItemData):
//   ItemData is a pure value-type description of one item.  Adding
//   a ComPtr<ID3D11ShaderResourceView> to it would couple the data
//   layer to D3D11 and break the "ItemData is JSON" invariant.
//   The cache lives at a higher layer where D3D dependencies are
//   appropriate.
//
// Why dedup by path (not by item id):
//   Two ItemData entries (e.g. "potion_small_red" and "potion_small_blue"
//   reskin) MAY share the same iconPath.  Keying the cache by path
//   means one VRAM texture instead of two for the same pixels.
//
// Lazy-load semantics:
//   GetIcon() loads on first request.  If the file is missing or
//   WIC fails to decode, the cache stores an empty ComPtr in the
//   map -- subsequent calls hit the cached null without re-trying
//   the filesystem.  This is the "tried-and-failed" sentinel state.
//
// Lifetime:
//   - Initialize() must be called once per battle / inventory open
//     (idempotent: ignores subsequent device/context arguments).
//   - Cached SRVs hold a COM ref on the device, so the device stays
//     alive as long as the cache does.
//   - Shutdown() releases every cached SRV.  Safe to omit -- COM
//     reference counting handles cleanup at static destruction.
//
// Common mistakes:
//   1. Calling GetIcon() before Initialize() -> returns nullptr forever.
//   2. Storing the returned pointer past a Shutdown() -> dangling SRV.
// ============================================================
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>

struct ItemData;

class ItemIconCache
{
public:
    // Meyers' Singleton -- thread-safe construction in C++11+.
    static ItemIconCache& Get();

    // ------------------------------------------------------------
    // Initialize
    // Purpose:
    //   Stash the device + context that GetIcon() will use to upload
    //   textures via WICTextureLoader.
    // Why idempotent:
    //   Both BattleState and InventoryState call this in OnEnter().
    //   The cache must survive across both -- the FIRST call wins
    //   so the cached SRVs match whichever device/context loaded them.
    // ------------------------------------------------------------
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // ------------------------------------------------------------
    // GetIcon
    // Purpose:
    //   Return the SRV for item->iconPath, lazy-loading on first call.
    // Returns:
    //   Non-null SRV     - draw the real icon.
    //   nullptr          - iconPath empty, file missing, or WIC failed.
    //                      Caller MUST fall back to a placeholder.
    // ------------------------------------------------------------
    ID3D11ShaderResourceView* GetIcon(const ItemData* item);

    // Release every cached SRV.  Optional -- COM cleanup happens
    // automatically at static destruction.  Provided so a long-running
    // app can free icon VRAM on demand (e.g. between major scenes).
    void Shutdown();

private:
    ItemIconCache() = default;
    ItemIconCache(const ItemIconCache&)            = delete;
    ItemIconCache& operator=(const ItemIconCache&) = delete;

    // Stored handles (non-owning).  ItemIconCache does NOT release
    // these -- D3DContext owns the device + context lifetime.
    ID3D11Device*        mDevice  = nullptr;
    ID3D11DeviceContext* mContext = nullptr;

    // Keyed by iconPath so two items pointing at the same PNG share
    // a single texture upload.  Empty ComPtr in the map is the
    // "tried-and-failed" sentinel -- we cache the null result so we
    // do not hammer the filesystem on every frame.
    std::unordered_map<std::string,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> mCache;
};
