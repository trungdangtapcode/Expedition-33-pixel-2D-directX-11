// ============================================================
// File: TitleMenuRenderer.cpp
// Responsibility: Implement the title screen visual presentation.
//
// Rendering pipeline:
//   1. Draw the title banner as a cover-scaled full-screen image.
//   2. Apply a translucent dark fill so menu text stays readable.
//   3. Draw one nine-slice menu panel for the active phase.
//   4. Draw row highlights and text from prepared MenuState view data.
//
// Common mistakes:
//   1. Passing a camera matrix to this renderer -> title UI is screen-space.
//   2. Pre-scaling the 1920x1080 logo asset -> cover math already handles it.
//   3. Recreating textures every frame -> Initialize owns all GPU resources.
// ============================================================
#define NOMINMAX
#include "TitleMenuRenderer.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <WICTextureLoader.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float kPanelMargin = 32.0f;
    constexpr float kHeaderY = 28.0f;
    constexpr float kSubtitleY = 58.0f;
    constexpr float kHighlightInsetX = 42.0f;
    constexpr float kHighlightH = 34.0f;
    constexpr float kSelectorOffsetX = 34.0f;
    constexpr size_t kSlotSecondaryLimit = 62;

    // ------------------------------------------------------------
    // Function: ReadTextFile
    // Purpose:
    //   Load the title-menu layout file into one string.
    // Why:
    //   The project JSON helpers operate on full text blocks, and the
    //   layout file is intentionally tiny.
    // ------------------------------------------------------------
    bool ReadTextFile(const std::string& path, std::string& out)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    // ------------------------------------------------------------
    // Function: ReadJsonString
    // Purpose:
    //   Read an optional top-level JSON string with a fallback.
    // Why:
    //   Title layout paths are authored data but should fail soft when a
    //   field is missing during iteration.
    // ------------------------------------------------------------
    std::string ReadJsonString(const std::string& src,
                               const std::string& key,
                               const std::string& fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }

    // ------------------------------------------------------------
    // Function: ReadJsonFloat
    // Purpose:
    //   Read an optional top-level JSON float with a fallback.
    // Why:
    //   Menu layout numbers should be data-driven while preserving safe
    //   defaults when the config file is incomplete.
    // ------------------------------------------------------------
    float ReadJsonFloat(const std::string& src,
                        const std::string& key,
                        float fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        return JsonLoader::detail::ParseFloat(raw, fallback);
    }

    // ------------------------------------------------------------
    // Function: Clamp01
    // Purpose:
    //   Clamp alpha values before passing them to SpriteBatch tint colors.
    // Why:
    //   Layout edits or timer values outside [0,1] should not produce
    //   invalid blend factors.
    // ------------------------------------------------------------
    float Clamp01(float value)
    {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    }

    // ------------------------------------------------------------
    // Function: MakeRect
    // Purpose:
    //   Convert float layout coordinates into a DirectXTK destination rect.
    // Why:
    //   SpriteBatch destination rectangles use integer pixel bounds.
    // ------------------------------------------------------------
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

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Load layout data and create all GPU-backed title-menu resources.
// Why:
//   The title state is the only owner of these resources, so startup work
//   happens once in OnEnter instead of during Render.
// Parameters:
//   device/context - D3D11 objects used to create textures and SpriteBatch.
//   layoutPath     - JSON file containing screen layout and asset paths.
//   screenW/H      - current render target size for SpriteBatch viewports.
// ------------------------------------------------------------
bool TitleMenuRenderer::Initialize(ID3D11Device* device,
                                   ID3D11DeviceContext* context,
                                   const std::string& layoutPath,
                                   int screenW,
                                   int screenH)
{
    Shutdown();

    mScreenW = screenW;
    mScreenH = screenH;

    LoadLayout(layoutPath);

    if (!LoadBackgroundTexture(device, context)) return false;
    if (!CreateFillTexture(device)) return false;

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);

    const std::wstring dialogTexture = ToWidePath(mLayout.dialogTexturePath);
    if (!mDialogBox.Initialize(device, context,
                               dialogTexture,
                               mLayout.dialogJsonPath,
                               mScreenW, mScreenH))
    {
        return false;
    }

    const std::wstring fontPath = ToWidePath(mLayout.fontPath);
    if (!mTextRenderer.Initialize(device, context,
                                  fontPath,
                                  mScreenW, mScreenH))
    {
        return false;
    }

    mInitialized = true;
    LOG("[TitleMenuRenderer] Initialized with '%s'.", layoutPath.c_str());
    return true;
}

// ------------------------------------------------------------
// Function: LoadLayout
// Purpose:
//   Parse title-menu layout values from JSON into mLayout.
// Why:
//   Artists and designers can adjust the menu composition without
//   recompiling C++.
// ------------------------------------------------------------
bool TitleMenuRenderer::LoadLayout(const std::string& layoutPath)
{
    std::string src;
    if (!ReadTextFile(layoutPath, src))
    {
        LOG("[TitleMenuRenderer] Layout '%s' missing; using defaults.",
            layoutPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, layoutPath);

    mLayout.backgroundImagePath = ReadJsonString(src, "backgroundImagePath", mLayout.backgroundImagePath);
    mLayout.dialogTexturePath = ReadJsonString(src, "dialogTexturePath", mLayout.dialogTexturePath);
    mLayout.dialogJsonPath = ReadJsonString(src, "dialogJsonPath", mLayout.dialogJsonPath);
    mLayout.fontPath = ReadJsonString(src, "fontPath", mLayout.fontPath);
    mLayout.mainPanelWidth = ReadJsonFloat(src, "mainPanelWidth", mLayout.mainPanelWidth);
    mLayout.mainPanelHeight = ReadJsonFloat(src, "mainPanelHeight", mLayout.mainPanelHeight);
    mLayout.mainPanelRight = ReadJsonFloat(src, "mainPanelRight", mLayout.mainPanelRight);
    mLayout.mainPanelBottom = ReadJsonFloat(src, "mainPanelBottom", mLayout.mainPanelBottom);
    mLayout.slotPanelWidth = ReadJsonFloat(src, "slotPanelWidth", mLayout.slotPanelWidth);
    mLayout.slotPanelHeight = ReadJsonFloat(src, "slotPanelHeight", mLayout.slotPanelHeight);
    mLayout.slotPanelBottom = ReadJsonFloat(src, "slotPanelBottom", mLayout.slotPanelBottom);
    mLayout.panelAlpha = Clamp01(ReadJsonFloat(src, "panelAlpha", mLayout.panelAlpha));
    mLayout.backgroundDimAlpha = Clamp01(ReadJsonFloat(src, "backgroundDimAlpha", mLayout.backgroundDimAlpha));
    mLayout.logoAlphaMin = Clamp01(ReadJsonFloat(src, "logoAlphaMin", mLayout.logoAlphaMin));
    mLayout.logoAlphaMax = Clamp01(ReadJsonFloat(src, "logoAlphaMax", mLayout.logoAlphaMax));
    mLayout.logoPulseSpeed = ReadJsonFloat(src, "logoPulseSpeed", mLayout.logoPulseSpeed);
    mLayout.optionStartX = ReadJsonFloat(src, "optionStartX", mLayout.optionStartX);
    mLayout.optionStartY = ReadJsonFloat(src, "optionStartY", mLayout.optionStartY);
    mLayout.optionRowHeight = ReadJsonFloat(src, "optionRowHeight", mLayout.optionRowHeight);
    mLayout.slotStartX = ReadJsonFloat(src, "slotStartX", mLayout.slotStartX);
    mLayout.slotStartY = ReadJsonFloat(src, "slotStartY", mLayout.slotStartY);
    mLayout.slotRowHeight = ReadJsonFloat(src, "slotRowHeight", mLayout.slotRowHeight);
    mLayout.flashDuration = ReadJsonFloat(src, "flashDuration", mLayout.flashDuration);

    if (mLayout.logoAlphaMax < mLayout.logoAlphaMin)
    {
        std::swap(mLayout.logoAlphaMin, mLayout.logoAlphaMax);
    }

    LOG("[TitleMenuRenderer] Layout loaded from '%s'.", layoutPath.c_str());
    return true;
}

// ------------------------------------------------------------
// Function: LoadBackgroundTexture
// Purpose:
//   Upload the title banner texture and cache its dimensions.
// Why:
//   Cover-scaling needs the source size to preserve the logo aspect ratio.
// ------------------------------------------------------------
bool TitleMenuRenderer::LoadBackgroundTexture(ID3D11Device* device,
                                              ID3D11DeviceContext* context)
{
    ComPtr<ID3D11Resource> resource;
    const std::wstring path = ToWidePath(mLayout.backgroundImagePath);

    const HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        path.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        DirectX::WIC_LOADER_IGNORE_SRGB,
        resource.GetAddressOf(),
        mBackgroundSRV.GetAddressOf());

    if (FAILED(hr))
    {
        LOG("[TitleMenuRenderer] Failed to load background '%ls' (0x%08X).",
            path.c_str(), static_cast<unsigned>(hr));
        return false;
    }

    ComPtr<ID3D11Texture2D> texture;
    if (SUCCEEDED(resource.As(&texture)) && texture)
    {
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        mBackgroundW = static_cast<int>(desc.Width);
        mBackgroundH = static_cast<int>(desc.Height);
    }

    LOG("[TitleMenuRenderer] Background loaded: '%ls' (%dx%d).",
        path.c_str(), mBackgroundW, mBackgroundH);
    return true;
}

// ------------------------------------------------------------
// Function: CreateFillTexture
// Purpose:
//   Create a 1x1 white SRV used for translucent overlays and highlights.
// Why:
//   Tinted rectangle draws avoid a collection of one-off panel PNGs.
// ------------------------------------------------------------
bool TitleMenuRenderer::CreateFillTexture(ID3D11Device* device)
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
    HRESULT hr = device->CreateTexture2D(&textureDesc, &init, texture.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[TitleMenuRenderer] Failed to create fill texture (0x%08X).",
            static_cast<unsigned>(hr));
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, mFillSRV.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[TitleMenuRenderer] Failed to create fill SRV (0x%08X).",
            static_cast<unsigned>(hr));
        return false;
    }

    return true;
}

// ------------------------------------------------------------
// Function: SetScreenSize
// Purpose:
//   Update viewport dimensions after a window resize.
// Why:
//   SpriteBatch derives pixel-to-NDC mapping from the active viewport.
// ------------------------------------------------------------
void TitleMenuRenderer::SetScreenSize(int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
    mDialogBox.SetScreenSize(screenW, screenH);
    mTextRenderer.SetScreenSize(screenW, screenH);
}

// ------------------------------------------------------------
// Function: BindViewport
// Purpose:
//   Bind the current screen viewport before SpriteBatch draws.
// Why:
//   Other renderers can leave the rasterizer stage without a viewport.
// ------------------------------------------------------------
void TitleMenuRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(mScreenW);
    viewport.Height = static_cast<float>(mScreenH);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    context->RSSetViewports(1, &viewport);
    if (mSpriteBatch)
    {
        mSpriteBatch->SetViewport(viewport);
    }
}

// ------------------------------------------------------------
// Function: DrawBackdrop
// Purpose:
//   Draw the title banner and a readability dimmer.
// Why:
//   The logo asset should be the first visual signal, while menu text still
//   needs contrast over the dark pixel texture.
// ------------------------------------------------------------
void TitleMenuRenderer::DrawBackdrop(ID3D11DeviceContext* context, float elapsed)
{
    if (!mSpriteBatch || !mStates || !mBackgroundSRV || !mFillSRV) return;

    const float screenW = static_cast<float>(mScreenW);
    const float screenH = static_cast<float>(mScreenH);
    const float sourceW = static_cast<float>(std::max(1, mBackgroundW));
    const float sourceH = static_cast<float>(std::max(1, mBackgroundH));
    const float coverScale = std::max(screenW / sourceW, screenH / sourceH);
    const float scalePulse = 1.0f + 0.006f * std::sin(elapsed * 0.41f);
    const float drawW = sourceW * coverScale * scalePulse;
    const float drawH = sourceH * coverScale * scalePulse;
    const float drawX = (screenW - drawW) * 0.5f;
    const float drawY = (screenH - drawH) * 0.5f;

    const float alphaT = (std::sin(elapsed * mLayout.logoPulseSpeed) + 1.0f) * 0.5f;
    const float logoAlpha = mLayout.logoAlphaMin +
        (mLayout.logoAlphaMax - mLayout.logoAlphaMin) * alphaT;

    const RECT backgroundRect = MakeRect(drawX, drawY, drawW, drawH);
    const RECT fullScreenRect = MakeRect(0.0f, 0.0f, screenW, screenH);

    BindViewport(context);
    mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred,
                        mStates->NonPremultiplied(),
                        mStates->LinearClamp(),
                        mStates->DepthNone());

    mSpriteBatch->Draw(mBackgroundSRV.Get(),
                       backgroundRect,
                       nullptr,
                       DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, logoAlpha));

    mSpriteBatch->Draw(mFillSRV.Get(),
                       fullScreenRect,
                       nullptr,
                       DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, mLayout.backgroundDimAlpha));

    mSpriteBatch->End();
}

// ------------------------------------------------------------
// Function: DrawFillRect
// Purpose:
//   Draw a tintable rectangle in screen pixels.
// Why:
//   Selection highlights and text shadow bands are simple filled quads.
// ------------------------------------------------------------
void TitleMenuRenderer::DrawFillRect(ID3D11DeviceContext* context,
                                     float x,
                                     float y,
                                     float width,
                                     float height,
                                     DirectX::FXMVECTOR color)
{
    if (!mSpriteBatch || !mStates || !mFillSRV) return;
    if (width <= 0.0f || height <= 0.0f) return;

    const RECT rect = MakeRect(x, y, width, height);

    BindViewport(context);
    mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred,
                        mStates->NonPremultiplied(),
                        mStates->PointClamp(),
                        mStates->DepthNone());
    mSpriteBatch->Draw(mFillSRV.Get(), rect, nullptr, color);
    mSpriteBatch->End();
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw the current title-menu visual phase.
// Why:
//   MenuState supplies input-derived view data; this function keeps all
//   title drawing behind one stable interface.
// ------------------------------------------------------------
void TitleMenuRenderer::Render(ID3D11DeviceContext* context,
                               const TitleMenuRenderState& state)
{
    if (!mInitialized || !context) return;

    DrawBackdrop(context, state.elapsed);

    if (state.phase == TitleMenuVisualPhase::LoadSlots)
    {
        RenderLoadSlots(context, state);
        return;
    }

    RenderMainOptions(context, state);
}

// ------------------------------------------------------------
// Function: RenderMainOptions
// Purpose:
//   Draw the main title command list.
// Why:
//   New Game, Continue, Load Slot, and Quit are the player's first choices.
// ------------------------------------------------------------
void TitleMenuRenderer::RenderMainOptions(ID3D11DeviceContext* context,
                                          const TitleMenuRenderState& state)
{
    const float screenW = static_cast<float>(mScreenW);
    const float screenH = static_cast<float>(mScreenH);
    const float panelW = std::min(mLayout.mainPanelWidth, screenW - kPanelMargin * 2.0f);
    const float panelH = std::min(mLayout.mainPanelHeight, screenH - kPanelMargin * 2.0f);
    float panelX = screenW - panelW - mLayout.mainPanelRight;
    float panelY = screenH - panelH - mLayout.mainPanelBottom;
    panelX = std::max(kPanelMargin, panelX);
    panelY = std::max(kPanelMargin, panelY);

    const DirectX::XMVECTOR panelColor =
        DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, mLayout.panelAlpha);
    mDialogBox.Draw(context, panelX, panelY, panelW, panelH,
                    1.0f, DirectX::XMMatrixIdentity(), panelColor);

    const float highlightX = panelX + kHighlightInsetX;
    const float highlightW = panelW - kHighlightInsetX * 2.0f;
    const float pulse = 0.62f + 0.18f * std::sin(state.elapsed * 6.0f);
    if (state.cursor >= 0 && state.cursor < static_cast<int>(state.options.size()))
    {
        const float rowY = panelY + mLayout.optionStartY +
            static_cast<float>(state.cursor) * mLayout.optionRowHeight - 6.0f;
        DrawFillRect(context, highlightX, rowY, highlightW, kHighlightH,
                     DirectX::XMVectorSet(0.78f, 0.54f, 0.28f, pulse));
    }

    mTextRenderer.BeginBatch(context);
    mTextRenderer.DrawStringCenteredRaw("Main Menu",
                                        panelX + panelW * 0.5f,
                                        panelY + kHeaderY,
                                        DirectX::Colors::White,
                                        1.22f,
                                        true);
    mTextRenderer.DrawStringCenteredRaw("Choose the next step.",
                                        panelX + panelW * 0.5f,
                                        panelY + kSubtitleY,
                                        DirectX::Colors::Silver);

    const float labelX = panelX + mLayout.optionStartX;
    const float labelY = panelY + mLayout.optionStartY;
    for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
    {
        const TitleMenuOptionView& option = state.options[static_cast<size_t>(i)];
        const bool selected = (i == state.cursor);
        const float rowY = labelY + static_cast<float>(i) * mLayout.optionRowHeight;
        const DirectX::XMVECTOR color = !option.enabled
            ? DirectX::Colors::Gray
            : (selected
                ? DirectX::XMVectorSet(1.0f, 0.92f, 0.58f, 1.0f)
                : DirectX::Colors::LightGray);

        if (selected)
        {
            mTextRenderer.DrawStringRaw(">", labelX - kSelectorOffsetX, rowY, color);
        }
        mTextRenderer.DrawStringRaw(option.label.c_str(), labelX, rowY, color);
    }

    if (!state.flashMessage.empty() && state.flashAlpha > 0.0f)
    {
        mTextRenderer.DrawStringCenteredRaw(state.flashMessage.c_str(),
                                            panelX + panelW * 0.5f,
                                            panelY + panelH - 36.0f,
                                            DirectX::XMVectorSet(0.72f, 1.0f, 0.72f,
                                                                  Clamp01(state.flashAlpha)));
    }

    mTextRenderer.EndBatch();
}

// ------------------------------------------------------------
// Function: RenderLoadSlots
// Purpose:
//   Draw the load-slot selection screen.
// Why:
//   Save-slot metadata should be visible before the player commits to a load.
// ------------------------------------------------------------
void TitleMenuRenderer::RenderLoadSlots(ID3D11DeviceContext* context,
                                        const TitleMenuRenderState& state)
{
    const float screenW = static_cast<float>(mScreenW);
    const float screenH = static_cast<float>(mScreenH);
    const float panelW = std::min(mLayout.slotPanelWidth, screenW - kPanelMargin * 2.0f);
    const float panelH = std::min(mLayout.slotPanelHeight, screenH - kPanelMargin * 2.0f);
    const float panelX = (screenW - panelW) * 0.5f;
    float panelY = screenH - panelH - mLayout.slotPanelBottom;
    panelY = std::max(kPanelMargin, panelY);

    const DirectX::XMVECTOR panelColor =
        DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, mLayout.panelAlpha);
    mDialogBox.Draw(context, panelX, panelY, panelW, panelH,
                    1.0f, DirectX::XMMatrixIdentity(), panelColor);

    const float highlightX = panelX + 42.0f;
    const float highlightW = panelW - 84.0f;
    const float pulse = 0.56f + 0.18f * std::sin(state.elapsed * 6.0f);
    if (state.slotCursor >= 0 && state.slotCursor < static_cast<int>(state.slots.size()))
    {
        const float rowY = panelY + mLayout.slotStartY +
            static_cast<float>(state.slotCursor) * mLayout.slotRowHeight - 9.0f;
        DrawFillRect(context, highlightX, rowY, highlightW, 58.0f,
                     DirectX::XMVectorSet(0.78f, 0.54f, 0.28f, pulse));
    }

    mTextRenderer.BeginBatch(context);
    mTextRenderer.DrawStringCenteredRaw("Load Game",
                                        panelX + panelW * 0.5f,
                                        panelY + kHeaderY,
                                        DirectX::Colors::White,
                                        1.22f,
                                        true);
    mTextRenderer.DrawStringCenteredRaw("Select a save slot.",
                                        panelX + panelW * 0.5f,
                                        panelY + kSubtitleY,
                                        DirectX::Colors::Silver);

    const float rowX = panelX + mLayout.slotStartX;
    const float rowY = panelY + mLayout.slotStartY;
    for (int i = 0; i < static_cast<int>(state.slots.size()); ++i)
    {
        const TitleMenuSlotView& slot = state.slots[static_cast<size_t>(i)];
        const bool selected = (i == state.slotCursor);
        const float currentY = rowY + static_cast<float>(i) * mLayout.slotRowHeight;
        const DirectX::XMVECTOR primaryColor = !slot.exists
            ? DirectX::Colors::Gray
            : (selected
                ? DirectX::XMVectorSet(1.0f, 0.92f, 0.58f, 1.0f)
                : DirectX::Colors::White);
        const DirectX::XMVECTOR secondaryColor = !slot.exists
            ? DirectX::Colors::DimGray
            : (slot.active ? DirectX::Colors::PaleGreen : DirectX::Colors::LightGray);

        if (selected)
        {
            mTextRenderer.DrawStringRaw(">", rowX - kSelectorOffsetX, currentY, primaryColor);
        }

        mTextRenderer.DrawStringRaw(slot.primary.c_str(), rowX, currentY, primaryColor);
        mTextRenderer.DrawStringRaw(ShortenText(slot.secondary, kSlotSecondaryLimit).c_str(),
                                    rowX + 24.0f,
                                    currentY + 25.0f,
                                    secondaryColor);
    }

    if (!state.flashMessage.empty() && state.flashAlpha > 0.0f)
    {
        mTextRenderer.DrawStringCenteredRaw(state.flashMessage.c_str(),
                                            panelX + panelW * 0.5f,
                                            panelY + panelH - 34.0f,
                                            DirectX::XMVectorSet(0.72f, 1.0f, 0.72f,
                                                                  Clamp01(state.flashAlpha)));
    }

    mTextRenderer.EndBatch();
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Release all GPU resources owned by the title menu renderer.
// Why:
//   DirectX resources should be released when the title state leaves the
//   state stack.
// ------------------------------------------------------------
void TitleMenuRenderer::Shutdown()
{
    mTextRenderer.Shutdown();
    mDialogBox.Shutdown();
    mSpriteBatch.reset();
    mStates.reset();
    mFillSRV.Reset();
    mBackgroundSRV.Reset();
    mInitialized = false;
}

// ------------------------------------------------------------
// Function: ToWidePath
// Purpose:
//   Convert project ASCII asset paths to wide strings for WIC loaders.
// Why:
//   DirectXTK texture loaders use Windows wide-character file APIs.
// ------------------------------------------------------------
std::wstring TitleMenuRenderer::ToWidePath(const std::string& path)
{
    return std::wstring(path.begin(), path.end());
}

// ------------------------------------------------------------
// Function: ShortenText
// Purpose:
//   Fit long save reasons into the slot panel.
// Why:
//   Save reasons are data strings and can grow beyond the available row width.
// ------------------------------------------------------------
std::string TitleMenuRenderer::ShortenText(const std::string& text, size_t limit)
{
    if (text.size() <= limit) return text;
    if (limit <= 3) return text.substr(0, limit);
    return text.substr(0, limit - 3) + "...";
}
