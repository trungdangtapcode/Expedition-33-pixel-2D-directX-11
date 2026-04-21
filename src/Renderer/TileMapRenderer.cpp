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

    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device, context,
        mData.texturePath.c_str(),
        0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        DirectX::WIC_LOADER_IGNORE_SRGB,
        nullptr, mTextureSRV.GetAddressOf()
    );

    if (FAILED(hr)) {
        LOG("[TileMapRenderer] ERROR — Failed to load tile map texture '%ls' (HRESULT 0x%08X).", mData.texturePath.c_str(), hr);
        return false;
    }

    // Determine how many tile columns are in the tilemap texture
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    mTextureSRV->GetResource(resource.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
    if (SUCCEEDED(resource.As(&tex2D))) {
        D3D11_TEXTURE2D_DESC desc;
        tex2D->GetDesc(&desc);
        mTilesetCols = desc.Width / mData.tileWidth;
    } else {
        mTilesetCols = 1; 
    }

    mStates = std::make_unique<DirectX::CommonStates>(device);
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);

    LOG("[TileMapRenderer] Initialized '%s'. Map size: %dx%d, Tileset cols: %d.", 
        jsonPath.c_str(), mData.cols, mData.rows, mTilesetCols);
        
    return true;
}

void TileMapRenderer::Render(ID3D11DeviceContext* context, const Camera2D& camera)
{
    if (!mSpriteBatch || !mTextureSRV || mData.tiles.empty() || mTilesetCols <= 0) return;

    DirectX::XMMATRIX view = camera.GetViewMatrix();

    // Use PointClamp to avoid bleeding edges in tilemaps
    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->AlphaBlend(),
        mStates->PointClamp(),
        nullptr, nullptr, nullptr,
        view
    );

    // Draw the map centered roughly around the origin, or top-left at (-width/2, -height/2)
    // We'll align the map so that (0,0) is in the middle of the tile map
    float startX = -((mData.cols * mData.tileWidth) / 2.0f);
    float startY = -((mData.rows * mData.tileHeight) / 2.0f);

    for (int y = 0; y < mData.rows; ++y) {
        for (int x = 0; x < mData.cols; ++x) {
            int gid = mData.tiles[y * mData.cols + x];
            if (gid == 0) continue; // Tiled 0 is empty

            int localId = gid - mData.firstGid;
            if (localId < 0) continue;

            int srcX = (localId % mTilesetCols) * mData.tileWidth;
            int srcY = (localId / mTilesetCols) * mData.tileHeight;

            RECT srcRect = { srcX, srcY, srcX + mData.tileWidth, srcY + mData.tileHeight };
            
            DirectX::XMFLOAT2 worldPos(
                startX + x * mData.tileWidth,
                startY + y * mData.tileHeight
            );

            // SpriteBatch::Draw expects a float2
            mSpriteBatch->Draw(mTextureSRV.Get(), worldPos, &srcRect, DirectX::Colors::White);
        }
    }

    mSpriteBatch->End();
}

void TileMapRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mTextureSRV.Reset();
    mData.tiles.clear();
}
