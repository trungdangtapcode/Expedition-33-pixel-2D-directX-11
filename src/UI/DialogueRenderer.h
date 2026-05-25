// ============================================================
// File: DialogueRenderer.h
// Responsibility: Render screen-space story dialogue panels.
//
// Owns:
//   A 1x1 fill texture, SpriteBatch, CommonStates, BattleTextRenderer,
//   and an optional NineSliceRenderer for the dialogue panel frame.
//
// Lifetime:
//   Created in  -> DialogueState::OnEnter().
//   Destroyed in -> DialogueState::OnExit().
//
// Important:
//   - DialogueState owns input and text reveal state.
//   - This renderer only draws prepared view data.
//   - Layout values are loaded from data/dialogue_layout.json.
// ============================================================
#pragma once

#include "BattleTextRenderer.h"
#include "../Renderer/NineSliceRenderer.h"
#include <CommonStates.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>

struct DialogueRenderState
{
    std::string speakerName;
    std::string text;
    std::string prompt;
    bool lineComplete = false;
    float elapsed = 0.0f;
};

class DialogueRenderer
{
public:
    ~DialogueRenderer();

    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::string& layoutPath,
                    int screenW,
                    int screenH);

    void SetScreenSize(int screenW, int screenH);
    void Render(ID3D11DeviceContext* context, const DialogueRenderState& state);
    void Shutdown();

    float GetCharsPerSecond() const { return mLayout.charsPerSecond; }
    const std::string& GetConfirmSfxId() const { return mLayout.confirmSfxId; }
    const std::string& GetBackSfxId() const { return mLayout.backSfxId; }

private:
    struct Layout
    {
        std::string fontPath = "assets/fonts/arial_16.spritefont";
        std::string panelTexturePath = "assets/UI/pause_menu_panel.png";
        std::string panelJsonPath = "assets/UI/pause_menu_panel.json";
        std::string confirmSfxId = "ui_confirm";
        std::string backSfxId = "ui_back";

        float dimAlpha = 0.18f;
        float panelWidth = 920.0f;
        float panelHeight = 190.0f;
        float panelBottom = 38.0f;
        float panelAlpha = 0.90f;
        float panelSliceScale = 1.0f;
        float panelFallbackBorder = 2.0f;
        float speakerOffsetX = 54.0f;
        float speakerOffsetY = 28.0f;
        float speakerScale = 1.10f;
        float textOffsetX = 54.0f;
        float textOffsetY = 72.0f;
        float textScale = 1.02f;
        float lineHeight = 29.0f;
        float textMaxWidth = 790.0f;
        float promptOffsetX = 726.0f;
        float promptOffsetY = 146.0f;
        float promptScale = 0.86f;
        float promptBlinkSpeed = 3.0f;
        float charsPerSecond = 42.0f;
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
    std::vector<std::string> WrapText(const std::string& text,
                                      std::size_t maxGlyphsPerLine) const;

    static std::wstring ToWidePath(const std::string& path);

    Layout mLayout;
    int mScreenW = 1280;
    int mScreenH = 720;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    BattleTextRenderer mTextRenderer;
    std::unique_ptr<NineSliceRenderer> mPanelRenderer;
    bool mPanelRendererReady = false;
};
