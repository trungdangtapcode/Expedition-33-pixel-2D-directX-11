// ============================================================
// File: TitleMenuRenderer.h
// Responsibility: Render the atmospheric title screen, press-start prompt,
//                 command menu, and load-slot picker.
//
// Owns:
//   ID3D11ShaderResourceView for the title banner texture.
//   ID3D11ShaderResourceView for a 1x1 tintable fill texture.
//   SpriteBatch and CommonStates for screen-space image/fill draws.
//   BattleTextRenderer for title prompt, command, and slot labels.
//
// Lifetime:
//   Created in  -> MenuState::OnEnter()
//   Destroyed in -> MenuState::OnExit()
//
// Important:
//   - MenuState owns input and save/load transitions.
//   - This renderer only observes prepared view data.
//   - Layout values are loaded from data/main_menu_layout.json.
//   - The title BGM track id is layout data; audio files stay in bgm.json.
//
// Common mistakes:
//   1. Loading save files here -> bypasses SaveManager validation.
//   2. Drawing text outside BattleTextRenderer's batch contract.
//   3. Forgetting SetScreenSize() after resize -> SpriteBatch viewport mismatch.
// ============================================================
#pragma once

#include "BattleTextRenderer.h"
#include <CommonStates.h>
#include <SpriteBatch.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>

enum class TitleMenuVisualPhase
{
    PressStart,
    MainOptions,
    Options,
    NewGameSlots,
    LoadSlots
};

struct TitleMenuOptionView
{
    std::string label;
    bool enabled = true;
};

struct TitleMenuSlotView
{
    std::string primary;
    std::string secondary;
    bool exists = false;
    bool active = false;
};

struct TitleMenuRenderState
{
    TitleMenuVisualPhase phase = TitleMenuVisualPhase::MainOptions;
    std::vector<TitleMenuOptionView> options;
    std::vector<TitleMenuSlotView> slots;
    int cursor = 0;
    int slotCursor = 0;
    float elapsed = 0.0f;
    std::string flashMessage;
    float flashAlpha = 0.0f;
    float transitionAlpha = 0.0f;
};

class TitleMenuRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::string& layoutPath,
                    int screenW,
                    int screenH);

    void SetScreenSize(int screenW, int screenH);
    bool ReloadFont(ID3D11Device* device, ID3D11DeviceContext* context);

    void Render(ID3D11DeviceContext* context, const TitleMenuRenderState& state);

    void Shutdown();

    bool IsInitialized() const { return mInitialized; }
    float GetFlashDuration() const { return mLayout.flashDuration; }
    float GetTransitionFadeOutDuration() const { return mLayout.transitionFadeOutDuration; }
    const std::string& GetBgmTrackId() const { return mLayout.bgmTrackId; }

private:
    struct Layout
    {
        std::string backgroundImagePath = "assets/e33_pixel_banner.png";
        std::string fontPath = "assets/fonts/arial_16.spritefont";
        std::string bgmTrackId;
        float slotPanelWidth = 760.0f;
        float slotPanelHeight = 372.0f;
        float slotPanelBottom = 64.0f;
        float backgroundDimAlpha = 0.32f;
        float logoAlphaMin = 0.72f;
        float logoAlphaMax = 0.92f;
        float logoPulseSpeed = 0.62f;
        float particleAlpha = 0.5f;
        float pressPromptY = 560.0f;
        float pressPromptScale = 1.28f;
        float pressPromptBlinkSpeed = 3.4f;
        float optionStartY = 514.0f;
        float optionRowHeight = 42.0f;
        float optionTextScale = 1.08f;
        float slotStartY = 96.0f;
        float slotRowHeight = 76.0f;
        float flashDuration = 2.2f;
        float transitionFadeOutDuration = 0.0f;
    };

    bool LoadLayout(const std::string& layoutPath);
    bool LoadBackgroundTexture(ID3D11Device* device, ID3D11DeviceContext* context);
    bool CreateFillTexture(ID3D11Device* device);
    void BindViewport(ID3D11DeviceContext* context);
    void DrawBackdrop(ID3D11DeviceContext* context, float elapsed);
    void DrawAmbientParticles(ID3D11DeviceContext* context, float elapsed);
    void DrawFillRect(ID3D11DeviceContext* context,
                      float x,
                      float y,
                      float width,
                      float height,
                      DirectX::FXMVECTOR color);
    void DrawTransitionOverlay(ID3D11DeviceContext* context, float alpha);
    void RenderPressStart(ID3D11DeviceContext* context,
                          const TitleMenuRenderState& state);
    void RenderMainOptions(ID3D11DeviceContext* context,
                           const TitleMenuRenderState& state);
    void RenderLoadSlots(ID3D11DeviceContext* context,
                         const TitleMenuRenderState& state);

    static std::wstring ToWidePath(const std::string& path);
    static std::string ShortenText(const std::string& text, size_t limit);

    Layout mLayout;
    int mScreenW = 1280;
    int mScreenH = 720;
    int mBackgroundW = 1;
    int mBackgroundH = 1;
    bool mInitialized = false;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mBackgroundSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    BattleTextRenderer mTextRenderer;
};
