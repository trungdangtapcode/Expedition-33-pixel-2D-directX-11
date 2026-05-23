// ============================================================
// File: BattleResultRenderer.h
// Responsibility: Draw cinematic victory and defeat result overlays
//                 on top of the frozen battle scene.
//
// Owns:
//   SpriteBatch/CommonStates for filled overlays and portrait sprites.
//   One 1x1 white SRV for tintable rectangles.
//   NineSliceRenderer for the defeat retry prompt panel.
//   Portrait SRVs loaded from PartyManager metadata for result rows.
//
// Lifetime:
//   Created in  -> BattleState::OnEnter().
//   Destroyed in -> BattleState::OnExit().
//
// Important:
//   - Text is drawn through BattleTextRenderer owned by BattleState so
//     localization font selection stays centralized.
//   - The renderer never reads gameplay singletons; all gameplay values
//     arrive through BattleResultData.
// ============================================================
#pragma once

#include "../Battle/BattleResultData.h"
#include "../Renderer/NineSliceRenderer.h"
#include "../UI/BattleTextRenderer.h"
#include "../Utils/JsonLoader.h"
#include <CommonStates.h>
#include <SpriteBatch.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

class BattleResultRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const JsonLoader::BattleResultLayout& layout,
                    int screenW,
                    int screenH);

    void Shutdown();
    void SetScreenSize(int screenW, int screenH);
    void Update(float dt);
    void LoadPortraits(ID3D11Device* device,
                       ID3D11DeviceContext* context,
                       const BattleResultData& data);

    void RenderVictory(ID3D11DeviceContext* context,
                       BattleTextRenderer& text,
                       const BattleResultData& data,
                       float visibleSeconds);
    void RenderDefeatSplash(ID3D11DeviceContext* context,
                            BattleTextRenderer& text,
                            float visibleSeconds);
    void RenderDefeatPrompt(ID3D11DeviceContext* context,
                            BattleTextRenderer& text,
                            int selectedOption,
                            float visibleSeconds);

private:
    struct PortraitEntry
    {
        std::string memberId;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    bool CreateFillTexture(ID3D11Device* device);
    void BindViewport(ID3D11DeviceContext* context);
    void BeginRects(ID3D11DeviceContext* context);
    void EndRects();
    void DrawFillRect(float x, float y, float width, float height, DirectX::FXMVECTOR color);
    void DrawDecorativeFrame(float x, float y, float width, float height, DirectX::FXMVECTOR color);
    void DrawBackdrop(float alphaMul);
    void DrawVictoryText(ID3D11DeviceContext* context,
                         BattleTextRenderer& text,
                         const BattleResultData& data,
                         float alpha);
    void DrawVictoryPortraits(const BattleResultData& data, float alpha);
    void DrawDefeatGlyph(float centerX, float centerY, float alpha);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FindPortrait(const std::string& memberId) const;

    JsonLoader::BattleResultLayout mLayout;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;
    NineSliceRenderer mPromptPanel;
    std::vector<PortraitEntry> mPortraits;

    int mScreenW = 1280;
    int mScreenH = 720;
    float mElapsed = 0.0f;
};
