// ============================================================
// File: StatusIconRenderer.cpp
// Responsibility: Draw status effect atlas frames and compact counters.
// ============================================================
#define NOMINMAX
#include "StatusIconRenderer.h"
#include "BattleTextRenderer.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
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
}

bool StatusIconRenderer::Initialize(ID3D11Device* device,
                                    ID3D11DeviceContext* context,
                                    const std::wstring& atlasPath,
                                    const std::string& metadataPath,
                                    const std::string& layoutPath,
                                    int screenW,
                                    int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
    LoadLayout(layoutPath);
    LoadMetadata(metadataPath);

    HRESULT hr = CreateWICTextureFromFileEx(
        device,
        context,
        atlasPath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mAtlasSRV.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[StatusIconRenderer] Failed to load atlas (0x%08X).", (unsigned)hr);
        return false;
    }

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
    hr = device->CreateTexture2D(&td, &init, tex.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(tex.Get(), &srv, mFillSRV.GetAddressOf());
    if (FAILED(hr)) return false;

    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates = std::make_unique<CommonStates>(device);
    return true;
}

void StatusIconRenderer::Render(ID3D11DeviceContext* context,
                                BattleTextRenderer& textRenderer,
                                const std::vector<StatusEffectView>& effects,
                                float barX,
                                float barY)
{
    RenderAt(context,
             textRenderer,
             effects,
             barX + mLayout.offsetX,
             barY + mLayout.offsetY);
}

void StatusIconRenderer::RenderAt(ID3D11DeviceContext* context,
                                  BattleTextRenderer& textRenderer,
                                  const std::vector<StatusEffectView>& effects,
                                  float startX,
                                  float startY)
{
    if (!IsInitialized() || effects.empty()) return;

    const int visibleCount = std::min(static_cast<int>(effects.size()), mLayout.maxVisible);
    BindViewport(context);

    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->PointClamp(),
        mStates->DepthNone());

    const XMFLOAT2 origin(0.0f, 0.0f);
    for (int i = 0; i < visibleCount; ++i)
    {
        const StatusEffectView& effect = effects[i];
        const float x = startX + static_cast<float>(i) * (mLayout.iconSize + mLayout.spacing);
        const float y = startY;

        const XMVECTORF32 ring = CategoryColor(effect.category);
        const float badgeX = x - mLayout.badgePadding;
        const float badgeY = y - mLayout.badgePadding;
        const float badgeSize = mLayout.iconSize + (mLayout.badgePadding * 2.0f);
        DrawSolidRect(
            badgeX,
            badgeY,
            badgeSize,
            badgeSize,
            XMVectorSet(mLayout.badgeBackR,
                        mLayout.badgeBackG,
                        mLayout.badgeBackB,
                        mLayout.badgeBackA));
        DrawBadgeFrame(badgeX, badgeY, badgeSize, ring);

        auto it = mFrames.find(effect.iconId);
        if (it == mFrames.end()) it = mFrames.find("fallback");
        if (it != mFrames.end())
        {
            const Frame& frame = it->second;
            RECT src = { frame.x, frame.y, frame.x + frame.w, frame.y + frame.h };
            const float frameW = static_cast<float>(std::max(1, frame.w));
            const float frameH = static_cast<float>(std::max(1, frame.h));
            const float scale = std::min(mLayout.iconSize / frameW, mLayout.iconSize / frameH);
            const float drawW = frameW * scale;
            const float drawH = frameH * scale;
            const XMFLOAT2 iconPos(
                x + (mLayout.iconSize - drawW) * 0.5f,
                y + (mLayout.iconSize - drawH) * 0.5f);
            mSpriteBatch->Draw(mAtlasSRV.Get(), iconPos, &src, Colors::White, 0.0f, origin, scale);
        }
    }
    mSpriteBatch->End();

    if (textRenderer.IsReady())
    {
        textRenderer.BeginBatch(context);
        for (int i = 0; i < visibleCount; ++i)
        {
            const StatusEffectView& effect = effects[i];
            const float x = startX + static_cast<float>(i) * (mLayout.iconSize + mLayout.spacing);
            const float y = startY;

            if (effect.remainingTurns > 0)
            {
                const std::string turns = std::to_string(effect.remainingTurns);
                textRenderer.DrawStringRawScaled(
                    turns.c_str(),
                    x + mLayout.iconSize - 8.0f,
                    y + mLayout.iconSize - 12.0f,
                    Colors::White,
                    mLayout.textScale,
                    true);
            }
            if (effect.stackCount > 1)
            {
                const std::string stacks = "x" + std::to_string(effect.stackCount);
                textRenderer.DrawStringRawScaled(
                    stacks.c_str(),
                    x - 1.0f,
                    y - 6.0f,
                    Colors::White,
                    mLayout.textScale,
                    true);
            }
        }

        if (static_cast<int>(effects.size()) > visibleCount)
        {
            const int overflow = static_cast<int>(effects.size()) - visibleCount;
            const float x = startX + static_cast<float>(visibleCount) * (mLayout.iconSize + mLayout.spacing);
            const float y = startY + 5.0f;
            const std::string label = "+" + std::to_string(overflow);
            textRenderer.DrawStringRawScaled(label.c_str(), x, y, Colors::White, mLayout.textScale, true);
        }
        textRenderer.EndBatch();
    }
}

void StatusIconRenderer::SetScreenSize(int w, int h)
{
    mScreenW = w;
    mScreenH = h;
}

void StatusIconRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mAtlasSRV.Reset();
    mFillSRV.Reset();
}

void StatusIconRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(mScreenW);
    vp.Height = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
    mSpriteBatch->SetViewport(vp);
}

bool StatusIconRenderer::LoadMetadata(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        LOG("[StatusIconRenderer] Cannot open metadata '%s'.", path.c_str());
        return false;
    }

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
        mFrames[id] = frame;
    }
    return true;
}

bool StatusIconRenderer::LoadLayout(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();

    mLayout.offsetX = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "offsetX"), mLayout.offsetX);
    mLayout.offsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "offsetY"), mLayout.offsetY);
    mLayout.iconSize = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "iconSize"), mLayout.iconSize);
    mLayout.spacing = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "spacing"), mLayout.spacing);
    mLayout.maxVisible = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "maxVisible"), mLayout.maxVisible);
    mLayout.textScale = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "textScale"), mLayout.textScale);
    mLayout.badgePadding = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "badgePadding"), mLayout.badgePadding);
    mLayout.badgeFrameThickness = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "badgeFrameThickness"), mLayout.badgeFrameThickness);
    mLayout.badgeBackR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "badgeBackR"), mLayout.badgeBackR);
    mLayout.badgeBackG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "badgeBackG"), mLayout.badgeBackG);
    mLayout.badgeBackB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "badgeBackB"), mLayout.badgeBackB);
    mLayout.badgeBackA = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "badgeBackA"), mLayout.badgeBackA);
    mLayout.buffFrameR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "buffFrameR"), mLayout.buffFrameR);
    mLayout.buffFrameG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "buffFrameG"), mLayout.buffFrameG);
    mLayout.buffFrameB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "buffFrameB"), mLayout.buffFrameB);
    mLayout.buffFrameA = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "buffFrameA"), mLayout.buffFrameA);
    mLayout.debuffFrameR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "debuffFrameR"), mLayout.debuffFrameR);
    mLayout.debuffFrameG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "debuffFrameG"), mLayout.debuffFrameG);
    mLayout.debuffFrameB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "debuffFrameB"), mLayout.debuffFrameB);
    mLayout.debuffFrameA = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "debuffFrameA"), mLayout.debuffFrameA);
    mLayout.neutralFrameR = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "neutralFrameR"), mLayout.neutralFrameR);
    mLayout.neutralFrameG = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "neutralFrameG"), mLayout.neutralFrameG);
    mLayout.neutralFrameB = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "neutralFrameB"), mLayout.neutralFrameB);
    mLayout.neutralFrameA = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "neutralFrameA"), mLayout.neutralFrameA);
    if (mLayout.iconSize < 1.0f) mLayout.iconSize = 1.0f;
    if (mLayout.badgePadding < 0.0f) mLayout.badgePadding = 0.0f;
    if (mLayout.badgeFrameThickness < 0.0f) mLayout.badgeFrameThickness = 0.0f;
    if (mLayout.maxVisible < 0) mLayout.maxVisible = 0;
    return true;
}

XMVECTORF32 StatusIconRenderer::CategoryColor(StatusEffectCategory category) const
{
    switch (category)
    {
    case StatusEffectCategory::Buff:
        return XMVECTORF32{ mLayout.buffFrameR, mLayout.buffFrameG, mLayout.buffFrameB, mLayout.buffFrameA };
    case StatusEffectCategory::Debuff:
        return XMVECTORF32{ mLayout.debuffFrameR, mLayout.debuffFrameG, mLayout.debuffFrameB, mLayout.debuffFrameA };
    case StatusEffectCategory::Neutral:
    default:
        return XMVECTORF32{ mLayout.neutralFrameR, mLayout.neutralFrameG, mLayout.neutralFrameB, mLayout.neutralFrameA };
    }
}

void StatusIconRenderer::DrawSolidRect(float x,
                                       float y,
                                       float width,
                                       float height,
                                       FXMVECTOR color)
{
    if (!mFillSRV) return;
    mSpriteBatch->Draw(
        mFillSRV.Get(),
        XMFLOAT2(x, y),
        nullptr,
        color,
        0.0f,
        XMFLOAT2(0.0f, 0.0f),
        XMFLOAT2(width, height));
}

void StatusIconRenderer::DrawBadgeFrame(float x,
                                        float y,
                                        float size,
                                        FXMVECTOR frameColor)
{
    const float t = std::max(0.0f, mLayout.badgeFrameThickness);
    if (t <= 0.0f) return;

    // Draw four thin rectangles instead of a solid category plate. This keeps
    // debuffs readable without turning the enemy HP area into a red block.
    DrawSolidRect(x, y, size, t, frameColor);
    DrawSolidRect(x, y + size - t, size, t, frameColor);
    DrawSolidRect(x, y, t, size, frameColor);
    DrawSolidRect(x + size - t, y, t, size, frameColor);
}
