// ============================================================
// File: ItemIconCache.cpp
// Responsibility: Implement lazy + deduplicating PNG upload for
//                 item icons declared in data/items/*.json.
// ============================================================
#include "ItemIconCache.h"
#include "ItemData.h"
#include "../Utils/Log.h"

#include <WICTextureLoader.h>
#include <filesystem>

namespace fs = std::filesystem;

ItemIconCache& ItemIconCache::Get()
{
    static ItemIconCache instance;
    return instance;
}

// ------------------------------------------------------------
// Initialize
//   Stores the device + context the cache will use for WIC uploads.
//   Idempotent: only the first non-null assignment wins, so the
//   second caller (e.g. inventory after battle) reuses the same
//   device that loaded the cached textures.  This avoids the
//   pathological case where an SRV created by one device is used
//   in another device's context -- which is undefined behaviour
//   in D3D11.
// ------------------------------------------------------------
void ItemIconCache::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (mDevice == nullptr)
    {
        mDevice  = device;
        mContext = context;
    }
}

// ------------------------------------------------------------
// GetIcon
//   First call for a given path performs the WIC upload.  Empty
//   path, missing file, and WIC failure all collapse to the same
//   "cached nullptr" state so callers get the same fallback signal
//   for every degenerate input without retry overhead.
// Common mistakes:
//   1. Forgetting to handle nullptr -> player sees nothing where
//      the icon should be.  Every caller in this codebase falls
//      back to the IconTintFor()/IconTint() colored quad helper.
//   2. Caching the returned pointer across a Shutdown() -> the SRV
//      is released and the pointer dangles.
// ------------------------------------------------------------
ID3D11ShaderResourceView* ItemIconCache::GetIcon(const ItemData* item)
{
    if (!item || item->iconPath.empty() || mDevice == nullptr)
        return nullptr;

    // Cache hit short-circuits both "loaded" and "tried-and-failed".
    auto it = mCache.find(item->iconPath);
    if (it != mCache.end())
        return it->second.Get();

    // Resolve the path.  Match the dual-cwd convention in
    // ItemRegistry::EnsureLoaded so running from bin/ also works.
    fs::path resolved = item->iconPath;
    if (!fs::exists(resolved))
    {
        fs::path alt = fs::path("..") / resolved;
        if (fs::exists(alt)) resolved = alt;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (fs::exists(resolved))
    {
        // wstring conversion: iconPath is ASCII (paths under assets/),
        // so a byte-by-byte widen is safe.  Avoids MultiByteToWideChar.
        const std::string narrow = resolved.string();
        std::wstring wide(narrow.begin(), narrow.end());

        // CreateWICTextureFromFile is sufficient for static icons --
        // we do not need the *Ex variant's flags here because we use
        // default usage (D3D11_USAGE_DEFAULT, SHADER_RESOURCE bind).
        HRESULT hr = DirectX::CreateWICTextureFromFile(
            mDevice, mContext, wide.c_str(), nullptr, srv.GetAddressOf());

        if (FAILED(hr))
        {
            LOG("[ItemIconCache] WIC load failed (hr=0x%08lX) for '%s' (item '%s')",
                static_cast<unsigned long>(hr),
                narrow.c_str(),
                item->id.c_str());
            srv.Reset();   // explicit -- empty ComPtr = tried-and-failed
        }
        else
        {
            LOG("[ItemIconCache] Loaded icon for '%s' from '%s'",
                item->id.c_str(), narrow.c_str());
        }
    }
    else
    {
        // ItemRegistry already logs a one-time "icon missing" at load
        // time, so we do not re-log here -- only cache the null and
        // let the caller render the placeholder.
    }

    ID3D11ShaderResourceView* raw = srv.Get();
    mCache.emplace(item->iconPath, std::move(srv));
    return raw;
}

void ItemIconCache::Shutdown()
{
    mCache.clear();
    mDevice  = nullptr;
    mContext = nullptr;
}
