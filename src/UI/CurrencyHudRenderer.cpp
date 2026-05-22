// ============================================================
// File: CurrencyHudRenderer.cpp
// Responsibility: Implement the reusable coin-balance HUD renderer.
// ============================================================
#define NOMINMAX
#include "CurrencyHudRenderer.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <WICTextureLoader.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    std::filesystem::path ResolveReadablePath(const std::string& path)
    {
        namespace fs = std::filesystem;

        fs::path direct(path);
        if (fs::exists(direct)) return direct;

        fs::path parent = fs::path("..") / path;
        if (fs::exists(parent)) return parent;

        return direct;
    }

    bool ReadTextFile(const std::filesystem::path& path, std::string& out)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    std::string ReadJsonString(const std::string& src,
                               const std::string& key,
                               const std::string& fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }

    float ReadJsonFloat(const std::string& src,
                        const std::string& key,
                        float fallback)
    {
        return JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(src, key),
            fallback);
    }
}

bool CurrencyHudRenderer::Initialize(ID3D11Device* device,
                                     ID3D11DeviceContext* context,
                                     const std::wstring& fontPath,
                                     int screenW,
                                     int screenH)
{
    Shutdown();

    mScreenW = screenW;
    mScreenH = screenH;
    LoadConfig("data/currency_hud.json");

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);

    const bool iconReady = LoadIcon(device, context, mConfig.iconPath);
    const bool textReady = mTextRenderer.Initialize(device, context, fontPath, screenW, screenH);
    return iconReady && textReady;
}

bool CurrencyHudRenderer::LoadConfig(const std::string& path)
{
    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[CurrencyHudRenderer] WARNING: Missing '%s'; using defaults.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    mConfig.iconPath = ReadJsonString(src, "iconPath", mConfig.iconPath);
    mConfig.labelKey = ReadJsonString(src, "labelKey", mConfig.labelKey);
    mConfig.sourceIconSize = ReadJsonFloat(src, "sourceIconSize", mConfig.sourceIconSize);
    mConfig.iconSize = ReadJsonFloat(src, "iconSize", mConfig.iconSize);
    mConfig.textOffsetX = ReadJsonFloat(src, "textOffsetX", mConfig.textOffsetX);
    mConfig.textOffsetY = ReadJsonFloat(src, "textOffsetY", mConfig.textOffsetY);
    mConfig.topRightWidth = ReadJsonFloat(src, "topRightWidth", mConfig.topRightWidth);
    mConfig.topRightMarginX = ReadJsonFloat(src, "topRightMarginX", mConfig.topRightMarginX);
    mConfig.topRightY = ReadJsonFloat(src, "topRightY", mConfig.topRightY);
    mConfig.campfireOffsetX = ReadJsonFloat(src, "campfireOffsetX", mConfig.campfireOffsetX);
    mConfig.campfireOffsetY = ReadJsonFloat(src, "campfireOffsetY", mConfig.campfireOffsetY);
    return true;
}

bool CurrencyHudRenderer::LoadIcon(ID3D11Device* device,
                                   ID3D11DeviceContext* context,
                                   const std::string& path)
{
    const std::filesystem::path resolved = ResolveReadablePath(path);
    const std::wstring widePath = resolved.wstring();

    const HRESULT hr = DirectX::CreateWICTextureFromFile(
        device,
        context,
        widePath.c_str(),
        nullptr,
        mCoinSRV.ReleaseAndGetAddressOf());

    if (FAILED(hr))
    {
        LOG("[CurrencyHudRenderer] WARNING: Failed to load coin icon '%s'.", path.c_str());
        return false;
    }

    return true;
}

void CurrencyHudRenderer::SetScreenSize(int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
    mTextRenderer.SetScreenSize(screenW, screenH);
}

std::string CurrencyHudRenderer::FormatLabel(int coins) const
{
    return LocalizationManager::Get().Format(
        mConfig.labelKey,
        { { "amount", std::to_string(coins) } });
}

void CurrencyHudRenderer::BindViewport(ID3D11DeviceContext* context) const
{
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(mScreenW);
    viewport.Height = static_cast<float>(mScreenH);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
}

void CurrencyHudRenderer::RenderAt(ID3D11DeviceContext* context, int coins, float x, float y)
{
    if (!mSpriteBatch || !mStates || !mCoinSRV) return;

    BindViewport(context);

    const DirectX::XMFLOAT2 position(x, y);
    const float sourceSize = (mConfig.sourceIconSize <= 0.0f) ? 32.0f : mConfig.sourceIconSize;
    const DirectX::XMFLOAT2 scale(mConfig.iconSize / sourceSize, mConfig.iconSize / sourceSize);
    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->PointClamp(),
        mStates->DepthNone());
    mSpriteBatch->Draw(mCoinSRV.Get(), position, nullptr, DirectX::Colors::White,
                       0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), scale);
    mSpriteBatch->End();

    const std::string label = FormatLabel(coins);
    mTextRenderer.DrawString(context,
                             label.c_str(),
                             x + mConfig.textOffsetX,
                             y + mConfig.textOffsetY,
                             DirectX::Colors::PaleGoldenrod);
}

void CurrencyHudRenderer::RenderTopRight(ID3D11DeviceContext* context, int coins)
{
    const float x = static_cast<float>(mScreenW) - mConfig.topRightMarginX - mConfig.topRightWidth;
    RenderAt(context, coins, x, mConfig.topRightY);
}

void CurrencyHudRenderer::RenderCampfirePanel(ID3D11DeviceContext* context,
                                              int coins,
                                              float panelX,
                                              float panelY)
{
    RenderAt(context,
             coins,
             panelX + mConfig.campfireOffsetX,
             panelY + mConfig.campfireOffsetY);
}

void CurrencyHudRenderer::Shutdown()
{
    mCoinSRV.Reset();
    mSpriteBatch.reset();
    mStates.reset();
    mTextRenderer.Shutdown();
}
