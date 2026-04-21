// ============================================================
// File: TileMapRenderer.h
// Responsibility: Draw an optimized regular grid of background tiles
//                 using a single SpriteBatch call and a Camera2D transform.
// ============================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <DirectXMath.h>
#include <memory>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include "Camera.h"
#include "../Utils/JsonLoader.h"

class TileMapRenderer
{
public:
    TileMapRenderer();
    ~TileMapRenderer();

    // ------------------------------------------------------------
    // Function: Initialize
    // Purpose: Load tile map descriptor and texture, create SpriteBatch.
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& jsonPath);

    // ------------------------------------------------------------
    // Function: Render
    // Purpose: Draw the active tiles using the camera's view-projection matrix.
    // ------------------------------------------------------------
    void Render(ID3D11DeviceContext* context, const Camera2D& camera);

    void Shutdown();

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;

    JsonLoader::TileMapData mData;
    int mTilesetCols = 0;
};
