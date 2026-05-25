// ============================================================
// File: TileMapRenderer.h
// Responsibility: Render Tiled tile layers in explicit world passes.
//
// Owns:
//   SpriteBatch    - batches world-space tile draw calls.
//   CommonStates   - owns sampler/blend state used by SpriteBatch.
//   TextureInfo[]  - shader resource views for every tileset image.
//   TileMapData    - parsed map, layer, tileset, and collider data.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter().
//   Destroyed in -> OverworldState::OnExit() via Shutdown().
//
// Important:
//   - Uses camera.GetViewMatrix(), not view-projection.
//   - Tile layers can be split into background and foreground passes.
//   - Foreground layer names use an authoring convention, not C++ hardcoding.
//
// Common mistakes:
//   1. Drawing all map layers before entities makes trees, roofs, and gates
//      unable to occlude the player.
//   2. Passing camera.GetViewProjectionMatrix() to SpriteBatch double-projects
//      tiles and makes them disappear.
//   3. Rendering every tile on a large map wastes CPU time; this renderer
//      culls the tile loops to the camera viewport.
// ============================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <CommonStates.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "Camera.h"
#include "../Utils/JsonLoader.h"

enum class TileMapLayerPass
{
    All,
    Background,
    Foreground
};

class TileMapRenderer
{
public:
    TileMapRenderer();
    ~TileMapRenderer();

    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose:
    //   Load the Tiled JSON map, load each tileset texture, and create
    //   SpriteBatch resources used for world-space tile rendering.
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& jsonPath);

    // ------------------------------------------------------------
    // Function: Render
    // Purpose:
    //   Draw every visible tile layer. Kept for callers that do not need
    //   foreground/background pass separation.
    // ------------------------------------------------------------
    void Render(ID3D11DeviceContext* context, const Camera2D& camera);

    // ------------------------------------------------------------
    // Function: RenderBackground
    // Purpose:
    //   Draw map layers that should appear behind SceneGraph entities.
    //   This includes normal Tiled layers such as Ground, Roads, and Objects.
    // ------------------------------------------------------------
    void RenderBackground(ID3D11DeviceContext* context, const Camera2D& camera);

    // ------------------------------------------------------------
    // Function: RenderForeground
    // Purpose:
    //   Draw map layers authored to appear in front of SceneGraph entities.
    //   Layer names containing Foreground, Front, Above, Canopy, Roof, or
    //   Overlay are routed here.
    // ------------------------------------------------------------
    void RenderForeground(ID3D11DeviceContext* context, const Camera2D& camera);

    void Shutdown();

    const JsonLoader::TileMapData& GetData() const { return mData; }

private:
    struct TextureInfo
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        int cols = 1;
    };

    struct VisibleTileBounds
    {
        int minX = 0;
        int minY = 0;
        int maxX = -1;
        int maxY = -1;
    };

    void RenderPass(ID3D11DeviceContext* context, const Camera2D& camera, TileMapLayerPass pass);
    void DrawLayer(const JsonLoader::TileLayer& layer, const Camera2D& camera);
    VisibleTileBounds ComputeVisibleBounds(const Camera2D& camera, int layerCols, int layerRows) const;
    bool ShouldRenderLayer(const JsonLoader::TileLayer& layer, TileMapLayerPass pass) const;
    bool IsForegroundLayer(const std::string& layerName) const;

    std::vector<TextureInfo> mTextures;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;

    JsonLoader::TileMapData mData;
};
