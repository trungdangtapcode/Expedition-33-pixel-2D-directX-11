// ============================================================
// File: CurrencyHudRenderer.h
// Responsibility: Render the player's coin balance in screen-space UI.
//
// Owns:
//   SpriteBatch and CommonStates for the coin icon, plus a BattleTextRenderer
//   for the localized balance label.
//
// Lifetime:
//   Created by states that need currency UI.
//   Initialized in -> state OnEnter().
//   Destroyed in  -> state OnExit() through Shutdown().
//
// Important:
//   - Layout and icon path come from data/currency_hud.json.
//   - The balance itself is not owned here; callers pass Wallet::GetCoins().
//
// Common mistakes:
//   1. Rendering without Initialize() -> no SRV or font is available.
//   2. Hardcoding text -> language changes would not update the HUD.
//   3. Forgetting SetScreenSize() on resize -> top-right anchoring drifts.
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

struct CurrencyHudConfig
{
    std::string iconPath = "assets/UI/coin_icon.png";
    std::string labelKey = "currency.coins";
    float sourceIconSize = 32.0f;
    float iconSize = 24.0f;
    float textOffsetX = 32.0f;
    float textOffsetY = 2.0f;
    float topRightWidth = 132.0f;
    float topRightMarginX = 24.0f;
    float topRightY = 20.0f;
    float campfireOffsetX = 382.0f;
    float campfireOffsetY = 96.0f;
};

class CurrencyHudRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::wstring& fontPath,
                    int screenW,
                    int screenH);

    void RenderTopRight(ID3D11DeviceContext* context, int coins);
    void RenderCampfirePanel(ID3D11DeviceContext* context,
                             int coins,
                             float panelX,
                             float panelY);

    void SetScreenSize(int screenW, int screenH);
    void Shutdown();
    bool IsReady() const { return mCoinSRV != nullptr && mTextRenderer.IsReady(); }

private:
    bool LoadConfig(const std::string& path);
    bool LoadIcon(ID3D11Device* device,
                  ID3D11DeviceContext* context,
                  const std::string& path);
    void RenderAt(ID3D11DeviceContext* context, int coins, float x, float y);
    std::string FormatLabel(int coins) const;
    void BindViewport(ID3D11DeviceContext* context) const;

    CurrencyHudConfig mConfig;
    BattleTextRenderer mTextRenderer;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mCoinSRV;
    int mScreenW = 1280;
    int mScreenH = 720;
};
