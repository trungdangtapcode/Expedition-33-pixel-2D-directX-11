// ============================================================
// File: EnvironmentRenderer.h
// Responsibility: Encapsulate data-driven rendering of dynamic backgrounds
//                 based on environment JSON configurations, scaled to a Target View Height.
// ============================================================
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <DirectXMath.h>
#include <memory>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include "Camera.h"
#include "../Utils/JsonLoader.h"

class EnvironmentRenderer
{
public:
    EnvironmentRenderer();
    ~EnvironmentRenderer();

    // Init the renderer, providing GPU context
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // Load an environment configuration from JSON (e.g. assets/environments/battle-paris-view.json)
    bool LoadEnvironment(const std::string& jsonPath);

    // Render the environment using the active camera's transform.
    // Call RenderBackground() BEFORE drawing sprites, and RenderForeground() AFTER.
    void RenderBackground(const Camera2D& camera);
    void RenderForeground(const Camera2D& camera);

private:
    Microsoft::WRL::ComPtr<ID3D11Device> mDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> mContext;

    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;

    JsonLoader::EnvironmentConfig mConfig;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mBgSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFgSRV;

    void DrawTexture(ID3D11ShaderResourceView* srv, const Camera2D& camera);
};
