// ============================================================
// File: BattleSkillMenuRenderer.cpp
// Responsibility: Render Expedition-inspired skill cards and details.
// ============================================================
#define NOMINMAX
#include "BattleSkillMenuRenderer.h"
#include "BattleTextRenderer.h"
#include "../Battle/BattleContext.h"
#include "../Battle/IBattler.h"
#include "../Battle/ISkill.h"
#include "../Battle/PlayerCombatant.h"
#include "../Battle/StatusEffectRegistry.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
    std::string StripQuotes(const std::string& value)
    {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            return value.substr(1, value.size() - 2);
        return value;
    }

    XMVECTOR WithAlpha(FXMVECTOR color, float alpha)
    {
        return XMVectorSetW(color, alpha);
    }

    int CeilDiv(int value, int divisor)
    {
        if (divisor <= 0) return 1;
        return (value + divisor - 1) / divisor;
    }
}

bool BattleSkillMenuRenderer::Initialize(ID3D11Device* device,
                                         ID3D11DeviceContext* context,
                                         const std::string& layoutPath,
                                         const std::wstring& iconAtlasPath,
                                         const std::string& iconMetadataPath,
                                         int screenW,
                                         int screenH)
{
    Shutdown();
    mScreenW = screenW;
    mScreenH = screenH;
    LoadLayout(layoutPath);
    LoadIconMetadata(iconMetadataPath);

    HRESULT hr = CreateWICTextureFromFileEx(
        device,
        context,
        iconAtlasPath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mIconAtlasSRV.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[BattleSkillMenuRenderer] Failed to load icon atlas (0x%08X).", (unsigned)hr);
        return false;
    }

    if (!CreateFillTexture(device)) return false;
    std::wstring panelTexture(mLayout.panelTexturePath.begin(), mLayout.panelTexturePath.end());
    if (!mPanelRenderer.Initialize(
        device,
        context,
        panelTexture,
        mLayout.panelMetadataPath,
        screenW,
        screenH))
    {
        LOG("[BattleSkillMenuRenderer] Failed to initialize 9-slice skill panel.");
        return false;
    }

    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates = std::make_unique<CommonStates>(device);
    return true;
}

void BattleSkillMenuRenderer::Update(float dt, bool visible)
{
    if (visible)
    {
        mTimer += dt;
    }
    else
    {
        mTimer = 0.0f;
    }
}

void BattleSkillMenuRenderer::Render(ID3D11DeviceContext* context,
                                     BattleTextRenderer& text,
                                     const PlayerCombatant* activePlayer,
                                     int selectedSkillIndex,
                                     bool targetSelectActive,
                                     int targetIndex,
                                     const std::vector<IBattler*>& enemies,
                                     const BattleContext& battleContext)
{
    if (!IsInitialized() || !activePlayer || !text.IsReady()) return;

    const int skillCount = activePlayer->GetSkillCount();
    if (skillCount <= 0) return;

    const int pageSize = std::max(1, mLayout.pageSize);
    const int pageCount = CeilDiv(skillCount, pageSize);
    const int pageIndex = std::max(0, std::min(selectedSkillIndex / pageSize, pageCount - 1));
    const int first = pageIndex * pageSize;
    const int last = std::min(skillCount, first + pageSize);

    const float progress = mLayout.entryDuration > 0.0f
        ? std::min(mTimer / mLayout.entryDuration, 1.0f)
        : 1.0f;
    const float ease = 1.0f - std::pow(1.0f - progress, mLayout.entryEasePower);
    const float alpha = mLayout.fadeStartAlpha + (1.0f - mLayout.fadeStartAlpha) * ease;

    const XMVECTOR panelTint = XMVectorSet(mLayout.panelR, mLayout.panelG, mLayout.panelB, 1.0f);
    const XMVECTOR gold = XMVectorSet(mLayout.goldR, mLayout.goldG, mLayout.goldB, alpha);
    const XMVECTOR white = XMVectorSet(mLayout.textR, mLayout.textG, mLayout.textB, alpha);
    const XMVECTOR muted = XMVectorSet(mLayout.mutedR, mLayout.mutedG, mLayout.mutedB, alpha);
    const XMVECTOR disabledText = XMVectorSet(mLayout.disabledTextR, mLayout.disabledTextG, mLayout.disabledTextB, alpha);
    const XMVECTOR costText = XMVectorSet(mLayout.costR, mLayout.costG, mLayout.costB, alpha);
    const XMVECTOR warningText = XMVectorSet(mLayout.warningR, mLayout.warningG, mLayout.warningB, alpha);
    const XMMATRIX uiTransform = BuildUiTransform();

    const ISkill* selectedSkill = activePlayer->GetSkill(selectedSkillIndex);
    const IBattler* previewTarget = nullptr;
    if (selectedSkill)
    {
        if (targetSelectActive && !enemies.empty())
        {
            const int safeIndex = std::max(0, std::min(targetIndex, static_cast<int>(enemies.size()) - 1));
            previewTarget = enemies[safeIndex];
        }
        else if (selectedSkill->GetTargeting() == SkillTargeting::Self)
        {
            previewTarget = activePlayer;
        }
    }

    for (int i = first; i < last; ++i)
    {
        const ISkill* skill = activePlayer->GetSkill(i);
        if (!skill) continue;

        const int row = i - first;
        const bool selected = i == selectedSkillIndex;
        const bool canUse = skill->CanUse(*activePlayer, battleContext);
        const float rowAlpha = (selected ? mLayout.selectedAlpha : mLayout.listDimAlpha) * alpha;
        const float x = mLayout.cardX - (1.0f - ease) * mLayout.cardSlideOffsetX +
            (selected ? mLayout.selectedNudgeX : 0.0f);
        const float y = mLayout.cardY + row * (mLayout.cardHeight + mLayout.cardSpacing);

        DrawNineSlice(
            context,
            x,
            y,
            mLayout.cardWidth,
            mLayout.cardHeight,
            mLayout.sliceScale,
            uiTransform,
            WithAlpha(panelTint, canUse ? rowAlpha : rowAlpha * mLayout.disabledPanelAlphaScale));
    }

    if (selectedSkill)
    {
        DrawNineSlice(
            context,
            mLayout.detailX,
            mLayout.detailY,
            mLayout.detailWidth,
            mLayout.detailHeight,
            mLayout.detailSliceScale,
            uiTransform,
            WithAlpha(panelTint, mLayout.panelAlpha * alpha));
        DrawNineSlice(
            context,
            mLayout.targetDetailX,
            mLayout.targetDetailY,
            mLayout.targetDetailWidth,
            mLayout.targetDetailHeight,
            mLayout.detailSliceScale,
            uiTransform,
            WithAlpha(panelTint, mLayout.targetPanelAlpha * alpha));
    }

    BindViewport(context);
    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->PointClamp(),
        mStates->DepthNone(),
        nullptr,
        nullptr,
        uiTransform);
    for (int i = first; i < last; ++i)
    {
        const ISkill* skill = activePlayer->GetSkill(i);
        if (!skill) continue;

        const int row = i - first;
        const bool selected = i == selectedSkillIndex;
        const bool canUse = skill->CanUse(*activePlayer, battleContext);
        const float x = mLayout.cardX - (1.0f - ease) * mLayout.cardSlideOffsetX +
            (selected ? mLayout.selectedNudgeX : 0.0f);
        const float y = mLayout.cardY + row * (mLayout.cardHeight + mLayout.cardSpacing);

        DrawPanel(x + mLayout.iconBackOffsetX,
                  y + mLayout.iconBackOffsetY,
                  mLayout.iconBackSize,
                  mLayout.iconBackSize,
                  XMVectorSet(mLayout.iconBackR, mLayout.iconBackG, mLayout.iconBackB, mLayout.iconBackAlpha * alpha));

        DrawPanel(x + mLayout.selectedAccentInsetY,
                  y + mLayout.selectedAccentInsetY,
                  mLayout.selectedAccentWidth,
                  mLayout.cardHeight - 2.0f * mLayout.selectedAccentInsetY,
                  selected ? gold : XMVectorSet(mLayout.unselectedAccentR, mLayout.unselectedAccentG, mLayout.unselectedAccentB, alpha));

        if (selected)
        {
            DrawPanel(x + mLayout.selectedUnderlineInsetX,
                      y + mLayout.cardHeight - mLayout.selectedAccentInsetY,
                      mLayout.cardWidth - 2.0f * mLayout.selectedUnderlineInsetX,
                      mLayout.selectedUnderlineHeight,
                      gold);
        }

        DrawIcon(skill->GetIconId().empty() ? "fallback" : skill->GetIconId(),
                 x + mLayout.iconOffsetX,
                 y + mLayout.iconOffsetY,
                 mLayout.iconSize,
                 canUse ? white : disabledText);

        if (targetSelectActive && selected)
        {
            DrawPanel(x + mLayout.targetMarkerOffsetX,
                      y + mLayout.targetMarkerOffsetY,
                      mLayout.targetMarkerSize,
                      mLayout.targetMarkerSize,
                      gold);
        }
    }

    if (selectedSkill)
    {
        DrawPanel(mLayout.detailX,
                  mLayout.detailY,
                  mLayout.detailWidth,
                  mLayout.detailAccentHeight,
                  gold);

        if (!selectedSkill->GetStatusEffectId().empty())
        {
            StatusEffectRegistry::Get().EnsureLoaded();
            if (const StatusEffectData* status = StatusEffectRegistry::Get().Find(selectedSkill->GetStatusEffectId()))
            {
                DrawIcon(status->iconId, mLayout.statusIconX, mLayout.statusIconY, mLayout.iconSize, white);
            }
        }

        if (previewTarget)
        {
            const auto effects = previewTarget->GetStatusEffectViews();
            const int visibleCount = std::min(static_cast<int>(effects.size()), mLayout.targetMaxIcons);
            for (int i = 0; i < visibleCount; ++i)
            {
                DrawIcon(
                    effects[i].iconId.empty() ? "fallback" : effects[i].iconId,
                    mLayout.targetDetailX + mLayout.detailBodyOffsetX +
                        static_cast<float>(i) * (mLayout.iconSize + mLayout.targetEffectIconSpacing),
                    mLayout.targetDetailY + mLayout.targetEffectIconOffsetY,
                    mLayout.iconSize,
                    white);
            }
        }
    }

    mSpriteBatch->End();

    text.BeginBatch(context, uiTransform);
    for (int i = first; i < last; ++i)
    {
        const ISkill* skill = activePlayer->GetSkill(i);
        if (!skill) continue;

        const int row = i - first;
        const bool selected = i == selectedSkillIndex;
        const bool canUse = skill->CanUse(*activePlayer, battleContext);
        const float x = mLayout.cardX - (1.0f - ease) * mLayout.cardSlideOffsetX +
            (selected ? mLayout.selectedNudgeX : 0.0f);
        const float y = mLayout.cardY + row * (mLayout.cardHeight + mLayout.cardSpacing);
        const XMVECTOR nameColor = selected ? gold : (canUse ? white : disabledText);
        const XMVECTOR rowCostColor = canUse ? costText : disabledText;

        DrawTextLine(text,
                     TruncateForCard(skill->GetName(), static_cast<std::size_t>(mLayout.cardNameMaxBytes)),
                     x + mLayout.nameOffsetX,
                     y + mLayout.nameOffsetY,
                     nameColor,
                     mLayout.textScale);
        DrawTextLine(text,
                     CostText(*skill),
                     x + mLayout.costOffsetX,
                     y + mLayout.costOffsetY,
                     rowCostColor,
                     mLayout.textScale);
    }

    if (selectedSkill)
    {
        DrawTextLine(text,
                     selectedSkill->GetName(),
                     mLayout.detailX + mLayout.detailTitleOffsetX,
                     mLayout.detailY + mLayout.detailTitleOffsetY,
                     gold,
                     mLayout.textScale);

        DrawTextLine(text,
                     TruncateForCard(selectedSkill->GetDescription(), static_cast<std::size_t>(mLayout.descriptionMaxBytes)),
                     mLayout.detailX + mLayout.detailBodyOffsetX,
                     mLayout.detailY + mLayout.detailBodyOffsetY,
                     white,
                     mLayout.detailTextScale);

        float lineY = mLayout.detailY + mLayout.detailBodyOffsetY + mLayout.detailLineSpacing;
        const std::string targetLine = LocalizationManager::Get().Format("battle.skill_ui.target", {
            { "target", TargetText(selectedSkill->GetTargeting()) }
        });
        DrawTextLine(text, targetLine, mLayout.detailX + mLayout.detailBodyOffsetX, lineY, muted, mLayout.detailTextScale);
        lineY += mLayout.detailLineSpacing;

        const std::string damageLine = LocalizationManager::Get().Format("battle.skill_ui.damage", {
            { "damage", DamageGradeText(*selectedSkill) },
            { "hits", selectedSkill->GetHitCount() > 0
                ? std::to_string(selectedSkill->GetHitCount())
                : std::string("-") }
        });
        DrawTextLine(text, damageLine, mLayout.detailX + mLayout.detailBodyOffsetX, lineY, muted, mLayout.detailTextScale);
        lineY += mLayout.detailLineSpacing;

        const std::string availability = AvailabilityText(*selectedSkill, *activePlayer, battleContext);
        if (!availability.empty())
        {
            DrawTextLine(text, availability, mLayout.detailX + mLayout.detailBodyOffsetX, lineY, warningText, mLayout.detailTextScale);
            lineY += mLayout.detailLineSpacing;
        }

        if (!selectedSkill->GetStatusEffectId().empty())
        {
            StatusEffectRegistry::Get().EnsureLoaded();
            if (const StatusEffectData* status = StatusEffectRegistry::Get().Find(selectedSkill->GetStatusEffectId()))
            {
                const std::string statusName = LocalizationManager::Get().TextOrFallback(status->nameKey, status->id);
                const std::string appliesLine = LocalizationManager::Get().Format("battle.skill_ui.applies", {
                    { "status", statusName }
                });
                const std::string metaLine = LocalizationManager::Get().Format("battle.skill_ui.status_meta", {
                    { "turns", std::to_string(status->durationTurns) },
                    { "stacks", std::to_string(status->maxStacks) }
                });
                DrawTextLine(text,
                             appliesLine,
                             mLayout.detailX + mLayout.detailBodyOffsetX,
                             lineY,
                             gold,
                             mLayout.detailTextScale);
                DrawTextLine(text,
                             metaLine,
                             mLayout.statusIconX - mLayout.detailStatusMetaOffsetX,
                             mLayout.statusIconY + mLayout.detailStatusMetaOffsetY,
                             muted,
                             mLayout.smallTextScale);
                const std::string summaryKey = status->shortDescriptionKey.empty()
                    ? status->descriptionKey
                    : status->shortDescriptionKey;
                const std::string summary = TruncateForCard(
                    LocalizationManager::Get().TextOrFallback(summaryKey, status->id),
                    static_cast<std::size_t>(mLayout.statusSummaryMaxBytes));
                DrawTextLine(text,
                             summary,
                             mLayout.detailX + mLayout.detailBodyOffsetX,
                             lineY + mLayout.detailStatusSummaryOffsetY,
                             muted,
                             mLayout.smallTextScale);
            }
        }

        std::string pageText = LocalizationManager::Get().Format("battle.skill_ui.page", {
            { "page", std::to_string(pageIndex + 1) },
            { "pages", std::to_string(pageCount) }
        });
        DrawTextLine(text, pageText, mLayout.pageTextX, mLayout.pageTextY, muted, mLayout.smallTextScale);

        const std::string targetName = previewTarget
            ? previewTarget->GetName()
            : LocalizationManager::Get().Text("battle.skill_ui.no_target");
        const std::string previewLine = LocalizationManager::Get().Format("battle.skill_ui.preview_target", {
            { "target", targetName }
        });
        DrawTextLine(
            text,
            previewLine,
            mLayout.targetDetailX + mLayout.detailBodyOffsetX,
            mLayout.targetDetailY + mLayout.targetTitleOffsetY,
            white,
            mLayout.detailTextScale);

        if (previewTarget)
        {
            const auto effects = previewTarget->GetStatusEffectViews();
            const std::string effectLine = effects.empty()
                ? LocalizationManager::Get().Text("battle.skill_ui.no_effects")
                : LocalizationManager::Get().Text("battle.skill_ui.active_effects");
            DrawTextLine(
                text,
                effectLine,
                mLayout.targetDetailX + mLayout.detailBodyOffsetX,
                mLayout.targetDetailY + mLayout.targetEffectLabelOffsetY,
                muted,
                mLayout.detailTextScale);
        }
    }
    text.EndBatch();
}

void BattleSkillMenuRenderer::SetScreenSize(int w, int h)
{
    mScreenW = w;
    mScreenH = h;
    mPanelRenderer.SetScreenSize(w, h);
}

void BattleSkillMenuRenderer::Shutdown()
{
    mPanelRenderer.Shutdown();
    mSpriteBatch.reset();
    mStates.reset();
    mFillSRV.Reset();
    mIconAtlasSRV.Reset();
    mIconFrames.clear();
}

void BattleSkillMenuRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(mScreenW);
    vp.Height = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
    if (mSpriteBatch) mSpriteBatch->SetViewport(vp);
}

bool BattleSkillMenuRenderer::LoadLayout(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        LOG("[BattleSkillMenuRenderer] Cannot open layout '%s'. Using defaults.", path.c_str());
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();

    mLayout.pageSize = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "pageSize"), mLayout.pageSize);
    mLayout.entryDuration = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "entryDuration"), mLayout.entryDuration);
    mLayout.entryEasePower = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "entryEasePower"), mLayout.entryEasePower);
    mLayout.fadeStartAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "fadeStartAlpha"), mLayout.fadeStartAlpha);
    mLayout.panelAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "panelAlpha"), mLayout.panelAlpha);
    mLayout.targetPanelAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetPanelAlpha"), mLayout.targetPanelAlpha);
    mLayout.panelR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "panelR"), mLayout.panelR);
    mLayout.panelG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "panelG"), mLayout.panelG);
    mLayout.panelB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "panelB"), mLayout.panelB);
    mLayout.goldR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "goldR"), mLayout.goldR);
    mLayout.goldG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "goldG"), mLayout.goldG);
    mLayout.goldB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "goldB"), mLayout.goldB);
    mLayout.textR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "textR"), mLayout.textR);
    mLayout.textG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "textG"), mLayout.textG);
    mLayout.textB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "textB"), mLayout.textB);
    mLayout.mutedR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "mutedR"), mLayout.mutedR);
    mLayout.mutedG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "mutedG"), mLayout.mutedG);
    mLayout.mutedB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "mutedB"), mLayout.mutedB);
    mLayout.disabledTextR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "disabledTextR"), mLayout.disabledTextR);
    mLayout.disabledTextG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "disabledTextG"), mLayout.disabledTextG);
    mLayout.disabledTextB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "disabledTextB"), mLayout.disabledTextB);
    mLayout.costR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "costR"), mLayout.costR);
    mLayout.costG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "costG"), mLayout.costG);
    mLayout.costB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "costB"), mLayout.costB);
    mLayout.warningR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "warningR"), mLayout.warningR);
    mLayout.warningG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "warningG"), mLayout.warningG);
    mLayout.warningB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "warningB"), mLayout.warningB);
    mLayout.unselectedAccentR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "unselectedAccentR"), mLayout.unselectedAccentR);
    mLayout.unselectedAccentG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "unselectedAccentG"), mLayout.unselectedAccentG);
    mLayout.unselectedAccentB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "unselectedAccentB"), mLayout.unselectedAccentB);
    mLayout.iconBackR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconBackR"), mLayout.iconBackR);
    mLayout.iconBackG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconBackG"), mLayout.iconBackG);
    mLayout.iconBackB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconBackB"), mLayout.iconBackB);
    mLayout.iconBackAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconBackAlpha"), mLayout.iconBackAlpha);
    mLayout.disabledPanelAlphaScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "disabledPanelAlphaScale"), mLayout.disabledPanelAlphaScale);
    mLayout.cardX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardX"), mLayout.cardX);
    mLayout.cardY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardY"), mLayout.cardY);
    mLayout.cardWidth = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardWidth"), mLayout.cardWidth);
    mLayout.cardHeight = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardHeight"), mLayout.cardHeight);
    mLayout.cardSpacing = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardSpacing"), mLayout.cardSpacing);
    mLayout.cardSlideOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardSlideOffsetX"), mLayout.cardSlideOffsetX);
    mLayout.selectedNudgeX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "selectedNudgeX"), mLayout.selectedNudgeX);
    mLayout.selectedAccentWidth = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "selectedAccentWidth"), mLayout.selectedAccentWidth);
    mLayout.selectedAccentInsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "selectedAccentInsetY"), mLayout.selectedAccentInsetY);
    mLayout.selectedUnderlineHeight = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "selectedUnderlineHeight"), mLayout.selectedUnderlineHeight);
    mLayout.selectedUnderlineInsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "selectedUnderlineInsetX"), mLayout.selectedUnderlineInsetX);
    mLayout.targetMarkerOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetMarkerOffsetX"), mLayout.targetMarkerOffsetX);
    mLayout.targetMarkerOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetMarkerOffsetY"), mLayout.targetMarkerOffsetY);
    mLayout.targetMarkerSize = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetMarkerSize"), mLayout.targetMarkerSize);
    mLayout.iconSize = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconSize"), mLayout.iconSize);
    mLayout.iconOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconOffsetX"), mLayout.iconOffsetX);
    mLayout.iconOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconOffsetY"), mLayout.iconOffsetY);
    mLayout.iconBackSize = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconBackSize"), mLayout.iconBackSize);
    mLayout.iconBackOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconBackOffsetX"), mLayout.iconBackOffsetX);
    mLayout.iconBackOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconBackOffsetY"), mLayout.iconBackOffsetY);
    mLayout.nameOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "nameOffsetX"), mLayout.nameOffsetX);
    mLayout.nameOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "nameOffsetY"), mLayout.nameOffsetY);
    mLayout.costOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "costOffsetX"), mLayout.costOffsetX);
    mLayout.costOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "costOffsetY"), mLayout.costOffsetY);
    mLayout.pageTextX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "pageTextX"), mLayout.pageTextX);
    mLayout.pageTextY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "pageTextY"), mLayout.pageTextY);
    mLayout.detailX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailX"), mLayout.detailX);
    mLayout.detailY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailY"), mLayout.detailY);
    mLayout.detailWidth = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailWidth"), mLayout.detailWidth);
    mLayout.detailHeight = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailHeight"), mLayout.detailHeight);
    mLayout.detailTitleOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailTitleOffsetX"), mLayout.detailTitleOffsetX);
    mLayout.detailTitleOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailTitleOffsetY"), mLayout.detailTitleOffsetY);
    mLayout.detailBodyOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailBodyOffsetX"), mLayout.detailBodyOffsetX);
    mLayout.detailBodyOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailBodyOffsetY"), mLayout.detailBodyOffsetY);
    mLayout.detailLineSpacing = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailLineSpacing"), mLayout.detailLineSpacing);
    mLayout.detailAccentHeight = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailAccentHeight"), mLayout.detailAccentHeight);
    mLayout.detailStatusMetaOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailStatusMetaOffsetX"), mLayout.detailStatusMetaOffsetX);
    mLayout.detailStatusMetaOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailStatusMetaOffsetY"), mLayout.detailStatusMetaOffsetY);
    mLayout.detailStatusSummaryOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailStatusSummaryOffsetY"), mLayout.detailStatusSummaryOffsetY);
    mLayout.statusIconX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "statusIconX"), mLayout.statusIconX);
    mLayout.statusIconY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "statusIconY"), mLayout.statusIconY);
    mLayout.targetDetailX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailX"), mLayout.targetDetailX);
    mLayout.targetDetailY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailY"), mLayout.targetDetailY);
    mLayout.targetDetailWidth = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailWidth"), mLayout.targetDetailWidth);
    mLayout.targetDetailHeight = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailHeight"), mLayout.targetDetailHeight);
    mLayout.targetTitleOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetTitleOffsetY"), mLayout.targetTitleOffsetY);
    mLayout.targetEffectLabelOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetEffectLabelOffsetY"), mLayout.targetEffectLabelOffsetY);
    mLayout.targetEffectIconOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetEffectIconOffsetY"), mLayout.targetEffectIconOffsetY);
    mLayout.targetEffectIconSpacing = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetEffectIconSpacing"), mLayout.targetEffectIconSpacing);
    mLayout.targetMaxIcons = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "targetMaxIcons"), mLayout.targetMaxIcons);
    mLayout.cardNameMaxBytes = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "cardNameMaxBytes"), mLayout.cardNameMaxBytes);
    mLayout.descriptionMaxBytes = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "descriptionMaxBytes"), mLayout.descriptionMaxBytes);
    mLayout.statusSummaryMaxBytes = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "statusSummaryMaxBytes"), mLayout.statusSummaryMaxBytes);
    mLayout.transformEnabled = JsonLoader::detail::ParseBool(JsonLoader::detail::ValueOf(src, "transformEnabled"), mLayout.transformEnabled);
    mLayout.transformRotationDegrees = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "transformRotationDegrees"), mLayout.transformRotationDegrees);
    mLayout.transformPivotX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "transformPivotX"), mLayout.transformPivotX);
    mLayout.transformPivotY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "transformPivotY"), mLayout.transformPivotY);
    mLayout.transformScaleX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "transformScaleX"), mLayout.transformScaleX);
    mLayout.transformScaleY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "transformScaleY"), mLayout.transformScaleY);
    mLayout.transformOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "transformOffsetX"), mLayout.transformOffsetX);
    mLayout.transformOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "transformOffsetY"), mLayout.transformOffsetY);
    mLayout.textScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "textScale"), mLayout.textScale);
    mLayout.smallTextScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "smallTextScale"), mLayout.smallTextScale);
    mLayout.detailTextScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailTextScale"), mLayout.detailTextScale);
    mLayout.sliceScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "sliceScale"), mLayout.sliceScale);
    mLayout.detailSliceScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailSliceScale"), mLayout.detailSliceScale);
    mLayout.listDimAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "listDimAlpha"), mLayout.listDimAlpha);
    mLayout.selectedAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "selectedAlpha"), mLayout.selectedAlpha);
    const std::string panelTexturePath = JsonLoader::detail::CleanString(JsonLoader::detail::ValueOf(src, "panelTexturePath"));
    if (!panelTexturePath.empty()) mLayout.panelTexturePath = panelTexturePath;
    const std::string panelMetadataPath = JsonLoader::detail::CleanString(JsonLoader::detail::ValueOf(src, "panelMetadataPath"));
    if (!panelMetadataPath.empty()) mLayout.panelMetadataPath = panelMetadataPath;
    if (mLayout.pageSize < 1) mLayout.pageSize = 1;
    if (mLayout.entryEasePower <= 0.0f) mLayout.entryEasePower = 1.0f;
    if (mLayout.targetMaxIcons < 0) mLayout.targetMaxIcons = 0;
    if (std::abs(mLayout.transformScaleX) <= 0.001f) mLayout.transformScaleX = 1.0f;
    if (std::abs(mLayout.transformScaleY) <= 0.001f) mLayout.transformScaleY = 1.0f;
    return true;
}

bool BattleSkillMenuRenderer::LoadIconMetadata(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();

    const auto objects = JsonLoader::detail::ExtractObjectsFromArray(src, "icons");
    for (const std::string& obj : objects)
    {
        const std::string id = StripQuotes(JsonLoader::detail::ValueOf(obj, "id"));
        if (id.empty()) continue;

        Frame frame;
        frame.x = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(obj, "x"), 0);
        frame.y = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(obj, "y"), 0);
        frame.w = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(obj, "w"), 32);
        frame.h = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(obj, "h"), 32);
        mIconFrames[id] = frame;
    }
    return true;
}

bool BattleSkillMenuRenderer::CreateFillTexture(ID3D11Device* device)
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const UINT32 white = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = &white;
    init.SysMemPitch = sizeof(UINT32);

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&td, &init, tex.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(tex.Get(), &srv, mFillSRV.GetAddressOf());
    return SUCCEEDED(hr);
}

void BattleSkillMenuRenderer::DrawPanel(float x, float y, float w, float h, XMVECTOR color)
{
    const XMFLOAT2 origin(0.0f, 0.0f);
    mSpriteBatch->Draw(mFillSRV.Get(), XMFLOAT2(x, y), nullptr, color, 0.0f, origin, XMFLOAT2(w, h));
}

XMMATRIX BattleSkillMenuRenderer::BuildUiTransform() const
{
    if (!mLayout.transformEnabled)
    {
        return XMMatrixIdentity();
    }

    const XMVECTOR scaleOrigin = XMVectorSet(mLayout.transformPivotX, mLayout.transformPivotY, 0.0f, 0.0f);
    const XMVECTOR rotationOrigin = scaleOrigin;
    const XMVECTOR scale = XMVectorSet(mLayout.transformScaleX, mLayout.transformScaleY, 0.0f, 0.0f);
    const XMVECTOR translation = XMVectorSet(mLayout.transformOffsetX, mLayout.transformOffsetY, 0.0f, 0.0f);
    const float radians = XMConvertToRadians(mLayout.transformRotationDegrees);
    return XMMatrixTransformation2D(scaleOrigin, 0.0f, scale, rotationOrigin, radians, translation);
}

void BattleSkillMenuRenderer::DrawNineSlice(ID3D11DeviceContext* context,
                                            float x,
                                            float y,
                                            float w,
                                            float h,
                                            float sliceScale,
                                            CXMMATRIX transform,
                                            FXMVECTOR color)
{
    mPanelRenderer.Draw(context, x, y, w, h, sliceScale, transform, color);
}

void BattleSkillMenuRenderer::DrawTextLine(BattleTextRenderer& text,
                                           const std::string& value,
                                           float x,
                                           float y,
                                           FXMVECTOR color,
                                           float scale) const
{
    text.DrawStringRawScaled(value.c_str(), x, y, color, scale, true);
}

void BattleSkillMenuRenderer::DrawIcon(const std::string& iconId, float x, float y, float size, XMVECTOR color)
{
    if (!mIconAtlasSRV) return;
    auto it = mIconFrames.find(iconId);
    if (it == mIconFrames.end()) it = mIconFrames.find("fallback");
    if (it == mIconFrames.end()) return;

    const Frame& frame = it->second;
    RECT src = { frame.x, frame.y, frame.x + frame.w, frame.y + frame.h };
    const float scale = size / static_cast<float>(std::max(1, frame.w));
    mSpriteBatch->Draw(mIconAtlasSRV.Get(), XMFLOAT2(x, y), &src, color, 0.0f, XMFLOAT2(0.0f, 0.0f), scale);
}

std::string BattleSkillMenuRenderer::CostText(const ISkill& skill) const
{
    if (skill.GetResourceKind() == SkillResourceKind::MP)
    {
        return LocalizationManager::Get().Format("battle.skill_cost.mp", {
            { "cost", std::to_string(skill.GetMpCost()) }
        });
    }
    if (skill.GetResourceKind() == SkillResourceKind::Rage)
    {
        return LocalizationManager::Get().Text("battle.skill_cost.rage");
    }
    return LocalizationManager::Get().Text("battle.skill_cost.free");
}

std::string BattleSkillMenuRenderer::TargetText(SkillTargeting targeting) const
{
    switch (targeting)
    {
    case SkillTargeting::Self: return LocalizationManager::Get().Text("battle.skill_target.self");
    case SkillTargeting::SingleAlly: return LocalizationManager::Get().Text("battle.skill_target.single_ally");
    case SkillTargeting::AllAllies: return LocalizationManager::Get().Text("battle.skill_target.all_allies");
    case SkillTargeting::AllEnemies: return LocalizationManager::Get().Text("battle.skill_target.all_enemies");
    case SkillTargeting::SingleEnemy:
    default:
        return LocalizationManager::Get().Text("battle.skill_target.single_enemy");
    }
}

std::string BattleSkillMenuRenderer::DamageTypeText(const ISkill& skill) const
{
    const std::string type = skill.GetDamageType();
    if (type == "magical") return LocalizationManager::Get().Text("battle.damage_type.magical");
    if (type == "true") return LocalizationManager::Get().Text("battle.damage_type.true");
    if (skill.GetKind() == "support" || skill.GetKind() == "status")
        return LocalizationManager::Get().Text("battle.damage_type.none");
    return LocalizationManager::Get().Text("battle.damage_type.physical");
}

std::string BattleSkillMenuRenderer::DamageGradeText(const ISkill& skill) const
{
    std::string label = DamageTypeText(skill);
    const std::string gradeKey = skill.GetDamageGradeKey();
    if (!gradeKey.empty())
    {
        label += " - " + LocalizationManager::Get().TextOrFallback(gradeKey, gradeKey);
        return label;
    }

    const float multiplier = skill.GetSkillMultiplier();
    const bool hasDamage =
        skill.GetKind() == "attack" ||
        skill.GetKind() == "damage" ||
        skill.GetKind() == "rage";
    if (hasDamage && std::abs(multiplier - 1.0f) > 0.01f)
    {
        std::ostringstream stream;
        stream.setf(std::ios::fixed);
        stream.precision(2);
        stream << multiplier;
        label += " x" + stream.str();
    }
    return label;
}

std::string BattleSkillMenuRenderer::AvailabilityText(const ISkill& skill, const IBattler& caster, const BattleContext& ctx) const
{
    if (skill.CanUse(caster, ctx)) return std::string();
    if (skill.GetResourceKind() == SkillResourceKind::MP && caster.GetStats().mp < skill.GetMpCost())
    {
        return LocalizationManager::Get().Text("battle.skill_ui.not_enough_mp");
    }
    if (skill.GetResourceKind() == SkillResourceKind::Rage && !caster.GetStats().IsRageFull())
    {
        return LocalizationManager::Get().Text("battle.skill_ui.rage_not_full");
    }
    return LocalizationManager::Get().Text("battle.skill_ui.unavailable");
}

std::string BattleSkillMenuRenderer::TruncateForCard(const std::string& value, std::size_t maxBytes) const
{
    if (value.size() <= maxBytes) return value;
    if (maxBytes <= 3) return value.substr(0, maxBytes);

    std::size_t cut = maxBytes - 3;
    while (cut > 0 && (static_cast<unsigned char>(value[cut]) & 0xC0) == 0x80)
    {
        --cut;
    }
    return value.substr(0, cut) + "...";
}
