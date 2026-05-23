// ============================================================
// File: BattleResultRenderer.cpp
// Responsibility: Implement the cinematic battle result overlay.
//
// Common mistakes:
//   1. Drawing result UI before the scene -> the battle backdrop covers it.
//   2. Reading rewards from Wallet here -> results change after saving or
//      retrying. BattleState owns the immutable snapshot.
//   3. Drawing text inside this renderer's SpriteBatch Begin/End -> nested
//      SpriteBatch calls throw in DirectXTK debug builds.
// ============================================================
#define NOMINMAX
#include "BattleResultRenderer.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/Log.h"
#include <WICTextureLoader.h>
#include <DirectXColors.h>
#include <algorithm>
#include <cstdio>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
    RECT MakeRect(float x, float y, float width, float height)
    {
        RECT rect{};
        rect.left = static_cast<LONG>(x);
        rect.top = static_cast<LONG>(y);
        rect.right = static_cast<LONG>(x + width);
        rect.bottom = static_cast<LONG>(y + height);
        return rect;
    }

    float Clamp01(float value)
    {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    }

    std::string FormatTime(float seconds)
    {
        const int total = static_cast<int>((std::max)(0.0f, seconds));
        const int minutes = total / 60;
        const int secs = total % 60;

        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, secs);
        return buffer;
    }
}

bool BattleResultRenderer::Initialize(ID3D11Device* device,
                                      ID3D11DeviceContext* context,
                                      const JsonLoader::BattleResultLayout& layout,
                                      int screenW,
                                      int screenH)
{
    Shutdown();

    if (!device || !context) return false;

    mLayout = layout;
    mScreenW = screenW;
    mScreenH = screenH;

    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates = std::make_unique<CommonStates>(device);
    if (!CreateFillTexture(device)) return false;

    LoadArtTexture(device, context, mLayout.defeatSigilTexturePath, mDefeatSigilSRV);
    LoadArtTexture(device, context, mLayout.promptPanelTexturePath, mPromptPanelSRV);
    LoadArtTexture(device, context, mLayout.vignetteTexturePath, mVignetteSRV);
    LoadArtTexture(device, context, mLayout.victoryFlourishTexturePath, mVictoryFlourishSRV);

    if (!mPromptPanel.Initialize(
        device,
        context,
        L"assets/UI/ui-dialog-box-hd.png",
        "assets/UI/ui-dialog-box-hd.json",
        screenW,
        screenH))
    {
        LOG("%s", "[BattleResultRenderer] WARNING: retry prompt panel failed to initialize.");
    }

    return true;
}

void BattleResultRenderer::Shutdown()
{
    mPortraits.clear();
    mPromptPanel.Shutdown();
    mDefeatSigilSRV.Reset();
    mPromptPanelSRV.Reset();
    mVignetteSRV.Reset();
    mVictoryFlourishSRV.Reset();
    mFillSRV.Reset();
    mStates.reset();
    mSpriteBatch.reset();
    mElapsed = 0.0f;
}

void BattleResultRenderer::SetScreenSize(int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
    mPromptPanel.SetScreenSize(screenW, screenH);
}

void BattleResultRenderer::Update(float dt)
{
    mElapsed += dt;
}

void BattleResultRenderer::LoadPortraits(ID3D11Device* device,
                                         ID3D11DeviceContext* context,
                                         const BattleResultData& data)
{
    mPortraits.clear();
    if (!device || !context) return;

    for (const BattleMemberResult& member : data.members)
    {
        if (member.portraitPath.empty()) continue;

        PortraitEntry entry{};
        entry.memberId = member.id;

        const HRESULT hr = CreateWICTextureFromFileEx(
            device,
            context,
            member.portraitPath.c_str(),
            0,
            D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE,
            0,
            0,
            WIC_LOADER_IGNORE_SRGB,
            nullptr,
            entry.srv.GetAddressOf());

        if (FAILED(hr))
        {
            LOG("[BattleResultRenderer] WARNING: portrait load failed for '%ls' (0x%08X).",
                member.portraitPath.c_str(),
                static_cast<unsigned>(hr));
            continue;
        }

        mPortraits.push_back(std::move(entry));
    }
}

void BattleResultRenderer::RenderVictory(ID3D11DeviceContext* context,
                                         BattleTextRenderer& text,
                                         const BattleResultData& data,
                                         float visibleSeconds)
{
    const float alpha = Clamp01(visibleSeconds / (std::max)(0.01f, mLayout.victoryEnterDuration));

    BeginRects(context);
    DrawBackdrop(alpha);
    DrawTextureRect(
        mVictoryFlourishSRV.Get(),
        mLayout.victoryFlourishX * (static_cast<float>(mScreenW) / 1280.0f),
        mLayout.victoryFlourishY * (static_cast<float>(mScreenH) / 720.0f),
        mLayout.victoryFlourishW * (static_cast<float>(mScreenW) / 1280.0f),
        mLayout.victoryFlourishH * (static_cast<float>(mScreenH) / 720.0f),
        XMVectorSet(1.0f, 1.0f, 1.0f, 0.86f * alpha));
    DrawVictoryPartyPanel(alpha);
    DrawVictoryPortraits(data, alpha);
    EndRects();

    DrawVictoryText(context, text, data, alpha);
}

void BattleResultRenderer::RenderDefeatSplash(ID3D11DeviceContext* context,
                                              BattleTextRenderer& text,
                                              float visibleSeconds)
{
    const float alpha = Clamp01(visibleSeconds / 0.55f);
    const float sx = static_cast<float>(mScreenW) / 1280.0f;
    const float sy = static_cast<float>(mScreenH) / 720.0f;
    const float ss = (std::min)(sx, sy);

    BeginRects(context);
    DrawBackdrop(alpha);
    DrawDefeatSigil(alpha);
    EndRects();

    text.BeginBatch(context);
    const std::string title = LocalizationManager::Get().Text("battle.result.defeat_title");
    text.DrawStringCenteredRaw(
        title.c_str(),
        mScreenW * 0.5f,
        292.0f * sy,
        XMVectorSet(0.82f, 0.12f, 0.12f, alpha),
        2.25f * ss,
        true);
    text.EndBatch();
}

void BattleResultRenderer::RenderDefeatPrompt(ID3D11DeviceContext* context,
                                              BattleTextRenderer& text,
                                              int selectedOption,
                                              float visibleSeconds)
{
    const float alpha = Clamp01(visibleSeconds / 0.35f);
    const float sx = static_cast<float>(mScreenW) / 1280.0f;
    const float sy = static_cast<float>(mScreenH) / 720.0f;
    const float ss = (std::min)(sx, sy);

    BeginRects(context);
    DrawBackdrop(1.0f);
    DrawDefeatSigil(0.42f);
    EndRects();

    const float panelX = mLayout.promptX * sx;
    const float panelY = mLayout.promptY * sy;
    const float panelW = mLayout.promptW * sx;
    const float panelH = mLayout.promptH * sy;

    if (mPromptPanelSRV.Get())
    {
        BeginRects(context);
        DrawTextureRect(
            mPromptPanelSRV.Get(),
            panelX,
            panelY,
            panelW,
            panelH,
            XMVectorSet(1.0f, 1.0f, 1.0f, alpha));
        EndRects();
    }
    else
    {
        mPromptPanel.Draw(
            context,
            panelX,
            panelY,
            panelW,
            panelH,
            0.35f * ss,
            XMMatrixIdentity(),
            XMVectorSet(0.95f, 0.95f, 0.95f, alpha));
    }

    BeginRects(context);
    const float optionY = panelY + panelH - 44.0f * sy;
    const float optionW = 112.0f * sx;
    const float optionH = 25.0f * sy;
    const float leftX = panelX + panelW * 0.5f - mLayout.promptOptionGap * 0.5f * sx - optionW * 0.5f;
    const float rightX = panelX + panelW * 0.5f + mLayout.promptOptionGap * 0.5f * sx - optionW * 0.5f;
    const float highlightX = (selectedOption == 0) ? leftX : rightX;
    DrawFillRect(highlightX, optionY - 4.0f * sy, optionW, optionH,
                 XMVectorSet(0.58f, 0.43f, 0.25f, 0.85f * alpha));
    EndRects();

    text.BeginBatch(context);
    text.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("battle.result.retry_question").c_str(),
        panelX + panelW * 0.5f,
        panelY + 24.0f * sy,
        XMVectorSet(0.96f, 0.92f, 0.84f, alpha),
        0.92f * ss,
        true);
    text.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("battle.result.retry_yes").c_str(),
        leftX + optionW * 0.5f,
        optionY,
        XMVectorSet(1.0f, 0.92f, 0.55f, alpha),
        0.65f * ss,
        true);
    text.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("battle.result.retry_no").c_str(),
        rightX + optionW * 0.5f,
        optionY,
        XMVectorSet(1.0f, 0.92f, 0.55f, alpha),
        0.65f * ss,
        true);
    text.EndBatch();
}

bool BattleResultRenderer::CreateFillTexture(ID3D11Device* device)
{
    const unsigned int white = 0xFFFFFFFFu;

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &white;
    init.SysMemPitch = sizeof(unsigned int);

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&textureDesc, &init, texture.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, mFillSRV.GetAddressOf());
    return SUCCEEDED(hr);
}

bool BattleResultRenderer::LoadArtTexture(ID3D11Device* device,
                                          ID3D11DeviceContext* context,
                                          const std::string& path,
                                          ComPtr<ID3D11ShaderResourceView>& outSrv)
{
    outSrv.Reset();
    if (!device || !context || path.empty()) return false;

    const std::wstring widePath(path.begin(), path.end());
    const HRESULT hr = CreateWICTextureFromFileEx(
        device,
        context,
        widePath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr,
        outSrv.GetAddressOf());

    if (FAILED(hr))
    {
        LOG("[BattleResultRenderer] WARNING: result art '%s' failed to load (0x%08X).",
            path.c_str(),
            static_cast<unsigned>(hr));
        return false;
    }

    return true;
}

void BattleResultRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(mScreenW);
    viewport.Height = static_cast<float>(mScreenH);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
}

void BattleResultRenderer::BeginRects(ID3D11DeviceContext* context)
{
    if (!mSpriteBatch || !mStates || !mFillSRV) return;

    BindViewport(context);
    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->PointClamp(),
        mStates->DepthNone());
}

void BattleResultRenderer::EndRects()
{
    if (!mSpriteBatch) return;
    mSpriteBatch->End();
}

void BattleResultRenderer::DrawFillRect(float x,
                                        float y,
                                        float width,
                                        float height,
                                        FXMVECTOR color)
{
    if (!mSpriteBatch || !mFillSRV) return;
    if (width <= 0.0f || height <= 0.0f) return;

    const RECT rect = MakeRect(x, y, width, height);
    mSpriteBatch->Draw(mFillSRV.Get(), rect, nullptr, color);
}

void BattleResultRenderer::DrawTextureRect(ID3D11ShaderResourceView* srv,
                                           float x,
                                           float y,
                                           float width,
                                           float height,
                                           FXMVECTOR color)
{
    if (!mSpriteBatch || !srv) return;
    if (width <= 0.0f || height <= 0.0f) return;

    const RECT rect = MakeRect(x, y, width, height);
    mSpriteBatch->Draw(srv, rect, nullptr, color);
}

void BattleResultRenderer::DrawDecorativeFrame(float x,
                                               float y,
                                               float width,
                                               float height,
                                               FXMVECTOR color)
{
    const float t = 2.0f;
    DrawFillRect(x, y, width, t, color);
    DrawFillRect(x, y + height, width, t, color);
    DrawFillRect(x, y, t, height, color);
    DrawFillRect(x + width, y, t, height, color);
}

void BattleResultRenderer::DrawBackdrop(float alphaMul)
{
    const float alpha = Clamp01(mLayout.scrimAlpha * alphaMul);
    DrawFillRect(0.0f, 0.0f, static_cast<float>(mScreenW), static_cast<float>(mScreenH),
                 XMVectorSet(0.0f, 0.0f, 0.0f, alpha));

    const float vignette = Clamp01(mLayout.vignetteAlpha * alphaMul);
    DrawFillRect(0.0f, 0.0f, static_cast<float>(mScreenW), 70.0f,
                 XMVectorSet(0.0f, 0.0f, 0.0f, vignette));
    DrawFillRect(0.0f, static_cast<float>(mScreenH) - 92.0f, static_cast<float>(mScreenW), 92.0f,
                 XMVectorSet(0.0f, 0.0f, 0.0f, vignette));
    DrawFillRect(0.0f, 0.0f, 130.0f, static_cast<float>(mScreenH),
                 XMVectorSet(0.0f, 0.0f, 0.0f, vignette));
    DrawFillRect(static_cast<float>(mScreenW) - 160.0f, 0.0f, 160.0f, static_cast<float>(mScreenH),
                 XMVectorSet(0.0f, 0.0f, 0.0f, vignette));

    DrawTextureRect(
        mVignetteSRV.Get(),
        0.0f,
        0.0f,
        static_cast<float>(mScreenW),
        static_cast<float>(mScreenH),
        XMVectorSet(1.0f, 1.0f, 1.0f, Clamp01(mLayout.vignetteTextureAlpha * alphaMul)));
}

void BattleResultRenderer::DrawDefeatSigil(float alpha)
{
    const float sx = static_cast<float>(mScreenW) / 1280.0f;
    const float sy = static_cast<float>(mScreenH) / 720.0f;
    const float centerX = mLayout.defeatSigilCenterX * sx;
    const float centerY = mLayout.defeatSigilCenterY * sy;
    const float width = mLayout.defeatSigilW * sx;
    const float height = mLayout.defeatSigilH * sy;

    if (mDefeatSigilSRV.Get())
    {
        DrawTextureRect(
            mDefeatSigilSRV.Get(),
            centerX - width * 0.5f,
            centerY - height * 0.5f,
            width,
            height,
            XMVectorSet(1.0f, 1.0f, 1.0f, alpha));
        return;
    }

    DrawDefeatGlyph(centerX, centerY, alpha);
}

void BattleResultRenderer::DrawVictoryPartyPanel(float alpha)
{
    const float sx = static_cast<float>(mScreenW) / 1280.0f;
    const float sy = static_cast<float>(mScreenH) / 720.0f;
    const float panelX = mLayout.partyPanelX * sx;
    const float panelY = mLayout.partyPanelY * sy;
    const float panelW = mLayout.partyPanelW * sx;
    const float panelH = mLayout.partyPanelH * sy;

    // The battle scene remains visible around the panel, but this fill
    // prevents large frozen enemies from reading as broken result UI art.
    DrawFillRect(panelX, panelY, panelW, panelH,
                 XMVectorSet(mLayout.partyPanelFillR,
                             mLayout.partyPanelFillG,
                             mLayout.partyPanelFillB,
                             Clamp01(mLayout.partyPanelFillAlpha * alpha)));
    DrawDecorativeFrame(panelX, panelY, panelW, panelH,
                        XMVectorSet(0.92f, 0.78f, 0.55f, mLayout.partyPanelFrameAlpha * alpha));
}

void BattleResultRenderer::DrawVictoryText(ID3D11DeviceContext* context,
                                           BattleTextRenderer& text,
                                           const BattleResultData& data,
                                           float alpha)
{
    const float sx = static_cast<float>(mScreenW) / 1280.0f;
    const float sy = static_cast<float>(mScreenH) / 720.0f;
    const float ss = (std::min)(sx, sy);

    auto value = [](int amount)
    {
        return std::to_string(amount);
    };

    text.BeginBatch(context);
    text.DrawStringRawScaled(
        LocalizationManager::Get().Text("battle.result.victory_title").c_str(),
        mLayout.titleX * sx,
        mLayout.titleY * sy,
        XMVectorSet(0.96f, 0.83f, 0.62f, alpha),
        mLayout.titleScale * ss,
        true);
    text.DrawStringRawScaled(
        LocalizationManager::Get().Text("battle.result.battle_loot").c_str(),
        mLayout.lootX * sx,
        mLayout.lootY * sy,
        XMVectorSet(0.94f, 0.83f, 0.64f, alpha),
        mLayout.subtitleScale * ss,
        true);
    text.DrawStringRawScaled(
        LocalizationManager::Get().Format("battle.result.exp", { { "amount", value(data.totalExp) } }).c_str(),
        (mLayout.lootX + 330.0f) * sx,
        mLayout.lootY * sy,
        XMVectorSet(1.0f, 0.96f, 0.86f, alpha),
        0.95f * ss,
        true);
    text.DrawStringRawScaled(
        LocalizationManager::Get().Format("battle.result.coins", { { "amount", value(data.totalCoins) } }).c_str(),
        (mLayout.lootX + 330.0f) * sx,
        (mLayout.lootY + 38.0f) * sy,
        XMVectorSet(1.0f, 0.88f, 0.42f, alpha),
        0.80f * ss,
        true);

    if (data.noDamage)
    {
        text.DrawStringRawScaled(
            LocalizationManager::Get().Format("battle.result.no_damage", {
                { "percent", value(mLayout.noDamageBonusPercent) }
            }).c_str(),
            (mLayout.lootX + 330.0f) * sx,
            (mLayout.lootY + 74.0f) * sy,
            XMVectorSet(1.0f, 0.78f, 0.48f, alpha),
            0.78f * ss,
            true);
    }

    const float statsX = mLayout.statsX * sx;
    float statsY = mLayout.statsY * sy;
    const float rowGap = mLayout.rowGap * sy;
    const XMVECTOR statColor = XMVectorSet(0.94f, 0.90f, 0.84f, alpha);

    const std::string stats[] = {
        LocalizationManager::Get().Format("battle.result.kills", { { "amount", value(data.kills) } }),
        LocalizationManager::Get().Format("battle.result.highest_damage", { { "amount", value(data.highestDamage) } }),
        LocalizationManager::Get().Format("battle.result.damage_dealt", { { "amount", value(data.totalDamageDealt) } }),
        LocalizationManager::Get().Format("battle.result.damage_received", { { "amount", value(data.totalDamageReceived) } }),
        LocalizationManager::Get().Format("battle.result.time", { { "time", FormatTime(data.battleSeconds) } }),
        LocalizationManager::Get().Format("battle.result.qte", {
            { "perfect", value(data.qtePerfect) },
            { "good", value(data.qteGood) },
            { "miss", value(data.qteMiss) }
        }),
        LocalizationManager::Get().Format("battle.result.dodges", {
            { "clean", value(data.cleanDodges) },
            { "hits", value(data.dodgeHits) }
        })
    };

    for (const std::string& stat : stats)
    {
        text.DrawStringRawScaled(stat.c_str(), statsX, statsY, statColor, 0.62f * ss, true);
        statsY += rowGap;
    }

    const float partyX = (mLayout.partyPanelX + mLayout.partyTextXOffset) * sx;
    float partyY = (mLayout.partyPanelY + mLayout.partyTextYOffset) * sy;
    for (const BattleMemberResult& member : data.members)
    {
        text.DrawStringRawScaled(member.name.c_str(), partyX, partyY,
                                 XMVectorSet(0.98f, 0.88f, 0.68f, alpha),
                                 0.72f * ss, true);
        text.DrawStringRawScaled(
            LocalizationManager::Get().Format("battle.result.member_level", {
                { "level", std::to_string(member.levelAfter) }
            }).c_str(),
            partyX,
            partyY + mLayout.partyLevelTextOffsetY * sy,
            XMVectorSet(0.92f, 0.92f, 0.88f, alpha),
            0.52f * ss,
            true);
        text.DrawStringRawScaled(
            LocalizationManager::Get().Format("battle.result.member_exp", {
                { "exp", std::to_string(member.expAfter) },
                { "next", std::to_string(member.expToNextAfter) }
            }).c_str(),
            partyX,
            partyY + mLayout.partyExpTextOffsetY * sy,
            XMVectorSet(0.82f, 0.86f, 0.90f, alpha),
            0.46f * ss,
            true);
        if (member.leveledUp)
        {
            text.DrawStringRawScaled(
                LocalizationManager::Get().Text("battle.result.level_up").c_str(),
                partyX + mLayout.partyLevelUpOffsetX * sx,
                partyY + mLayout.partyLevelTextOffsetY * sy,
                XMVectorSet(1.0f, 0.74f, 0.36f, alpha),
                0.46f * ss,
                true);
        }
        partyY += mLayout.partyRowGap * sy;
    }

    text.DrawStringRawScaled(
        LocalizationManager::Get().Text("battle.result.continue").c_str(),
        (mScreenW - 250.0f * sx),
        (mScreenH - 70.0f * sy),
        XMVectorSet(0.98f, 0.94f, 0.84f, alpha),
        0.70f * ss,
        true);
    text.EndBatch();
}

void BattleResultRenderer::DrawVictoryPortraits(const BattleResultData& data, float alpha)
{
    const float sx = static_cast<float>(mScreenW) / 1280.0f;
    const float sy = static_cast<float>(mScreenH) / 720.0f;

    float y = (mLayout.partyPanelY + mLayout.partyPortraitYOffset) * sy;
    const float x = (mLayout.partyPanelX + mLayout.partyPortraitXOffset) * sx;
    const float size = mLayout.partyPortraitSize * (std::min)(sx, sy);

    RECT source{};
    source.left = static_cast<LONG>(mLayout.partyPortraitSourceX);
    source.top = static_cast<LONG>(mLayout.partyPortraitSourceY);
    source.right = static_cast<LONG>(mLayout.partyPortraitSourceX + mLayout.partyPortraitSourceW);
    source.bottom = static_cast<LONG>(mLayout.partyPortraitSourceY + mLayout.partyPortraitSourceH);
    const RECT* sourceRect = (mLayout.partyPortraitSourceW > 0.0f && mLayout.partyPortraitSourceH > 0.0f)
        ? &source
        : nullptr;

    for (const BattleMemberResult& member : data.members)
    {
        const auto portrait = FindPortrait(member.id);
        if (portrait.Get())
        {
            const RECT dst = MakeRect(x, y, size, size);
            mSpriteBatch->Draw(portrait.Get(), dst, sourceRect, XMVectorSet(1.0f, 1.0f, 1.0f, alpha));
        }
        y += mLayout.partyRowGap * sy;
    }
}

void BattleResultRenderer::DrawDefeatGlyph(float centerX, float centerY, float alpha)
{
    const XMVECTOR red = XMVectorSet(0.75f, 0.04f, 0.08f, alpha);
    const float h = 240.0f;
    const float w = 120.0f;
    const float t = 2.0f;

    DrawFillRect(centerX - t * 0.5f, centerY - h * 0.5f, t, h, red);
    DrawFillRect(centerX - w * 0.5f, centerY - h * 0.5f, t, h, red);
    DrawFillRect(centerX + w * 0.5f, centerY - h * 0.5f, t, h, red);
    DrawFillRect(centerX - w * 0.5f, centerY - h * 0.5f, w, t, red);
    DrawFillRect(centerX - w * 0.5f, centerY + h * 0.5f, w, t, red);
    DrawFillRect(centerX - w * 0.35f, centerY - h * 0.25f, w * 0.70f, t, red);
    DrawFillRect(centerX - w * 0.35f, centerY + h * 0.25f, w * 0.70f, t, red);
}

ComPtr<ID3D11ShaderResourceView> BattleResultRenderer::FindPortrait(const std::string& memberId) const
{
    for (const PortraitEntry& entry : mPortraits)
    {
        if (entry.memberId == memberId) return entry.srv;
    }
    return {};
}
