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
    const float ease = 1.0f - std::pow(1.0f - progress, 3.0f);
    const float alpha = mLayout.fadeStartAlpha + (1.0f - mLayout.fadeStartAlpha) * ease;

    BindViewport(context);
    mSpriteBatch->Begin(SpriteSortMode_Deferred, mStates->NonPremultiplied(), mStates->LinearClamp(), mStates->DepthNone());

    const XMVECTOR panel = XMVectorSet(0.02f, 0.018f, 0.018f, mLayout.panelAlpha * alpha);
    const XMVECTOR panelDim = XMVectorSet(0.02f, 0.018f, 0.018f, 0.46f * alpha);
    const XMVECTOR gold = XMVectorSet(0.78f, 0.62f, 0.32f, alpha);
    const XMVECTOR blue = XMVectorSet(0.44f, 0.62f, 1.0f, alpha);
    const XMVECTOR red = XMVectorSet(0.65f, 0.18f, 0.18f, alpha);

    for (int i = first; i < last; ++i)
    {
        const ISkill* skill = activePlayer->GetSkill(i);
        if (!skill) continue;

        const int row = i - first;
        const bool selected = i == selectedSkillIndex;
        const bool canUse = skill->CanUse(*activePlayer, battleContext);
        const float scale = selected ? mLayout.selectedScale : 1.0f;
        const float x = mLayout.cardX - (1.0f - ease) * 80.0f;
        const float y = mLayout.cardY + row * (mLayout.cardHeight + mLayout.cardSpacing);
        const float w = mLayout.cardWidth * scale;
        const float h = mLayout.cardHeight * scale;
        const float cx = x + w * 0.5f;
        const float cy = y + h * 0.5f;

        DrawRotatedPanel(cx, cy, w, h, mLayout.cardAngle, canUse ? panel : panelDim);
        DrawRotatedPanel(x + 6.0f, cy, 5.0f, h - 8.0f, mLayout.cardAngle, selected ? gold : XMVectorSet(0.28f, 0.26f, 0.23f, alpha));
        if (selected)
        {
            DrawRotatedPanel(cx, y + h - 4.0f, w - 16.0f, 3.0f, mLayout.cardAngle, gold);
        }

        DrawIcon(skill->GetIconId().empty() ? "fallback" : skill->GetIconId(),
                 x + mLayout.iconOffsetX,
                 y + mLayout.iconOffsetY,
                 mLayout.iconSize,
                 canUse ? Colors::White : XMVectorSet(0.55f, 0.55f, 0.60f, alpha));

        if (targetSelectActive && selected)
        {
            DrawRotatedPanel(x - 34.0f, cy, 24.0f, 24.0f, 0.0f, gold);
        }

        (void)blue;
        (void)red;
    }

    DrawPanel(mLayout.detailX, mLayout.detailY, mLayout.detailWidth, mLayout.detailHeight, XMVectorSet(0.015f, 0.014f, 0.014f, 0.78f * alpha));
    DrawPanel(mLayout.detailX, mLayout.detailY, mLayout.detailWidth, 3.0f, gold);
    DrawPanel(mLayout.targetDetailX, mLayout.targetDetailY, mLayout.targetDetailWidth, mLayout.targetDetailHeight, XMVectorSet(0.015f, 0.014f, 0.014f, 0.66f * alpha));

    mSpriteBatch->End();

    const ISkill* selectedSkill = activePlayer->GetSkill(selectedSkillIndex);
    const IBattler* previewTarget = nullptr;

    text.BeginBatch(context);
    for (int i = first; i < last; ++i)
    {
        const ISkill* skill = activePlayer->GetSkill(i);
        if (!skill) continue;

        const int row = i - first;
        const bool selected = i == selectedSkillIndex;
        const bool canUse = skill->CanUse(*activePlayer, battleContext);
        const float y = mLayout.cardY + row * (mLayout.cardHeight + mLayout.cardSpacing);
        XMVECTOR nameColor = canUse ? Colors::White : XMVectorSet(0.50f, 0.50f, 0.54f, alpha);
        if (selected) nameColor = canUse ? Colors::PaleGoldenrod : Colors::Orange;
        nameColor = WithAlpha(nameColor, alpha);

        text.DrawStringRawScaled(
            TruncateForCard(skill->GetName(), 26).c_str(),
            mLayout.cardX + mLayout.nameOffsetX,
            y + mLayout.nameOffsetY,
            nameColor,
            mLayout.textScale,
            true);

        text.DrawStringRawScaled(
            CostText(*skill).c_str(),
            mLayout.cardX + mLayout.costOffsetX,
            y + mLayout.costOffsetY,
            canUse ? XMVectorSet(0.55f, 0.70f, 1.0f, alpha) : XMVectorSet(0.42f, 0.42f, 0.48f, alpha),
            mLayout.textScale,
            true);
    }

    if (selectedSkill)
    {
        const XMVECTOR detailColor = WithAlpha(Colors::White, alpha);
        const XMVECTOR mutedColor = XMVectorSet(0.78f, 0.76f, 0.70f, alpha);
        const XMVECTOR goldColor = XMVectorSet(0.88f, 0.72f, 0.38f, alpha);

        text.DrawStringRawScaled(
            selectedSkill->GetName().c_str(),
            mLayout.detailX + mLayout.detailTitleOffsetX,
            mLayout.detailY + mLayout.detailTitleOffsetY,
            goldColor,
            mLayout.textScale,
            true);

        const std::string desc = TruncateForCard(selectedSkill->GetDescription(), 58);
        text.DrawStringRawScaled(
            desc.c_str(),
            mLayout.detailX + mLayout.detailBodyOffsetX,
            mLayout.detailY + mLayout.detailBodyOffsetY,
            detailColor,
            mLayout.detailTextScale,
            true);

        float lineY = mLayout.detailY + mLayout.detailBodyOffsetY + mLayout.detailLineSpacing;
        const std::string targetLine = LocalizationManager::Get().Format("battle.skill_ui.target", {
            { "target", TargetText(selectedSkill->GetTargeting()) }
        });
        text.DrawStringRawScaled(targetLine.c_str(), mLayout.detailX + mLayout.detailBodyOffsetX, lineY, mutedColor, mLayout.detailTextScale, true);
        lineY += mLayout.detailLineSpacing;

        const std::string damageLine = LocalizationManager::Get().Format("battle.skill_ui.damage", {
            { "damage", DamageGradeText(*selectedSkill) },
            { "hits", selectedSkill->GetHitCount() > 0
                ? std::to_string(selectedSkill->GetHitCount())
                : std::string("-") }
        });
        text.DrawStringRawScaled(damageLine.c_str(), mLayout.detailX + mLayout.detailBodyOffsetX, lineY, mutedColor, mLayout.detailTextScale, true);
        lineY += mLayout.detailLineSpacing;

        const std::string availability = AvailabilityText(*selectedSkill, *activePlayer, battleContext);
        if (!availability.empty())
        {
            text.DrawStringRawScaled(availability.c_str(), mLayout.detailX + mLayout.detailBodyOffsetX, lineY, XMVectorSet(1.0f, 0.48f, 0.34f, alpha), mLayout.detailTextScale, true);
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
                text.DrawStringRawScaled(appliesLine.c_str(),
                                         mLayout.detailX + mLayout.detailBodyOffsetX,
                                         lineY,
                                         goldColor,
                                         mLayout.detailTextScale,
                                         true);
                text.DrawStringRawScaled(metaLine.c_str(),
                                         mLayout.statusIconX - 132.0f,
                                         mLayout.statusIconY + 30.0f,
                                         mutedColor,
                                         mLayout.smallTextScale,
                                         true);
                const std::string summaryKey = status->shortDescriptionKey.empty()
                    ? status->descriptionKey
                    : status->shortDescriptionKey;
                const std::string summary = TruncateForCard(
                    LocalizationManager::Get().TextOrFallback(summaryKey, status->id),
                    46);
                text.DrawStringRawScaled(summary.c_str(),
                                         mLayout.detailX + mLayout.detailBodyOffsetX,
                                         lineY + mLayout.detailLineSpacing,
                                         mutedColor,
                                         mLayout.smallTextScale,
                                         true);
            }
        }

        std::string pageText = LocalizationManager::Get().Format("battle.skill_ui.page", {
            { "page", std::to_string(pageIndex + 1) },
            { "pages", std::to_string(pageCount) }
        });
        text.DrawStringRawScaled(pageText.c_str(), mLayout.pageTextX, mLayout.pageTextY, mutedColor, mLayout.smallTextScale, true);

        if (targetSelectActive && !enemies.empty())
        {
            const int safeIndex = std::max(0, std::min(targetIndex, static_cast<int>(enemies.size()) - 1));
            previewTarget = enemies[safeIndex];
        }
        if (!previewTarget && selectedSkill->GetTargeting() == SkillTargeting::Self)
        {
            previewTarget = activePlayer;
        }

        const std::string targetName = previewTarget
            ? previewTarget->GetName()
            : LocalizationManager::Get().Text("battle.skill_ui.no_target");
        text.DrawStringRawScaled(
            LocalizationManager::Get().Format("battle.skill_ui.preview_target", { { "target", targetName } }).c_str(),
            mLayout.targetDetailX + mLayout.detailBodyOffsetX,
            mLayout.targetDetailY + 18.0f,
            detailColor,
            mLayout.detailTextScale,
            true);

        if (previewTarget)
        {
            const auto effects = previewTarget->GetStatusEffectViews();
            std::string effectLine = effects.empty()
                ? LocalizationManager::Get().Text("battle.skill_ui.no_effects")
                : LocalizationManager::Get().Text("battle.skill_ui.active_effects");
            text.DrawStringRawScaled(
                effectLine.c_str(),
                mLayout.targetDetailX + mLayout.detailBodyOffsetX,
                mLayout.targetDetailY + 46.0f,
                mutedColor,
                mLayout.detailTextScale,
                true);
        }
    }
    text.EndBatch();

    BindViewport(context);
    mSpriteBatch->Begin(SpriteSortMode_Deferred, mStates->NonPremultiplied(), mStates->PointClamp(), mStates->DepthNone());

    if (selectedSkill && !selectedSkill->GetStatusEffectId().empty())
    {
        StatusEffectRegistry::Get().EnsureLoaded();
        if (const StatusEffectData* status = StatusEffectRegistry::Get().Find(selectedSkill->GetStatusEffectId()))
        {
            DrawIcon(status->iconId, mLayout.statusIconX, mLayout.statusIconY, mLayout.iconSize, Colors::White);
        }
    }

    if (previewTarget)
    {
        const auto effects = previewTarget->GetStatusEffectViews();
        for (int i = 0; i < static_cast<int>(effects.size()) && i < 6; ++i)
        {
            DrawIcon(
                effects[i].iconId.empty() ? "fallback" : effects[i].iconId,
                mLayout.targetDetailX + mLayout.detailBodyOffsetX + static_cast<float>(i) * (mLayout.iconSize + 6.0f),
                mLayout.targetDetailY + 70.0f,
                mLayout.iconSize,
                Colors::White);
        }
    }

    mSpriteBatch->End();
}

void BattleSkillMenuRenderer::SetScreenSize(int w, int h)
{
    mScreenW = w;
    mScreenH = h;
}

void BattleSkillMenuRenderer::Shutdown()
{
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
    mLayout.fadeStartAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "fadeStartAlpha"), mLayout.fadeStartAlpha);
    mLayout.panelAlpha = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "panelAlpha"), mLayout.panelAlpha);
    mLayout.cardX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardX"), mLayout.cardX);
    mLayout.cardY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardY"), mLayout.cardY);
    mLayout.cardWidth = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardWidth"), mLayout.cardWidth);
    mLayout.cardHeight = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardHeight"), mLayout.cardHeight);
    mLayout.cardSpacing = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardSpacing"), mLayout.cardSpacing);
    mLayout.cardAngle = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "cardAngle"), mLayout.cardAngle);
    mLayout.selectedScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "selectedScale"), mLayout.selectedScale);
    mLayout.iconSize = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconSize"), mLayout.iconSize);
    mLayout.iconOffsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconOffsetX"), mLayout.iconOffsetX);
    mLayout.iconOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconOffsetY"), mLayout.iconOffsetY);
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
    mLayout.statusIconX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "statusIconX"), mLayout.statusIconX);
    mLayout.statusIconY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "statusIconY"), mLayout.statusIconY);
    mLayout.targetDetailX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailX"), mLayout.targetDetailX);
    mLayout.targetDetailY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailY"), mLayout.targetDetailY);
    mLayout.targetDetailWidth = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailWidth"), mLayout.targetDetailWidth);
    mLayout.targetDetailHeight = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "targetDetailHeight"), mLayout.targetDetailHeight);
    mLayout.textScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "textScale"), mLayout.textScale);
    mLayout.smallTextScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "smallTextScale"), mLayout.smallTextScale);
    mLayout.detailTextScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "detailTextScale"), mLayout.detailTextScale);
    if (mLayout.pageSize < 1) mLayout.pageSize = 1;
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

void BattleSkillMenuRenderer::DrawRotatedPanel(float centerX, float centerY, float w, float h, float rotation, XMVECTOR color)
{
    const XMFLOAT2 origin(0.5f, 0.5f);
    mSpriteBatch->Draw(mFillSRV.Get(), XMFLOAT2(centerX, centerY), nullptr, color, rotation, origin, XMFLOAT2(w, h));
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
