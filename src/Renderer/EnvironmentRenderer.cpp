// ============================================================
// File: EnvironmentRenderer.cpp
// ============================================================
#include "EnvironmentRenderer.h"
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <WICTextureLoader.h>
#include "../Utils/Log.h"

EnvironmentRenderer::EnvironmentRenderer() = default;
EnvironmentRenderer::~EnvironmentRenderer() = default;

void EnvironmentRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    mDevice = device;
    mContext = context;
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);
}

bool EnvironmentRenderer::LoadEnvironment(const std::string& jsonPath)
{
    mBgSRV.Reset();
    mFgSRV.Reset();

    if (!JsonLoader::LoadEnvironmentConfig(jsonPath, mConfig))
    {
        LOG("[EnvironmentRenderer] Failed to load config: %s", jsonPath.c_str());
        return false;
    }

    auto loadTex = [&](const std::wstring& texPath, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) {
        if (texPath.empty()) return;

        HRESULT hr = DirectX::CreateWICTextureFromFileEx(
            mDevice.Get(), mContext.Get(), texPath.c_str(),
            0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
            DirectX::WIC_LOADER_IGNORE_SRGB, nullptr, srv.GetAddressOf()
        );

        if (FAILED(hr))
        {
            LOG("[EnvironmentRenderer] WARNING: Failed to load texture: %ls", texPath.c_str());
        }
    };

    loadTex(mConfig.background, mBgSRV);
    loadTex(mConfig.foreground, mFgSRV);

    return true;
}

void EnvironmentRenderer::DrawTexture(ID3D11ShaderResourceView* srv, const Camera2D& camera)
{
    if (!srv) return;

    // Retrieve viewport from context or camera dimensions to ensure SpriteBatch projects correctly
    D3D11_VIEWPORT vp;
    UINT numVPs = 1;
    mContext->RSGetViewports(&numVPs, &vp);
    vp.Width = (float)camera.GetScreenW();
    vp.Height = (float)camera.GetScreenH();
    mContext->RSSetViewports(1, &vp);

    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        nullptr, nullptr, nullptr, nullptr,
        camera.GetViewMatrix()
    );

    // Dynamic Scale Calculation (Relative/Adaptive Sizing)
    // We first calculate the scale needed to exactly fit the current screen height.
    // Then we multiply it by zoomLevel (e.g., 1.2 for 20% zoom or 1.0 for perfect fit).
    float baseScale = static_cast<float>(camera.GetScreenH()) / static_cast<float>(mConfig.height);
    float finalScale = baseScale * (mConfig.zoomLevel > 0.0f ? mConfig.zoomLevel : 1.0f);

    // Make origin the center of the texture
    DirectX::XMFLOAT2 origin = { static_cast<float>(mConfig.width) / 2.0f, static_cast<float>(mConfig.height) / 2.0f }; 

    // Where do we draw it in the world space?
    // We position the background center so it sits at (0,0) of the world space + offsets.
    // By multiplying offsetX and offsetY by finalScale, we transform absolute "world units" 
    // into "relative texture units". This guarantees that if you offset the ground by 400px 
    // on the original texture, it will stay exactly at that proportion no matter how the screen scales!
    DirectX::XMFLOAT2 position = { 
        mConfig.offsetX * finalScale, 
        mConfig.offsetY * finalScale 
    };

    mSpriteBatch->Draw(
        srv,
        position,
        nullptr,
        DirectX::Colors::White,
        0.f,                     // Rotation
        origin,                  // Origin (Center of texture)
        finalScale,              // Scale
        DirectX::SpriteEffects_None,
        0.0f
    );

    mSpriteBatch->End();
}

void EnvironmentRenderer::RenderBackground(const Camera2D& camera)
{
    DrawTexture(mBgSRV.Get(), camera);
}

void EnvironmentRenderer::RenderForeground(const Camera2D& camera)
{
    DrawTexture(mFgSRV.Get(), camera);
}
