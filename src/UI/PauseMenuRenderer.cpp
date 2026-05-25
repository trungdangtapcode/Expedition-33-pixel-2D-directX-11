// ============================================================
// File: PauseMenuRenderer.cpp
// Responsibility: Implement the screen-space overworld pause overlay.
//
// Rendering pipeline:
//   1. Draw a translucent dark scrim over the already-rendered overworld.
//   2. Draw a quiet framed panel and the current menu rows.
//   3. Draw a confirmation panel when destructive actions need consent.
//   4. Draw the black fade overlay last for title/quit transitions.
//
// Common mistakes:
//   1. Rendering the overworld here duplicates world ownership.
//   2. Hardcoding labels here bypasses localization.
//   3. Nesting BattleTextRenderer calls inside this SpriteBatch is invalid.
// ============================================================
#define NOMINMAX
#include "PauseMenuRenderer.h"
#include "../Renderer/NineSliceRenderer.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace
{
    bool ReadTextFile(const std::string& path, std::string& out)
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
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        return JsonLoader::detail::ParseFloat(raw, fallback);
    }

    float Clamp01(float value)
    {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    }

    RECT MakeRect(float x, float y, float width, float height)
    {
        RECT rect{};
        rect.left = static_cast<LONG>(std::round(x));
        rect.top = static_cast<LONG>(std::round(y));
        rect.right = static_cast<LONG>(std::round(x + width));
        rect.bottom = static_cast<LONG>(std::round(y + height));
        return rect;
    }
}

PauseMenuRenderer::~PauseMenuRenderer()
{
    Shutdown();
}

bool PauseMenuRenderer::Initialize(ID3D11Device* device,
                                   ID3D11DeviceContext* context,
                                   const std::string& layoutPath,
                                   int screenW,
                                   int screenH)
{
    Shutdown();

    mScreenW = screenW;
    mScreenH = screenH;

    LoadLayout(layoutPath);
    if (!CreateFillTexture(device)) return false;

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);

    const std::string activeFontPath = LocalizationManager::Get().GetCurrentFontPath();
    const std::wstring fontPath = ToWidePath(activeFontPath.empty() ? mLayout.fontPath : activeFontPath);
    if (!mTextRenderer.Initialize(device, context, fontPath, mScreenW, mScreenH))
    {
        return false;
    }

    // The renderer keeps a rectangle fallback, but the data-driven 9-slice
    // panel is the intended path because it preserves ornate corners while
    // allowing the same asset to scale to the main and confirmation panels.
    mPanelRendererReady = false;
    if (!mLayout.panelTexturePath.empty() && !mLayout.panelJsonPath.empty())
    {
        mPanelRenderer = std::make_unique<NineSliceRenderer>();
        mPanelRendererReady = mPanelRenderer->Initialize(
            device,
            context,
            ToWidePath(mLayout.panelTexturePath),
            mLayout.panelJsonPath,
            mScreenW,
            mScreenH);

        if (!mPanelRendererReady)
        {
            LOG("[PauseMenuRenderer] Panel asset failed to load; using rectangle fallback.");
            mPanelRenderer.reset();
        }
    }

    LOG("[PauseMenuRenderer] Initialized with '%s'.", layoutPath.c_str());
    return true;
}

void PauseMenuRenderer::SetScreenSize(int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
    mTextRenderer.SetScreenSize(screenW, screenH);
    if (mPanelRenderer)
    {
        mPanelRenderer->SetScreenSize(screenW, screenH);
    }
}

void PauseMenuRenderer::Render(ID3D11DeviceContext* context, const PauseMenuRenderState& state)
{
    if (!mSpriteBatch || !mStates || !mFillSRV) return;

    DrawFillRect(context,
                 0.0f,
                 0.0f,
                 static_cast<float>(mScreenW),
                 static_cast<float>(mScreenH),
                 DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, Clamp01(mLayout.dimAlpha)));

    DrawMainPanel(context, state);
    if (state.confirming)
    {
        DrawConfirmPanel(context, state);
    }

    DrawFade(context, state.fadeAlpha);
}

void PauseMenuRenderer::Shutdown()
{
    mTextRenderer.Shutdown();
    if (mPanelRenderer)
    {
        mPanelRenderer->Shutdown();
    }
    mPanelRenderer.reset();
    mPanelRendererReady = false;
    mSpriteBatch.reset();
    mStates.reset();
    mFillSRV.Reset();
}

bool PauseMenuRenderer::LoadLayout(const std::string& layoutPath)
{
    std::string src;
    if (!ReadTextFile(layoutPath, src))
    {
        LOG("[PauseMenuRenderer] Layout '%s' missing; using defaults.", layoutPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, layoutPath);

    mLayout.fontPath = ReadJsonString(src, "fontPath", mLayout.fontPath);
    mLayout.panelTexturePath = ReadJsonString(src, "panelTexturePath", mLayout.panelTexturePath);
    mLayout.panelJsonPath = ReadJsonString(src, "panelJsonPath", mLayout.panelJsonPath);
    mLayout.navigateSfxId = ReadJsonString(src, "navigateSfxId", mLayout.navigateSfxId);
    mLayout.confirmSfxId = ReadJsonString(src, "confirmSfxId", mLayout.confirmSfxId);
    mLayout.backSfxId = ReadJsonString(src, "backSfxId", mLayout.backSfxId);
    mLayout.dimAlpha = Clamp01(ReadJsonFloat(src, "dimAlpha", mLayout.dimAlpha));
    mLayout.panelWidth = ReadJsonFloat(src, "panelWidth", mLayout.panelWidth);
    mLayout.panelHeight = ReadJsonFloat(src, "panelHeight", mLayout.panelHeight);
    mLayout.panelCenterY = ReadJsonFloat(src, "panelCenterY", mLayout.panelCenterY);
    mLayout.panelAlpha = Clamp01(ReadJsonFloat(src, "panelAlpha", mLayout.panelAlpha));
    mLayout.panelSliceScale = std::max(0.1f, ReadJsonFloat(src, "panelSliceScale", mLayout.panelSliceScale));
    mLayout.borderAlpha = Clamp01(ReadJsonFloat(src, "borderAlpha", mLayout.borderAlpha));
    mLayout.borderThickness = ReadJsonFloat(src, "borderThickness", mLayout.borderThickness);
    mLayout.titleOffsetY = ReadJsonFloat(src, "titleOffsetY", mLayout.titleOffsetY);
    mLayout.titleScale = ReadJsonFloat(src, "titleScale", mLayout.titleScale);
    mLayout.optionStartOffsetY = ReadJsonFloat(src, "optionStartOffsetY", mLayout.optionStartOffsetY);
    mLayout.optionRowHeight = ReadJsonFloat(src, "optionRowHeight", mLayout.optionRowHeight);
    mLayout.optionTextScale = ReadJsonFloat(src, "optionTextScale", mLayout.optionTextScale);
    mLayout.highlightHeight = ReadJsonFloat(src, "highlightHeight", mLayout.highlightHeight);
    mLayout.highlightInset = ReadJsonFloat(src, "highlightInset", mLayout.highlightInset);
    mLayout.confirmPanelWidth = ReadJsonFloat(src, "confirmPanelWidth", mLayout.confirmPanelWidth);
    mLayout.confirmPanelHeight = ReadJsonFloat(src, "confirmPanelHeight", mLayout.confirmPanelHeight);
    mLayout.confirmPanelAlpha = Clamp01(ReadJsonFloat(src, "confirmPanelAlpha", mLayout.confirmPanelAlpha));
    mLayout.confirmTextOffsetY = ReadJsonFloat(src, "confirmTextOffsetY", mLayout.confirmTextOffsetY);
    mLayout.confirmOptionOffsetY = ReadJsonFloat(src, "confirmOptionOffsetY", mLayout.confirmOptionOffsetY);
    mLayout.confirmOptionGap = ReadJsonFloat(src, "confirmOptionGap", mLayout.confirmOptionGap);
    mLayout.confirmTextScale = ReadJsonFloat(src, "confirmTextScale", mLayout.confirmTextScale);
    mLayout.fadeDuration = ReadJsonFloat(src, "fadeDuration", mLayout.fadeDuration);

    LOG("[PauseMenuRenderer] Layout loaded from '%s'.", layoutPath.c_str());
    return true;
}

bool PauseMenuRenderer::CreateFillTexture(ID3D11Device* device)
{
    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = 1;
    textureDesc.Height = 1;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const std::uint32_t white = 0xFFFFFFFFu;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &white;
    init.SysMemPitch = sizeof(white);

    ComPtr<ID3D11Texture2D> texture;
    const HRESULT hr = device->CreateTexture2D(&textureDesc, &init, texture.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[PauseMenuRenderer] Failed to create fill texture (0x%08X).",
            static_cast<unsigned>(hr));
        return false;
    }

    const HRESULT srvHr = device->CreateShaderResourceView(texture.Get(), nullptr, mFillSRV.GetAddressOf());
    if (FAILED(srvHr))
    {
        LOG("[PauseMenuRenderer] Failed to create fill SRV (0x%08X).",
            static_cast<unsigned>(srvHr));
        return false;
    }

    return true;
}

void PauseMenuRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(mScreenW);
    viewport.Height = static_cast<float>(mScreenH);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    context->RSSetViewports(1, &viewport);
}

void PauseMenuRenderer::DrawFillRect(ID3D11DeviceContext* context,
                                     float x,
                                     float y,
                                     float width,
                                     float height,
                                     DirectX::FXMVECTOR color)
{
    if (!mSpriteBatch || !mStates || !mFillSRV) return;

    BindViewport(context);
    mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred,
                        mStates->NonPremultiplied(),
                        mStates->PointClamp(),
                        mStates->DepthNone());

    const RECT rect = MakeRect(x, y, width, height);
    mSpriteBatch->Draw(mFillSRV.Get(), rect, color);

    mSpriteBatch->End();
}

void PauseMenuRenderer::DrawPanel(ID3D11DeviceContext* context,
                                  float x,
                                  float y,
                                  float width,
                                  float height,
                                  float alpha)
{
    if (mPanelRendererReady && mPanelRenderer)
    {
        const float tint = Clamp01(alpha);
        mPanelRenderer->Draw(context,
                             x,
                             y,
                             width,
                             height,
                             mLayout.panelSliceScale,
                             DirectX::XMMatrixIdentity(),
                             DirectX::XMVectorSet(tint, tint, tint, tint));
        return;
    }

    const float border = std::max(1.0f, mLayout.borderThickness);
    const DirectX::XMVECTOR panelColor = DirectX::XMVectorSet(0.04f, 0.04f, 0.045f, Clamp01(alpha));
    const DirectX::XMVECTOR borderColor = DirectX::XMVectorSet(0.86f, 0.73f, 0.46f, Clamp01(mLayout.borderAlpha));

    DrawFillRect(context, x, y, width, height, panelColor);
    DrawFillRect(context, x, y, width, border, borderColor);
    DrawFillRect(context, x, y + height - border, width, border, borderColor);
    DrawFillRect(context, x, y, border, height, borderColor);
    DrawFillRect(context, x + width - border, y, border, height, borderColor);
}

void PauseMenuRenderer::DrawMainPanel(ID3D11DeviceContext* context,
                                      const PauseMenuRenderState& state)
{
    const float panelX = (static_cast<float>(mScreenW) - mLayout.panelWidth) * 0.5f;
    const float panelY = mLayout.panelCenterY - (mLayout.panelHeight * 0.5f);
    const float centerX = static_cast<float>(mScreenW) * 0.5f;
    DrawPanel(context, panelX, panelY, mLayout.panelWidth, mLayout.panelHeight, mLayout.panelAlpha);

    const float pulse = 0.88f + 0.12f * (0.5f + 0.5f * std::sin(state.elapsed * 2.2f));
    const DirectX::XMVECTOR titleColor = DirectX::XMVectorSet(0.93f, 0.84f, 0.64f, pulse);

    mTextRenderer.BeginBatch(context);
    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("pause.title").c_str(),
        centerX,
        panelY + mLayout.titleOffsetY,
        titleColor,
        mLayout.titleScale,
        true);

    const float rowX = panelX + mLayout.highlightInset;
    const float rowW = mLayout.panelWidth - mLayout.highlightInset * 2.0f;
    for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
    {
        const float rowY = panelY + mLayout.optionStartOffsetY + i * mLayout.optionRowHeight;
        const bool selected = i == state.cursor && !state.confirming;
        if (selected)
        {
            mTextRenderer.EndBatch();
            DrawFillRect(context,
                         rowX,
                         rowY + (mLayout.optionRowHeight - mLayout.highlightHeight) * 0.5f,
                         rowW,
                         mLayout.highlightHeight,
                         DirectX::XMVectorSet(0.64f, 0.46f, 0.24f, 0.72f));
            mTextRenderer.BeginBatch(context);
        }

        const DirectX::XMVECTOR textColor = selected
            ? DirectX::XMVectorSet(1.0f, 0.91f, 0.55f, 1.0f)
            : DirectX::XMVectorSet(0.86f, 0.86f, 0.82f, 0.92f);
        mTextRenderer.DrawStringCenteredRaw(
            state.options[i].label.c_str(),
            centerX,
            rowY + 5.0f,
            textColor,
            mLayout.optionTextScale,
            true);
    }
    mTextRenderer.EndBatch();
}

void PauseMenuRenderer::DrawConfirmPanel(ID3D11DeviceContext* context,
                                         const PauseMenuRenderState& state)
{
    const float panelX = (static_cast<float>(mScreenW) - mLayout.confirmPanelWidth) * 0.5f;
    const float panelY = (static_cast<float>(mScreenH) - mLayout.confirmPanelHeight) * 0.5f;
    const float centerX = static_cast<float>(mScreenW) * 0.5f;
    DrawPanel(context, panelX, panelY, mLayout.confirmPanelWidth, mLayout.confirmPanelHeight, mLayout.confirmPanelAlpha);

    const float optionY = panelY + mLayout.confirmOptionOffsetY;
    const float yesX = centerX - mLayout.confirmOptionGap * 0.5f;
    const float noX = centerX + mLayout.confirmOptionGap * 0.5f;

    if (state.confirmCursor == 0)
    {
        DrawFillRect(context, yesX - 62.0f, optionY - 5.0f, 124.0f, 34.0f,
                     DirectX::XMVectorSet(0.64f, 0.46f, 0.24f, 0.74f));
    }
    else
    {
        DrawFillRect(context, noX - 62.0f, optionY - 5.0f, 124.0f, 34.0f,
                     DirectX::XMVectorSet(0.64f, 0.46f, 0.24f, 0.74f));
    }

    mTextRenderer.BeginBatch(context);
    mTextRenderer.DrawStringCenteredRaw(
        state.confirmMessage.c_str(),
        centerX,
        panelY + mLayout.confirmTextOffsetY,
        DirectX::XMVectorSet(0.92f, 0.88f, 0.80f, 1.0f),
        mLayout.confirmTextScale,
        true);

    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("pause.yes").c_str(),
        yesX,
        optionY,
        state.confirmCursor == 0
            ? DirectX::XMVectorSet(1.0f, 0.91f, 0.55f, 1.0f)
            : DirectX::XMVectorSet(0.82f, 0.82f, 0.78f, 0.88f),
        mLayout.optionTextScale,
        true);

    mTextRenderer.DrawStringCenteredRaw(
        LocalizationManager::Get().Text("pause.no").c_str(),
        noX,
        optionY,
        state.confirmCursor == 1
            ? DirectX::XMVectorSet(1.0f, 0.91f, 0.55f, 1.0f)
            : DirectX::XMVectorSet(0.82f, 0.82f, 0.78f, 0.88f),
        mLayout.optionTextScale,
        true);
    mTextRenderer.EndBatch();
}

void PauseMenuRenderer::DrawFade(ID3D11DeviceContext* context, float alpha)
{
    const float clamped = Clamp01(alpha);
    if (clamped <= 0.0f) return;

    DrawFillRect(context,
                 0.0f,
                 0.0f,
                 static_cast<float>(mScreenW),
                 static_cast<float>(mScreenH),
                 DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, clamped));
}

std::wstring PauseMenuRenderer::ToWidePath(const std::string& path)
{
    return std::wstring(path.begin(), path.end());
}
