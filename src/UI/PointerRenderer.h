// ============================================================
// File: PointerRenderer.h
// Responsibility: Renders a floating pointer arrow (in world space)
//                 to indicate the currently selected target.
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

class PointerRenderer {
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, 
                    const std::wstring& texturePath, const std::string& jsonPath,
                    int screenW, int screenH);

    void Update(float dt);

    void Draw(ID3D11DeviceContext* context, float worldX, float worldY, 
              DirectX::CXMMATRIX transform);

    void Shutdown();

private:
    void BindViewport(ID3D11DeviceContext* context);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    
    int mWidth = 64;
    int mHeight = 64;
    float mPivotX = 32.0f;
    float mPivotY = 32.0f;
    int mScreenW = 1280;
    int mScreenH = 720;
    
    // Data-driven animation properties
    float mElapsedTime = 0.0f;
    float mOffsetY = -128.0f;
    float mBobSpeed = 0.0f;
    float mBobAmplitude = 0.0f;
};
