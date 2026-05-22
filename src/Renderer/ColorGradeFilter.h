// ============================================================
// File: ColorGradeFilter.h
// Responsibility: Apply a restrained fullscreen color grade to the world.
//
// Visual effect:
//   The filter adjusts saturation, contrast, brightness, tint, and vignette.
//   It is meant for biome mood and region identity, not for hiding weak
//   composition or tinting UI text.
//
// Implementation:
//   The filter copies the current back buffer into an internal texture, then
//   draws a fullscreen quad sampling that copy through a color-grade shader.
//   This copy-based path avoids SpriteBatch render-target state hazards and
//   matches the project's current post-process style.
//
// Owns:
//   ID3D11Texture2D, RTV-free shader resource copy texture.
//   ID3D11ShaderResourceView sampled during the fullscreen draw.
//   ID3D11Buffer for the fullscreen quad and grade constant buffer.
//   ID3D11VertexShader, ID3D11PixelShader, ID3D11InputLayout.
//   ID3D11SamplerState, RasterizerState, DepthStencilState.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter.
//   Destroyed in -> OverworldState::OnExit.
//
// Common mistakes:
//   1. Applying this after UI draw tints text and HUD icons, hurting
//      readability. OverworldState renders UI after this filter.
//   2. Using strong tint values makes every biome look like a flat overlay.
//      Theme data should stay subtle.
//   3. Leaving the copied scene bound as SRV can trigger D3D11 warnings on
//      the next frame; Render unbinds it after drawing.
// ============================================================
#pragma once

#include "ColorGradeSettings.h"
#include "IScreenFilter.h"
#include <d3d11.h>
#include <wrl/client.h>

class ColorGradeFilter : public IScreenFilter
{
public:
    bool Initialize(ID3D11Device* device, int width, int height) override;
    void Shutdown() override;
    void BeginCapture(ID3D11DeviceContext* ctx) override;
    void EndCapture(ID3D11DeviceContext* ctx) override;
    void Render(ID3D11DeviceContext* ctx) override;
    void Update(float dt, float intensity) override;
    bool IsActive() const override;

    void SetSettings(const ColorGradeSettings& settings);

private:
    struct Vertex
    {
        float x;
        float y;
    };

    struct GradeCB
    {
        float tintR;
        float tintG;
        float tintB;
        float tintStrength;
        float saturation;
        float contrast;
        float brightness;
        float vignetteStrength;
    };

    static_assert(sizeof(GradeCB) == 32, "GradeCB must remain 32 bytes.");

    Microsoft::WRL::ComPtr<ID3D11Texture2D> mSceneTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mSceneSRV;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> mVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> mPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> mQuadVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> mConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> mSampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> mRasterState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> mDepthOffState;

    ColorGradeSettings mSettings;
    float mIntensity = 1.0f;
    bool mInitialized = false;
};
