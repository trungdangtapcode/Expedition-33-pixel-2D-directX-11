// ============================================================
// File: StatusIconRenderer.h
// Responsibility: Render active status effect icons near party HP bars.
// ============================================================
#pragma once
#include "../Battle/StatusEffectView.h"
#include <CommonStates.h>
#include <SpriteBatch.h>
#include <WICTextureLoader.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class BattleTextRenderer;

struct StatusIconLayout
{
    float offsetX = 58.0f;
    float offsetY = 146.0f;
    float iconSize = 24.0f;
    float spacing = 4.0f;
    int maxVisible = 5;
    float textScale = 0.34f;
    float badgePadding = 2.0f;
    float badgeFrameThickness = 1.0f;
    float badgeBackR = 0.03f;
    float badgeBackG = 0.03f;
    float badgeBackB = 0.035f;
    float badgeBackA = 0.78f;
    float buffFrameR = 0.86f;
    float buffFrameG = 0.67f;
    float buffFrameB = 0.25f;
    float buffFrameA = 0.95f;
    float debuffFrameR = 0.78f;
    float debuffFrameG = 0.20f;
    float debuffFrameB = 0.22f;
    float debuffFrameA = 0.95f;
    float neutralFrameR = 0.48f;
    float neutralFrameG = 0.48f;
    float neutralFrameB = 0.54f;
    float neutralFrameA = 0.90f;
};

class StatusIconRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::wstring& atlasPath,
                    const std::string& metadataPath,
                    const std::string& layoutPath,
                    int screenW,
                    int screenH);

    void Render(ID3D11DeviceContext* context,
                BattleTextRenderer& textRenderer,
                const std::vector<StatusEffectView>& effects,
                float barX,
                float barY);

    void RenderAt(ID3D11DeviceContext* context,
                  BattleTextRenderer& textRenderer,
                  const std::vector<StatusEffectView>& effects,
                  float x,
                  float y);

    void SetScreenSize(int w, int h);
    void Shutdown();
    bool IsInitialized() const { return mSpriteBatch != nullptr; }

private:
    struct Frame
    {
        int x = 0;
        int y = 0;
        int w = 32;
        int h = 32;
    };

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mAtlasSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    std::unordered_map<std::string, Frame> mFrames;
    StatusIconLayout mLayout;
    int mScreenW = 1280;
    int mScreenH = 720;

    void BindViewport(ID3D11DeviceContext* context);
    bool LoadMetadata(const std::string& path);
    bool LoadLayout(const std::string& path);
    DirectX::XMVECTORF32 CategoryColor(StatusEffectCategory category) const;
    void DrawSolidRect(float x,
                       float y,
                       float width,
                       float height,
                       DirectX::FXMVECTOR color);
    void DrawBadgeFrame(float x,
                        float y,
                        float size,
                        DirectX::FXMVECTOR frameColor);
};
