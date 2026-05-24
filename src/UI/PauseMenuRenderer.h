// ============================================================
// File: PauseMenuRenderer.h
// Responsibility: Render the screen-space overworld pause overlay.
//
// Owns:
//   A 1x1 fill texture, SpriteBatch, CommonStates, and BattleTextRenderer.
//
// Lifetime:
//   Created in  -> PauseState::OnEnter().
//   Destroyed in -> PauseState::OnExit().
//
// Important:
//   - PauseState owns input and transition decisions.
//   - This renderer only draws prepared view data.
//   - Layout and SFX ids are loaded from data/pause_menu_layout.json.
// ============================================================
#pragma once

#include "BattleTextRenderer.h"
#include <CommonStates.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>

struct PauseMenuOptionView
{
    std::string label;
};

struct PauseMenuRenderState
{
    std::vector<PauseMenuOptionView> options;
    int cursor = 0;
    bool confirming = false;
    std::string confirmMessage;
    int confirmCursor = 1;
    float elapsed = 0.0f;
    float fadeAlpha = 0.0f;
};

class PauseMenuRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::string& layoutPath,
                    int screenW,
                    int screenH);

    void SetScreenSize(int screenW, int screenH);
    void Render(ID3D11DeviceContext* context, const PauseMenuRenderState& state);
    void Shutdown();

    float GetFadeDuration() const { return mLayout.fadeDuration; }
    const std::string& GetNavigateSfxId() const { return mLayout.navigateSfxId; }
    const std::string& GetConfirmSfxId() const { return mLayout.confirmSfxId; }
    const std::string& GetBackSfxId() const { return mLayout.backSfxId; }

private:
    struct Layout
    {
        std::string fontPath = "assets/fonts/arial_16.spritefont";
        std::string navigateSfxId = "ui_navigate";
        std::string confirmSfxId = "ui_confirm";
        std::string backSfxId = "ui_back";

        float dimAlpha = 0.54f;
        float panelWidth = 560.0f;
        float panelHeight = 330.0f;
        float panelCenterY = 388.0f;
        float panelAlpha = 0.62f;
        float borderAlpha = 0.78f;
        float borderThickness = 2.0f;
        float titleOffsetY = 34.0f;
        float titleScale = 1.55f;
        float optionStartOffsetY = 112.0f;
        float optionRowHeight = 46.0f;
        float optionTextScale = 1.08f;
        float highlightHeight = 34.0f;
        float highlightInset = 48.0f;
        float confirmPanelWidth = 620.0f;
        float confirmPanelHeight = 214.0f;
        float confirmTextOffsetY = 56.0f;
        float confirmOptionOffsetY = 130.0f;
        float confirmOptionGap = 190.0f;
        float confirmTextScale = 1.08f;
        float fadeDuration = 0.55f;
    };

    bool LoadLayout(const std::string& layoutPath);
    bool CreateFillTexture(ID3D11Device* device);
    void BindViewport(ID3D11DeviceContext* context);
    void DrawFillRect(ID3D11DeviceContext* context,
                      float x,
                      float y,
                      float width,
                      float height,
                      DirectX::FXMVECTOR color);
    void DrawPanel(ID3D11DeviceContext* context,
                   float x,
                   float y,
                   float width,
                   float height,
                   float alpha);
    void DrawMainPanel(ID3D11DeviceContext* context, const PauseMenuRenderState& state);
    void DrawConfirmPanel(ID3D11DeviceContext* context, const PauseMenuRenderState& state);
    void DrawFade(ID3D11DeviceContext* context, float alpha);

    static std::wstring ToWidePath(const std::string& path);

    Layout mLayout;
    int mScreenW = 1280;
    int mScreenH = 720;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    BattleTextRenderer mTextRenderer;
};
