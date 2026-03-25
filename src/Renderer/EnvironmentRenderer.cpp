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

    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        nullptr, nullptr, nullptr, nullptr,
        camera.GetViewMatrix()
    );

    // Dynamic sizing based on Target View 
    // Example: The physical camera view expects 1080 units in height.
    // We scale the asset (which might be 1536 tall) to fit the 1080 logical height constraint nicely.
    float scale = mConfig.targetViewHeight > 0.0f ? (mConfig.targetViewHeight / mConfig.height) : 1.0f;

    // The asset origin is currently calculated as Center.
    DirectX::XMFLOAT2 origin = { mConfig.width / 2.0f, mConfig.height / 2.0f };

    // Where do we draw it in the world space? 
    // We position the background center so it sits at (0,0) of the world space + any offsets assigned.
    DirectX::XMFLOAT2 position = { mConfig.offsetX, mConfig.offsetY };

    mSpriteBatch->Draw(
        srv,
        position,
        nullptr,
        DirectX::Colors::White,
        0.f,                     // Rotation
        origin,                  // Origin (Center of texture)
        scale,                   // Scale
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
