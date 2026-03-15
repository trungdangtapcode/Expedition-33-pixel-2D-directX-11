// ============================================================
// File: NineSliceRenderer.h
// Responsibility: Draw scalable UI elements using the 9-slice pattern.
//
// NineSliceRenderer takes a single texture and its JSON metadata (crop-region
// and nine-slice margins) and slices it into 9 rendering segments. It allows UI
// components (like dialog boxes or buttons) to scale their width and height
// without stretching their border corners.
//
// Expected JSON Schema:
// {
//      "width": 256,
//      "height": 256,
//      "crop-region": { "left": 16, "top": 80, "right": 231, "bottom": 188 },
//      "nine-slice": { "left": 46, "top": 110, "right": 197, "bottom": 152 }
// }
//
// Lifetime:
//   Created in  -> Typically a State or UI Component.
//   Destroyed in -> The same class.
//
// Usage:
//   NineSliceRenderer dialog;
//   dialog.Initialize(device, context, L"assets/UI/ui-dialog-box-hd.png", "assets/UI/ui-dialog-box-hd.json");
//   dialog.Draw(context, x, y, targetWidth, targetHeight, 1.0f);
// ============================================================
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <string>
#include <memory>

struct NineSliceRegion {
    float left, top, right, bottom;
};

struct NineSliceData {
    int width, height;
    NineSliceRegion cropRegion;
    NineSliceRegion nineSlice;
};

class NineSliceRenderer {
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::wstring& texturePath, const std::string& jsonPath, int screenW, int screenH);
    
    // Draw the 9-slice UI.
    // By default, draws with Identity Matrix. To project to a specific camera view, pass a custom transform.
    void Draw(ID3D11DeviceContext* context, float destX, float destY, float targetWidth, float targetHeight,
              float scale = 1.0f, DirectX::CXMMATRIX transform = DirectX::XMMatrixIdentity(),
              DirectX::XMVECTOR color = DirectX::Colors::White);

    void Shutdown();
private:
    void BindViewport(ID3D11DeviceContext* context);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    NineSliceData mData{};
    int mScreenW = 1280;
    int mScreenH = 720;
};
