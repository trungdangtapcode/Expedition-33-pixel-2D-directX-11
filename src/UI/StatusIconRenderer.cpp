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
        mSpriteBatch->Draw(mFillSRV.Get(), XMFLOAT2(x - 2.0f, y - 2.0f), nullptr, ring, 0.0f, origin,
                           XMFLOAT2(mLayout.iconSize + 4.0f, mLayout.iconSize + 4.0f));

        auto it = mFrames.find(effect.iconId);
        if (it == mFrames.end()) it = mFrames.find("fallback");
        if (it != mFrames.end())
        {
            const Frame& frame = it->second;
            RECT src = { frame.x, frame.y, frame.x + frame.w, frame.y + frame.h };
            const float scale = mLayout.iconSize / static_cast<float>(std::max(1, frame.w));
            mSpriteBatch->Draw(mAtlasSRV.Get(), XMFLOAT2(x, y), &src, Colors::White, 0.0f, origin, scale);
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
    return true;
}

XMVECTORF32 StatusIconRenderer::CategoryColor(StatusEffectCategory category) const
{
    switch (category)
    {
    case StatusEffectCategory::Buff:
        return XMVECTORF32{ 0.86f, 0.67f, 0.25f, 0.95f };
    case StatusEffectCategory::Debuff:
        return XMVECTORF32{ 0.70f, 0.16f, 0.18f, 0.95f };
    case StatusEffectCategory::Neutral:
    default:
        return XMVECTORF32{ 0.46f, 0.46f, 0.50f, 0.95f };
    }
}
