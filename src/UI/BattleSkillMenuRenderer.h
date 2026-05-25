// ============================================================
// File: BattleSkillMenuRenderer.h
// Responsibility: Draw the screen-space battle skill card menu and
//                 selected-skill details.
// ============================================================
#pragma once
#include "../Battle/SkillTypes.h"
#include "../Battle/StatusEffectView.h"
#include <CommonStates.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <WICTextureLoader.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class BattleTextRenderer;
class IBattler;
class ISkill;
class PlayerCombatant;
struct BattleContext;

struct BattleSkillMenuLayout
{
    int pageSize = 4;
    float entryDuration = 0.28f;
    float fadeStartAlpha = 0.0f;
    float panelAlpha = 0.84f;
    float cardX = 590.0f;
    float cardY = 262.0f;
    float cardWidth = 370.0f;
    float cardHeight = 46.0f;
    float cardSpacing = 14.0f;
    float cardAngle = -0.035f;
    float selectedScale = 1.04f;
    float iconSize = 26.0f;
    float iconOffsetX = 18.0f;
    float iconOffsetY = 10.0f;
    float nameOffsetX = 56.0f;
    float nameOffsetY = 12.0f;
    float costOffsetX = 292.0f;
    float costOffsetY = 12.0f;
    float pageTextX = 804.0f;
    float pageTextY = 474.0f;
    float detailX = 760.0f;
    float detailY = 120.0f;
    float detailWidth = 430.0f;
    float detailHeight = 180.0f;
    float detailTitleOffsetX = 24.0f;
    float detailTitleOffsetY = 18.0f;
    float detailBodyOffsetX = 24.0f;
    float detailBodyOffsetY = 54.0f;
    float detailLineSpacing = 22.0f;
    float statusIconX = 1088.0f;
    float statusIconY = 198.0f;
    float targetDetailX = 760.0f;
    float targetDetailY = 318.0f;
    float targetDetailWidth = 430.0f;
    float targetDetailHeight = 112.0f;
    float textScale = 0.58f;
    float smallTextScale = 0.42f;
    float detailTextScale = 0.42f;
};

class BattleSkillMenuRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::string& layoutPath,
                    const std::wstring& iconAtlasPath,
                    const std::string& iconMetadataPath,
                    int screenW,
                    int screenH);

    void Update(float dt, bool visible);

    void Render(ID3D11DeviceContext* context,
                BattleTextRenderer& text,
                const PlayerCombatant* activePlayer,
                int selectedSkillIndex,
                bool targetSelectActive,
                int targetIndex,
                const std::vector<IBattler*>& enemies,
                const BattleContext& battleContext);

    void SetScreenSize(int w, int h);
    void Shutdown();
    bool IsInitialized() const { return mSpriteBatch != nullptr; }
    int GetPageSize() const { return mLayout.pageSize; }

private:
    struct Frame
    {
        int x = 0;
        int y = 0;
        int w = 32;
        int h = 32;
    };

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mIconAtlasSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    std::unordered_map<std::string, Frame> mIconFrames;
    BattleSkillMenuLayout mLayout;
    int mScreenW = 1280;
    int mScreenH = 720;
    float mTimer = 0.0f;

    void BindViewport(ID3D11DeviceContext* context);
    bool LoadLayout(const std::string& path);
    bool LoadIconMetadata(const std::string& path);
    bool CreateFillTexture(ID3D11Device* device);
    void DrawPanel(float x, float y, float w, float h, DirectX::XMVECTOR color);
    void DrawRotatedPanel(float centerX, float centerY, float w, float h, float rotation, DirectX::XMVECTOR color);
    void DrawIcon(const std::string& iconId, float x, float y, float size, DirectX::XMVECTOR color);
    std::string CostText(const ISkill& skill) const;
    std::string TargetText(SkillTargeting targeting) const;
    std::string DamageTypeText(const ISkill& skill) const;
    std::string DamageGradeText(const ISkill& skill) const;
    std::string AvailabilityText(const ISkill& skill, const IBattler& caster, const BattleContext& ctx) const;
    std::string TruncateForCard(const std::string& text, std::size_t maxBytes) const;
};
