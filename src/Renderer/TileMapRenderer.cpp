// ============================================================
// File: TileMapRenderer.cpp
// Responsibility: Draw an optimized regular grid of background tiles
//                 using a single SpriteBatch call and a Camera2D transform.
// ============================================================
#include "TileMapRenderer.h"
#include "../Utils/Log.h"
#include <WICTextureLoader.h>

TileMapRenderer::TileMapRenderer() = default;

TileMapRenderer::~TileMapRenderer()
{
    Shutdown();
}

bool TileMapRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& jsonPath)
{
    if (!JsonLoader::LoadTileMapData(jsonPath, mData)) {
        LOG("[TileMapRenderer] Failed to load tile map json: %s", jsonPath.c_str());
        return false;
    }

    for (const auto& ts : mData.tilesets) {
        if (ts.texturePath.empty()) {
            LOG("[TileMapRenderer] ERROR: Tileset has no image path.");
            return false;
        }

        TextureInfo info;
        HRESULT hr = DirectX::CreateWICTextureFromFileEx(
            device, context,
            ts.texturePath.c_str(),
            0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
            DirectX::WIC_LOADER_IGNORE_SRGB,
            nullptr, info.srv.GetAddressOf()
        );

        if (FAILED(hr)) {
            LOG("[TileMapRenderer] ERROR — Failed to load tileset texture '%ls' (HRESULT 0x%08X).", ts.texturePath.c_str(), hr);
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        info.srv->GetResource(resource.GetAddressOf());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
        if (SUCCEEDED(resource.As(&tex2D))) {
            D3D11_TEXTURE2D_DESC desc;
            tex2D->GetDesc(&desc);
            const int tileWidth = (ts.tileWidth > 0) ? ts.tileWidth : mData.tileWidth;
            info.cols = (tileWidth > 0) ? static_cast<int>(desc.Width) / tileWidth : 1;
            if (info.cols <= 0) info.cols = 1;
        } else {
            info.cols = 1; 
        }
        
        mTextures.push_back(info);
    }

    mStates = std::make_unique<DirectX::CommonStates>(device);
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);

    LOG("[TileMapRenderer] Initialized '%s'. Map size: %dx%d, Layers: %zu, Tilesets: %zu.", 
        jsonPath.c_str(), mData.cols, mData.rows, mData.layers.size(), mData.tilesets.size());
        
    return true;
}

void TileMapRenderer::Render(ID3D11DeviceContext* context, const Camera2D& camera)
{
    if (!mSpriteBatch || mTextures.empty() || mData.layers.empty()) return;

    DirectX::XMMATRIX view = camera.GetViewMatrix();

    // Use PointClamp to avoid bleeding edges in tilemaps
    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->AlphaBlend(),
        mStates->PointClamp(),
        nullptr, nullptr, nullptr,
        view
    );

    // Keep the map centered on world origin so existing player, campfire,
    // and enemy spawn coordinates remain stable when the map grows.
    float startX = -((mData.cols * mData.tileWidth) / 2.0f);
    float startY = -((mData.rows * mData.tileHeight) / 2.0f);

    for (const auto& layer : mData.layers) {
        const int layerCols = (layer.cols > 0) ? layer.cols : mData.cols;
        const int layerRows = (layer.rows > 0) ? layer.rows : mData.rows;
        if (layerCols <= 0 || layerRows <= 0) continue;

        for (int y = 0; y < layerRows; ++y) {
            for (int x = 0; x < layerCols; ++x) {
                const size_t index = static_cast<size_t>(y) * static_cast<size_t>(layerCols)
                                   + static_cast<size_t>(x);
                if (index >= layer.tiles.size()) continue;

                int gid = layer.tiles[index];
                if (gid == 0) continue; // Tiled 0 is empty.

                // Clear Tiled flip flags from the top bits before resolving
                // the tileset.  The renderer does not draw flipped tiles yet,
                // but clearing flags prevents edited maps from sampling a
                // nonsense atlas location.
                gid = static_cast<int>(static_cast<unsigned int>(gid) & 0x1FFFFFFFu);

                // Find which tileset this GID belongs to.
                int tsIndex = 0;
                for (size_t i = 1; i < mData.tilesets.size(); ++i) {
                    if (gid >= mData.tilesets[i].firstGid) {
                        tsIndex = static_cast<int>(i);
                    }
                }
                
                const auto& ts = mData.tilesets[tsIndex];
                const auto& tex = mTextures[tsIndex];

                int localId = gid - ts.firstGid;
                if (localId < 0) continue;

                const int tileWidth = (ts.tileWidth > 0) ? ts.tileWidth : mData.tileWidth;
                const int tileHeight = (ts.tileHeight > 0) ? ts.tileHeight : mData.tileHeight;
                int srcX = (localId % tex.cols) * tileWidth;
                int srcY = (localId / tex.cols) * tileHeight;

                RECT srcRect = { srcX, srcY, srcX + tileWidth, srcY + tileHeight };
                
                DirectX::XMFLOAT2 worldPos(
                    startX + x * mData.tileWidth,
                    startY + y * mData.tileHeight
                );

                mSpriteBatch->Draw(tex.srv.Get(), worldPos, &srcRect, DirectX::Colors::White);
            }
        }
    }

    mSpriteBatch->End();
}
void TileMapRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mTextures.clear();
    mData.layers.clear();
    mData.colliders.clear();
}
