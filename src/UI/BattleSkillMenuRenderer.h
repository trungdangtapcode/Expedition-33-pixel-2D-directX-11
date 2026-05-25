// ============================================================
// File: BattleSkillMenuRenderer.h
// Responsibility: Draw the screen-space battle skill card menu and
//                 selected-skill details.
// ============================================================
#pragma once
#include "../Battle/SkillTypes.h"
#include "../Battle/StatusEffectView.h"
#include "../Renderer/NineSliceRenderer.h"
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
    float entryEasePower = 3.0f;
    float fadeStartAlpha = 0.0f;
    float panelAlpha = 0.84f;
    float targetPanelAlpha = 0.76f;
    float panelR = 1.0f;
    float panelG = 1.0f;
    float panelB = 1.0f;
    float goldR = 0.88f;
    float goldG = 0.72f;
    float goldB = 0.38f;
    float textR = 1.0f;
    float textG = 1.0f;
    float textB = 1.0f;
    float mutedR = 0.78f;
    float mutedG = 0.76f;
    float mutedB = 0.70f;
    float disabledTextR = 0.50f;
    float disabledTextG = 0.50f;
    float disabledTextB = 0.54f;
    float costR = 0.55f;
    float costG = 0.70f;
    float costB = 1.0f;
    float warningR = 1.0f;
    float warningG = 0.48f;
    float warningB = 0.34f;
    float unselectedAccentR = 0.20f;
    float unselectedAccentG = 0.19f;
    float unselectedAccentB = 0.18f;
    float iconBackR = 0.0f;
    float iconBackG = 0.0f;
    float iconBackB = 0.0f;
    float iconBackAlpha = 0.35f;
    float disabledPanelAlphaScale = 0.58f;
    float cardX = 326.0f;
    float cardY = 374.0f;
    float cardWidth = 374.0f;
    float cardHeight = 54.0f;
    float cardSpacing = 8.0f;
    float cardSlideOffsetX = 72.0f;
    float selectedNudgeX = 8.0f;
    float selectedAccentWidth = 5.0f;
    float selectedAccentInsetY = 7.0f;
    float selectedUnderlineHeight = 3.0f;
    float selectedUnderlineInsetX = 18.0f;
    float targetMarkerOffsetX = -30.0f;
    float targetMarkerOffsetY = 14.0f;
    float targetMarkerSize = 22.0f;
    float iconSize = 28.0f;
    float iconOffsetX = 22.0f;
    float iconOffsetY = 13.0f;
    float iconBackSize = 32.0f;
    float iconBackOffsetX = 19.0f;
    float iconBackOffsetY = 10.0f;
    float nameOffsetX = 66.0f;
    float nameOffsetY = 16.0f;
    float costOffsetX = 292.0f;
    float costOffsetY = 16.0f;
    float pageTextX = 600.0f;
    float pageTextY = 626.0f;
    float detailX = 716.0f;
    float detailY = 374.0f;
    float detailWidth = 408.0f;
    float detailHeight = 170.0f;
    float detailTitleOffsetX = 24.0f;
    float detailTitleOffsetY = 18.0f;
    float detailBodyOffsetX = 24.0f;
    float detailBodyOffsetY = 54.0f;
    float detailLineSpacing = 22.0f;
    float detailAccentHeight = 3.0f;
    float detailStatusMetaOffsetX = 132.0f;
    float detailStatusMetaOffsetY = 30.0f;
    float detailStatusSummaryOffsetY = 22.0f;
    float statusIconX = 1020.0f;
    float statusIconY = 468.0f;
    float targetDetailX = 716.0f;
    float targetDetailY = 552.0f;
    float targetDetailWidth = 408.0f;
    float targetDetailHeight = 84.0f;
    float targetTitleOffsetY = 16.0f;
    float targetEffectLabelOffsetY = 42.0f;
    float targetEffectIconOffsetY = 48.0f;
    float targetEffectIconSpacing = 6.0f;
    int targetMaxIcons = 6;
    int cardNameMaxBytes = 30;
    int descriptionMaxBytes = 72;
    int statusSummaryMaxBytes = 54;
    bool transformEnabled = true;
    bool transformFollowCameraRotation = true;
    float transformCameraRotationMultiplier = -1.0f;
    float transformRotationDegrees = -2.0f;
    float transformPivotX = 640.0f;
    float transformPivotY = 360.0f;
    float transformScaleX = 1.0f;
    float transformScaleY = 1.0f;
    float transformOffsetX = 0.0f;
    float transformOffsetY = 0.0f;
    bool anchorToActiveCharacter = true;
    float anchorReferenceX = 640.0f;
    float anchorReferenceY = 360.0f;
    float anchorOffsetX = 0.0f;
    float anchorOffsetY = 0.0f;
    bool clampAnchorOffset = true;
    float anchorMinOffsetX = -240.0f;
    float anchorMaxOffsetX = 160.0f;
    float anchorMinOffsetY = -160.0f;
    float anchorMaxOffsetY = 140.0f;
    bool hideDetailPanelsDuringTargetSelect = true;
    float textScale = 0.54f;
    float smallTextScale = 0.36f;
    float detailTextScale = 0.38f;
    float sliceScale = 0.44f;
    float detailSliceScale = 0.52f;
    float listDimAlpha = 0.64f;
    float selectedAlpha = 1.0f;
    std::string panelTexturePath = "assets/UI/ui-dialog-box-hd.png";
    std::string panelMetadataPath = "assets/UI/ui-dialog-box-hd.json";
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
                const BattleContext& battleContext,
                float cameraRotationRadians,
                bool hasActiveAnchor,
                float activeAnchorScreenX,
                float activeAnchorScreenY);

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
    NineSliceRenderer mPanelRenderer;
    std::unordered_map<std::string, Frame> mIconFrames;
    BattleSkillMenuLayout mLayout;
    int mScreenW = 1280;
    int mScreenH = 720;
    float mTimer = 0.0f;

    void BindViewport(ID3D11DeviceContext* context);
    bool LoadLayout(const std::string& path);
    bool LoadIconMetadata(const std::string& path);
    bool CreateFillTexture(ID3D11Device* device);
    DirectX::XMMATRIX BuildUiTransform(float cameraRotationRadians) const;
    DirectX::XMFLOAT2 ComputeAnchorOffset(DirectX::CXMMATRIX transform,
                                          bool hasActiveAnchor,
                                          float activeAnchorScreenX,
                                          float activeAnchorScreenY) const;
    void DrawPanel(float x, float y, float w, float h, DirectX::XMVECTOR color);
    void DrawIcon(const std::string& iconId, float x, float y, float size, DirectX::XMVECTOR color);
    void DrawNineSlice(ID3D11DeviceContext* context,
                       float x,
                       float y,
                       float w,
                       float h,
                       float sliceScale,
                       DirectX::CXMMATRIX transform,
                       DirectX::FXMVECTOR color);
    void DrawTextLine(BattleTextRenderer& text,
                      const std::string& value,
                      float x,
                      float y,
                      DirectX::FXMVECTOR color,
                      float scale) const;
    std::string CostText(const ISkill& skill) const;
    std::string TargetText(SkillTargeting targeting) const;
    std::string DamageTypeText(const ISkill& skill) const;
    std::string DamageGradeText(const ISkill& skill) const;
    std::string AvailabilityText(const ISkill& skill, const IBattler& caster, const BattleContext& ctx) const;
    std::string TruncateForCard(const std::string& text, std::size_t maxBytes) const;
};
