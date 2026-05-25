// ============================================================
// File: TileMapRenderer.cpp
// Responsibility: Render Tiled tile layers with camera culling and
//                 foreground/background pass separation.
// ============================================================
#include "TileMapRenderer.h"
#include "../Utils/Log.h"
#include <WICTextureLoader.h>
#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
    std::string ToLowerAscii(std::string text)
    {
        for (char& ch : text)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return text;
    }

    bool ContainsToken(const std::string& text, const char* token)
    {
        return text.find(token) != std::string::npos;
    }
}

TileMapRenderer::TileMapRenderer() = default;

TileMapRenderer::~TileMapRenderer()
{
    Shutdown();
}

bool TileMapRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& jsonPath)
{
    if (!JsonLoader::LoadTileMapData(jsonPath, mData))
    {
        LOG("[TileMapRenderer] Failed to load tile map json: %s", jsonPath.c_str());
        return false;
    }

    for (const auto& ts : mData.tilesets)
    {
        if (ts.texturePath.empty())
        {
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

        if (FAILED(hr))
        {
            LOG("[TileMapRenderer] ERROR: Failed to load tileset texture '%ls' (HRESULT 0x%08X).", ts.texturePath.c_str(), hr);
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        info.srv->GetResource(resource.GetAddressOf());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
        if (SUCCEEDED(resource.As(&tex2D)))
        {
            D3D11_TEXTURE2D_DESC desc;
            tex2D->GetDesc(&desc);
            const int tileWidth = (ts.tileWidth > 0) ? ts.tileWidth : mData.tileWidth;
            info.cols = (tileWidth > 0) ? static_cast<int>(desc.Width) / tileWidth : 1;
            if (info.cols <= 0) info.cols = 1;
        }
        else
        {
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
    RenderPass(context, camera, TileMapLayerPass::All);
}

void TileMapRenderer::RenderBackground(ID3D11DeviceContext* context, const Camera2D& camera)
{
    RenderPass(context, camera, TileMapLayerPass::Background);
}

void TileMapRenderer::RenderForeground(ID3D11DeviceContext* context, const Camera2D& camera)
{
    RenderPass(context, camera, TileMapLayerPass::Foreground);
}

void TileMapRenderer::RenderPass(ID3D11DeviceContext* context, const Camera2D& camera, TileMapLayerPass pass)
{
    if (!mSpriteBatch || mTextures.empty() || mData.layers.empty()) return;

    const DirectX::XMMATRIX view = camera.GetViewMatrix();

    // PointClamp preserves pixel art edges. Linear sampling would blur tile
    // boundaries and make authored pixel art look muddy while the camera moves.
    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->AlphaBlend(),
        mStates->PointClamp(),
        nullptr, nullptr, nullptr,
        view
    );

    for (const auto& layer : mData.layers)
    {
        if (ShouldRenderLayer(layer, pass))
        {
            DrawLayer(layer, camera);
        }
    }

    mSpriteBatch->End();
}

void TileMapRenderer::DrawLayer(const JsonLoader::TileLayer& layer, const Camera2D& camera)
{
    const int layerCols = (layer.cols > 0) ? layer.cols : mData.cols;
    const int layerRows = (layer.rows > 0) ? layer.rows : mData.rows;
    if (layerCols <= 0 || layerRows <= 0) return;

    const VisibleTileBounds bounds = ComputeVisibleBounds(camera, layerCols, layerRows);
    if (bounds.maxX < bounds.minX || bounds.maxY < bounds.minY) return;

    // Keep the map centered on world origin so existing player, campfire,
    // and enemy spawn coordinates remain stable when the map grows.
    const float startX = -((mData.cols * mData.tileWidth) / 2.0f);
    const float startY = -((mData.rows * mData.tileHeight) / 2.0f);

    for (int y = bounds.minY; y <= bounds.maxY; ++y)
    {
        for (int x = bounds.minX; x <= bounds.maxX; ++x)
        {
            const size_t index = static_cast<size_t>(y) * static_cast<size_t>(layerCols)
                               + static_cast<size_t>(x);
            if (index >= layer.tiles.size()) continue;

            int gid = layer.tiles[index];
            if (gid == 0) continue;

            // Clear Tiled flip flags from the top bits before resolving the
            // tileset. Flipped rendering is not implemented yet, but clearing
            // flags prevents an edited map from sampling invalid atlas cells.
            gid = static_cast<int>(static_cast<unsigned int>(gid) & 0x1FFFFFFFu);

            int tsIndex = 0;
            for (size_t i = 1; i < mData.tilesets.size(); ++i)
            {
                if (gid >= mData.tilesets[i].firstGid)
                {
                    tsIndex = static_cast<int>(i);
                }
            }

            const auto& ts = mData.tilesets[tsIndex];
            const auto& tex = mTextures[tsIndex];

            const int localId = gid - ts.firstGid;
            if (localId < 0) continue;

            const int tileWidth = (ts.tileWidth > 0) ? ts.tileWidth : mData.tileWidth;
            const int tileHeight = (ts.tileHeight > 0) ? ts.tileHeight : mData.tileHeight;
            const int srcX = (localId % tex.cols) * tileWidth;
            const int srcY = (localId / tex.cols) * tileHeight;

            const RECT srcRect = { srcX, srcY, srcX + tileWidth, srcY + tileHeight };
            const DirectX::XMFLOAT2 worldPos(
                startX + x * mData.tileWidth,
                startY + y * mData.tileHeight
            );

            mSpriteBatch->Draw(tex.srv.Get(), worldPos, &srcRect, DirectX::Colors::White);
        }
    }
}

TileMapRenderer::VisibleTileBounds TileMapRenderer::ComputeVisibleBounds(
    const Camera2D& camera,
    int layerCols,
    int layerRows) const
{
    VisibleTileBounds bounds;
    if (mData.tileWidth <= 0 || mData.tileHeight <= 0 || layerCols <= 0 || layerRows <= 0)
    {
        return bounds;
    }

    const DirectX::XMFLOAT2 center = camera.GetPosition();
    const float zoom = std::max(camera.GetZoom(), 0.01f);

    float halfW = (static_cast<float>(camera.GetScreenW()) * 0.5f) / zoom;
    float halfH = (static_cast<float>(camera.GetScreenH()) * 0.5f) / zoom;

    // Rotation can make the viewport corners reach farther than the axis
    // aligned half extents. Use the half diagonal as a conservative bound.
    if (std::fabs(camera.GetRotation()) > 0.0001f)
    {
        const float halfDiagonal = std::sqrt(halfW * halfW + halfH * halfH);
        halfW = halfDiagonal;
        halfH = halfDiagonal;
    }

    const float paddingX = static_cast<float>(mData.tileWidth * 2);
    const float paddingY = static_cast<float>(mData.tileHeight * 2);
    const float startX = -((mData.cols * mData.tileWidth) / 2.0f);
    const float startY = -((mData.rows * mData.tileHeight) / 2.0f);

    const float left = center.x - halfW - paddingX;
    const float right = center.x + halfW + paddingX;
    const float top = center.y - halfH - paddingY;
    const float bottom = center.y + halfH + paddingY;

    bounds.minX = std::max(0, static_cast<int>(std::floor((left - startX) / mData.tileWidth)));
    bounds.maxX = std::min(layerCols - 1, static_cast<int>(std::ceil((right - startX) / mData.tileWidth)));
    bounds.minY = std::max(0, static_cast<int>(std::floor((top - startY) / mData.tileHeight)));
    bounds.maxY = std::min(layerRows - 1, static_cast<int>(std::ceil((bottom - startY) / mData.tileHeight)));
    return bounds;
}

bool TileMapRenderer::ShouldRenderLayer(const JsonLoader::TileLayer& layer, TileMapLayerPass pass) const
{
    if (pass == TileMapLayerPass::All) return true;

    const bool foreground = IsForegroundLayer(layer.name);
    if (pass == TileMapLayerPass::Foreground) return foreground;
    return !foreground;
}

bool TileMapRenderer::IsForegroundLayer(const std::string& layerName) const
{
    const std::string lower = ToLowerAscii(layerName);
    return ContainsToken(lower, "foreground")
        || ContainsToken(lower, "front")
        || ContainsToken(lower, "above")
        || ContainsToken(lower, "canopy")
        || ContainsToken(lower, "roof")
        || ContainsToken(lower, "overlay");
}

void TileMapRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mTextures.clear();
    mData.layers.clear();
    mData.colliders.clear();
}
