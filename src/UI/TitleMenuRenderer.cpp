// ============================================================
// File: TitleMenuRenderer.cpp
// Responsibility: Implement the title screen visual presentation.
//
// Rendering pipeline:
//   1. Draw the title banner as a cover-scaled full-screen image.
//   2. Apply a subtle dark fill and drifting particles.
//   3. Draw either the press-start prompt, centered command list, or slots.
//   4. Draw the optional black transition overlay last.
//
// Common mistakes:
//   1. Passing a camera matrix to this renderer -> title UI is screen-space.
//   2. Pre-scaling the 1920x1080 logo asset -> cover math already handles it.
//   3. Recreating textures every frame -> Initialize owns all GPU resources.
// ============================================================
#define NOMINMAX
#include "TitleMenuRenderer.h"
#include "../Systems/LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <WICTextureLoader.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float kPanelMargin = 32.0f;
    constexpr float kHighlightH = 32.0f;
    constexpr size_t kSlotSecondaryLimit = 62;
    constexpr int kParticleCount = 56;
    constexpr int kMeterSegmentCount = 10;

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

    // ------------------------------------------------------------
    // Function: Hash01
    // Purpose:
    //   Generate a stable pseudo-random value in [0, 1).
    // Why:
    //   Title particles need deterministic placement without storing a
    //   per-particle array or introducing a random-number system.
    // ------------------------------------------------------------
    float Hash01(int index, float salt)
    {
        const float n = std::sin((static_cast<float>(index) * 12.9898f + salt) * 78.233f) * 43758.5453f;
        return n - std::floor(n);
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

    const std::string activeFontPath = LocalizationManager::Get().GetCurrentFontPath();
    const std::wstring fontPath = ToWidePath(activeFontPath.empty() ? mLayout.fontPath : activeFontPath);
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

bool TitleMenuRenderer::ReloadFont(ID3D11Device* device, ID3D11DeviceContext* context)
{
    const std::string activeFontPath = LocalizationManager::Get().GetCurrentFontPath();
    const std::wstring fontPath = ToWidePath(activeFontPath.empty() ? mLayout.fontPath : activeFontPath);
    return mTextRenderer.Initialize(device, context, fontPath, mScreenW, mScreenH);
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
    mLayout.fontPath = ReadJsonString(src, "fontPath", mLayout.fontPath);
    mLayout.bgmTrackId = ReadJsonString(src, "bgmTrackId", mLayout.bgmTrackId);
    mLayout.slotPanelWidth = ReadJsonFloat(src, "slotPanelWidth", mLayout.slotPanelWidth);
    mLayout.slotPanelHeight = ReadJsonFloat(src, "slotPanelHeight", mLayout.slotPanelHeight);
    mLayout.slotPanelBottom = ReadJsonFloat(src, "slotPanelBottom", mLayout.slotPanelBottom);
    mLayout.backgroundDimAlpha = Clamp01(ReadJsonFloat(src, "backgroundDimAlpha", mLayout.backgroundDimAlpha));
    mLayout.logoAlphaMin = Clamp01(ReadJsonFloat(src, "logoAlphaMin", mLayout.logoAlphaMin));
    mLayout.logoAlphaMax = Clamp01(ReadJsonFloat(src, "logoAlphaMax", mLayout.logoAlphaMax));
    mLayout.logoPulseSpeed = ReadJsonFloat(src, "logoPulseSpeed", mLayout.logoPulseSpeed);
    mLayout.particleAlpha = Clamp01(ReadJsonFloat(src, "particleAlpha", mLayout.particleAlpha));
    mLayout.pressPromptY = ReadJsonFloat(src, "pressPromptY", mLayout.pressPromptY);
    mLayout.pressPromptScale = ReadJsonFloat(src, "pressPromptScale", mLayout.pressPromptScale);
    mLayout.pressPromptBlinkSpeed = ReadJsonFloat(src, "pressPromptBlinkSpeed", mLayout.pressPromptBlinkSpeed);
    mLayout.optionStartY = ReadJsonFloat(src, "optionStartY", mLayout.optionStartY);
    mLayout.optionRowHeight = ReadJsonFloat(src, "optionRowHeight", mLayout.optionRowHeight);
    mLayout.optionTextScale = ReadJsonFloat(src, "optionTextScale", mLayout.optionTextScale);
    mLayout.optionsPanelWidth = ReadJsonFloat(src, "optionsPanelWidth", mLayout.optionsPanelWidth);
    mLayout.optionsPanelHeight = ReadJsonFloat(src, "optionsPanelHeight", mLayout.optionsPanelHeight);
    mLayout.optionsPanelBottom = ReadJsonFloat(src, "optionsPanelBottom", mLayout.optionsPanelBottom);
    mLayout.optionsPanelAlpha = Clamp01(ReadJsonFloat(src, "optionsPanelAlpha", mLayout.optionsPanelAlpha));
    mLayout.optionsBandAlpha = Clamp01(ReadJsonFloat(src, "optionsBandAlpha", mLayout.optionsBandAlpha));
    mLayout.optionsStartY = ReadJsonFloat(src, "optionsStartY", mLayout.optionsStartY);
    mLayout.optionsRowHeight = ReadJsonFloat(src, "optionsRowHeight", mLayout.optionsRowHeight);
    mLayout.optionsHighlightHeight = ReadJsonFloat(src, "optionsHighlightHeight", mLayout.optionsHighlightHeight);
    mLayout.optionsHighlightInset = ReadJsonFloat(src, "optionsHighlightInset", mLayout.optionsHighlightInset);
    mLayout.optionsLabelInset = ReadJsonFloat(src, "optionsLabelInset", mLayout.optionsLabelInset);
    mLayout.optionsValueRightInset = ReadJsonFloat(src, "optionsValueRightInset", mLayout.optionsValueRightInset);
    mLayout.optionsMeterRightInset = ReadJsonFloat(src, "optionsMeterRightInset", mLayout.optionsMeterRightInset);
    mLayout.optionsMeterSegmentWidth = ReadJsonFloat(src, "optionsMeterSegmentWidth", mLayout.optionsMeterSegmentWidth);
    mLayout.optionsMeterSegmentHeight = ReadJsonFloat(src, "optionsMeterSegmentHeight", mLayout.optionsMeterSegmentHeight);
    mLayout.optionsMeterSegmentGap = ReadJsonFloat(src, "optionsMeterSegmentGap", mLayout.optionsMeterSegmentGap);
    mLayout.optionsFutureTagOffset = ReadJsonFloat(src, "optionsFutureTagOffset", mLayout.optionsFutureTagOffset);
    mLayout.optionsFutureTagWidth = ReadJsonFloat(src, "optionsFutureTagWidth", mLayout.optionsFutureTagWidth);
    mLayout.optionsFutureTagHeight = ReadJsonFloat(src, "optionsFutureTagHeight", mLayout.optionsFutureTagHeight);
    mLayout.slotStartY = ReadJsonFloat(src, "slotStartY", mLayout.slotStartY);
    mLayout.slotRowHeight = ReadJsonFloat(src, "slotRowHeight", mLayout.slotRowHeight);
    mLayout.flashDuration = ReadJsonFloat(src, "flashDuration", mLayout.flashDuration);
    mLayout.transitionFadeOutDuration = ReadJsonFloat(src,
                                                       "transitionFadeOutDuration",
                                                       mLayout.transitionFadeOutDuration);

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
// Function: DrawAmbientParticles
// Purpose:
//   Draw slow drifting petal-like flecks over the dark title screen.
// Why:
//   The reference screen is atmospheric and sparse; lightweight particles
//   create motion without adding a new particle system or texture atlas.
// ------------------------------------------------------------
void TitleMenuRenderer::DrawAmbientParticles(ID3D11DeviceContext* context, float elapsed)
{
    if (!mSpriteBatch || !mStates || !mFillSRV) return;

    const float screenW = static_cast<float>(mScreenW);
    const float screenH = static_cast<float>(mScreenH);

    BindViewport(context);
    mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred,
                        mStates->NonPremultiplied(),
                        mStates->LinearClamp(),
                        mStates->DepthNone());

    for (int i = 0; i < kParticleCount; ++i)
    {
        const float lane = Hash01(i, 0.13f);
        const float depth = Hash01(i, 0.37f);
        const float speed = 10.0f + depth * 28.0f;
        const float baseY = Hash01(i, 0.71f) * (screenH + 96.0f);
        const float y = std::fmod(baseY + elapsed * speed, screenH + 96.0f) - 48.0f;
        const float sway = std::sin(elapsed * (0.25f + depth * 0.55f) + depth * 6.28318f) * (18.0f + depth * 42.0f);
        const float x = lane * screenW + sway;
        const float w = 2.0f + Hash01(i, 1.23f) * 7.0f;
        const float h = 1.0f + Hash01(i, 1.79f) * 3.0f;
        const float rotation = Hash01(i, 2.41f) * 6.28318f + elapsed * (0.12f + depth * 0.45f);
        const float warm = Hash01(i, 3.11f);
        const float alpha = mLayout.particleAlpha * (0.22f + depth * 0.72f);
        const DirectX::XMVECTOR color = (warm < 0.32f)
            ? DirectX::XMVectorSet(0.95f, 0.18f + warm * 0.55f, 0.26f, alpha)
            : DirectX::XMVectorSet(0.88f, 0.82f, 0.68f, alpha);

        mSpriteBatch->Draw(mFillSRV.Get(),
                           DirectX::XMFLOAT2(x, y),
                           nullptr,
                           color,
                           rotation,
                           DirectX::XMFLOAT2(0.5f, 0.5f),
                           DirectX::XMFLOAT2(w, h));
    }

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
// Function: DrawTransitionOverlay
// Purpose:
//   Draw the full-screen black fade used when leaving the title menu.
// Why:
//   MenuState decides when the scene hand-off is allowed, while the renderer
//   remains the only class that knows how to draw screen-space title quads.
// Parameters:
//   alpha - Black overlay opacity in [0, 1].
// ------------------------------------------------------------
void TitleMenuRenderer::DrawTransitionOverlay(ID3D11DeviceContext* context, float alpha)
{
    const float clampedAlpha = Clamp01(alpha);
    if (clampedAlpha <= 0.0f) return;

    DrawFillRect(context,
                 0.0f,
                 0.0f,
                 static_cast<float>(mScreenW),
                 static_cast<float>(mScreenH),
                 DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, clampedAlpha));
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
    DrawAmbientParticles(context, state.elapsed);

    if (state.phase == TitleMenuVisualPhase::PressStart)
    {
        RenderPressStart(context, state);
    }
    else if (state.phase == TitleMenuVisualPhase::NewGameSlots ||
             state.phase == TitleMenuVisualPhase::LoadSlots)
    {
        RenderLoadSlots(context, state);
    }
    else if (state.phase == TitleMenuVisualPhase::Options)
    {
        RenderOptions(context, state);
    }
    else
    {
        RenderMainOptions(context, state);
    }

    DrawTransitionOverlay(context, state.transitionAlpha);
}

// ------------------------------------------------------------
// Function: RenderPressStart
// Purpose:
//   Draw the reference-style centered press-start prompt.
// Why:
//   The first title screen should preserve the logo composition and avoid
//   menu chrome until the player asks to continue.
// ------------------------------------------------------------
void TitleMenuRenderer::RenderPressStart(ID3D11DeviceContext* context,
                                         const TitleMenuRenderState& state)
{
    const float screenW = static_cast<float>(mScreenW);
    const float y = std::min(mLayout.pressPromptY,
                             static_cast<float>(mScreenH) - 96.0f);
    const float pulse = (std::sin(state.elapsed * mLayout.pressPromptBlinkSpeed) + 1.0f) * 0.5f;
    const float alpha = 0.36f + pulse * 0.64f;

    const std::string prompt = LocalizationManager::Get().Text("menu.press_any_button");

    mTextRenderer.BeginBatch(context);
    mTextRenderer.DrawStringCenteredRaw(prompt.c_str(),
                                        screenW * 0.5f,
                                        y,
                                        DirectX::XMVectorSet(1.0f, 0.95f, 0.86f, alpha),
                                        mLayout.pressPromptScale,
                                        true);
    mTextRenderer.EndBatch();
}

// ------------------------------------------------------------
// Function: RenderMainOptions
// Purpose:
//   Draw the main title command list as centered text.
// Why:
//   The reference composition is clean and logo-led; a quiet text list keeps
//   the screen from becoming a floating dialog box.
// ------------------------------------------------------------
void TitleMenuRenderer::RenderMainOptions(ID3D11DeviceContext* context,
                                           const TitleMenuRenderState& state)
{
    const float screenW = static_cast<float>(mScreenW);
    const float screenH = static_cast<float>(mScreenH);
    const float listY = std::min(mLayout.optionStartY, screenH - 210.0f);
    const float highlightW = std::min(360.0f, screenW - kPanelMargin * 2.0f);
    const float highlightX = (screenW - highlightW) * 0.5f;
    const float pulse = 0.22f + 0.10f * std::sin(state.elapsed * 5.4f);

    if (state.cursor >= 0 && state.cursor < static_cast<int>(state.options.size()))
    {
        const float rowY = listY +
            static_cast<float>(state.cursor) * mLayout.optionRowHeight - 6.0f;
        DrawFillRect(context, highlightX, rowY, highlightW, kHighlightH,
                     DirectX::XMVectorSet(0.82f, 0.56f, 0.24f, pulse));
    }

    mTextRenderer.BeginBatch(context);
    for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
    {
        const TitleMenuOptionView& option = state.options[static_cast<size_t>(i)];
        const bool selected = (i == state.cursor);
        const float rowY = listY + static_cast<float>(i) * mLayout.optionRowHeight;
        const DirectX::XMVECTOR color = !option.enabled
            ? DirectX::Colors::DimGray
            : (selected
                ? DirectX::XMVectorSet(1.0f, 0.91f, 0.58f, 1.0f)
                : DirectX::XMVectorSet(0.86f, 0.84f, 0.78f, 0.86f));

        char label[96]{};
        if (selected)
        {
            std::snprintf(label, sizeof(label), ">  %s", option.label.c_str());
        }
        else
        {
            std::snprintf(label, sizeof(label), "%s", option.label.c_str());
        }

        mTextRenderer.DrawStringCenteredRaw(label,
                                            screenW * 0.5f,
                                            rowY,
                                            color,
                                            mLayout.optionTextScale,
                                            selected);
    }

    if (!state.flashMessage.empty() && state.flashAlpha > 0.0f)
    {
        mTextRenderer.DrawStringCenteredRaw(state.flashMessage.c_str(),
                                            screenW * 0.5f,
                                            listY + mLayout.optionRowHeight * 4.5f,
                                            DirectX::XMVectorSet(0.72f, 1.0f, 0.72f,
                                                                  Clamp01(state.flashAlpha)));
    }

    mTextRenderer.EndBatch();
}

// ------------------------------------------------------------
// Function: RenderOptions
// Purpose:
//   Draw the language and audio settings panel with value columns and
//   compact volume meters.
// Why:
//   The Options screen needs denser controls than the main command list
//   while preserving the quiet title composition.
// ------------------------------------------------------------
void TitleMenuRenderer::RenderOptions(ID3D11DeviceContext* context,
                                      const TitleMenuRenderState& state)
{
    const float screenW = static_cast<float>(mScreenW);
    const float screenH = static_cast<float>(mScreenH);
    const float panelW = std::min(mLayout.optionsPanelWidth, screenW - kPanelMargin * 2.0f);
    const float panelH = std::min(mLayout.optionsPanelHeight, screenH - kPanelMargin * 2.0f);
    const float panelX = (screenW - panelW) * 0.5f;
    float panelY = screenH - panelH - mLayout.optionsPanelBottom;
    panelY = std::max(kPanelMargin, panelY);

    DrawFillRect(context,
                 0.0f,
                 panelY - 20.0f,
                 screenW,
                 panelH + 40.0f,
                 DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, mLayout.optionsBandAlpha));
    DrawFillRect(context,
                 panelX,
                 panelY,
                 panelW,
                 panelH,
                 DirectX::XMVectorSet(0.03f, 0.03f, 0.035f, mLayout.optionsPanelAlpha));

    const float highlightW = std::max(0.0f, panelW - mLayout.optionsHighlightInset * 2.0f);
    const float pulse = 0.22f + 0.10f * std::sin(state.elapsed * 5.4f);
    if (state.cursor >= 0 && state.cursor < static_cast<int>(state.options.size()))
    {
        const float rowY = panelY + mLayout.optionsStartY +
            static_cast<float>(state.cursor) * mLayout.optionsRowHeight - 7.0f;
        DrawFillRect(context,
                     panelX + mLayout.optionsHighlightInset,
                     rowY,
                     highlightW,
                     mLayout.optionsHighlightHeight,
                     DirectX::XMVectorSet(0.82f, 0.56f, 0.24f, pulse));
    }

    const float segmentTotalW =
        static_cast<float>(kMeterSegmentCount) * mLayout.optionsMeterSegmentWidth +
        static_cast<float>(kMeterSegmentCount - 1) * mLayout.optionsMeterSegmentGap;
    const float meterX = panelX + panelW - mLayout.optionsMeterRightInset - segmentTotalW;

    for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
    {
        const TitleMenuOptionView& option = state.options[static_cast<size_t>(i)];
        if (!option.showMeter) continue;

        const bool selected = (i == state.cursor);
        const int filledSegments = static_cast<int>(
            std::round(Clamp01(option.meterValue) * static_cast<float>(kMeterSegmentCount)));
        const float rowCenterY = panelY + mLayout.optionsStartY +
            static_cast<float>(i) * mLayout.optionsRowHeight + 9.0f;
        const float segmentY = rowCenterY + 8.0f;

        for (int segment = 0; segment < kMeterSegmentCount; ++segment)
        {
            const bool filled = segment < filledSegments;
            const float segmentX = meterX +
                static_cast<float>(segment) *
                (mLayout.optionsMeterSegmentWidth + mLayout.optionsMeterSegmentGap);
            const float alpha = option.isFuture ? 0.72f : 1.0f;
            const DirectX::XMVECTOR color = filled
                ? DirectX::XMVectorSet(0.83f, 0.61f, 0.31f, selected ? alpha : alpha * 0.82f)
                : DirectX::XMVectorSet(0.42f, 0.40f, 0.36f, selected ? 0.46f : 0.28f);

            DrawFillRect(context,
                         segmentX,
                         segmentY,
                         mLayout.optionsMeterSegmentWidth,
                         mLayout.optionsMeterSegmentHeight,
                         color);
        }
    }

    for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
    {
        const TitleMenuOptionView& option = state.options[static_cast<size_t>(i)];
        if (!option.isFuture) continue;

        const bool selected = (i == state.cursor);
        const float rowY = panelY + mLayout.optionsStartY +
            static_cast<float>(i) * mLayout.optionsRowHeight;
        DrawFillRect(context,
                     panelX + mLayout.optionsLabelInset + mLayout.optionsFutureTagOffset,
                     rowY + 1.0f,
                     mLayout.optionsFutureTagWidth,
                     mLayout.optionsFutureTagHeight,
                     DirectX::XMVectorSet(0.58f, 0.50f, 0.34f, selected ? 0.24f : 0.14f));
    }

    mTextRenderer.BeginBatch(context);
    for (int i = 0; i < static_cast<int>(state.options.size()); ++i)
    {
        const TitleMenuOptionView& option = state.options[static_cast<size_t>(i)];
        const bool selected = (i == state.cursor);
        const float rowY = panelY + mLayout.optionsStartY +
            static_cast<float>(i) * mLayout.optionsRowHeight;
        const float labelAlpha = option.isFuture ? 0.72f : 0.92f;
        const DirectX::XMVECTOR labelColor = !option.enabled
            ? DirectX::Colors::DimGray
            : (selected
                ? DirectX::XMVectorSet(1.0f, 0.91f, 0.58f, 1.0f)
                : DirectX::XMVectorSet(0.88f, 0.86f, 0.80f, labelAlpha));
        const DirectX::XMVECTOR valueColor = option.isFuture
            ? DirectX::XMVectorSet(0.72f, 0.70f, 0.64f, selected ? 0.92f : 0.68f)
            : DirectX::XMVectorSet(0.92f, 0.88f, 0.78f, selected ? 1.0f : 0.82f);

        char label[128]{};
        std::snprintf(label, sizeof(label), selected ? ">  %s" : "%s", option.label.c_str());
        mTextRenderer.DrawStringRaw(label,
                                    panelX + mLayout.optionsLabelInset,
                                    rowY,
                                    labelColor);

        if (option.isFuture)
        {
            const float tagX = panelX + mLayout.optionsLabelInset + mLayout.optionsFutureTagOffset;
            const float tagY = rowY + 1.0f;
            mTextRenderer.DrawStringCenteredRaw(
                LocalizationManager::Get().Text("menu.option_future").c_str(),
                tagX + mLayout.optionsFutureTagWidth * 0.5f,
                tagY,
                DirectX::XMVectorSet(0.72f, 0.68f, 0.54f, selected ? 0.96f : 0.62f),
                0.78f,
                false);
        }

        if (!option.value.empty())
        {
            mTextRenderer.DrawStringRaw(option.value.c_str(),
                                        panelX + panelW - mLayout.optionsValueRightInset,
                                        rowY,
                                        valueColor);
        }
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
//   Draw the new-game or load-game slot selection screen.
// Why:
//   Save-slot metadata should be visible before the player creates or loads
//   a save file.
// ------------------------------------------------------------
void TitleMenuRenderer::RenderLoadSlots(ID3D11DeviceContext* context,
                                        const TitleMenuRenderState& state)
{
    const bool choosingNewGame = (state.phase == TitleMenuVisualPhase::NewGameSlots);
    const float screenW = static_cast<float>(mScreenW);
    const float screenH = static_cast<float>(mScreenH);
    const float panelW = std::min(mLayout.slotPanelWidth, screenW - kPanelMargin * 2.0f);
    const float panelH = std::min(mLayout.slotPanelHeight, screenH - kPanelMargin * 2.0f);
    const float panelX = (screenW - panelW) * 0.5f;
    float panelY = screenH - panelH - mLayout.slotPanelBottom;
    panelY = std::max(kPanelMargin, panelY);

    DrawFillRect(context, 0.0f, panelY - 28.0f, screenW, panelH + 56.0f,
                 DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.46f));

    const float highlightX = panelX + 56.0f;
    const float highlightW = panelW - 112.0f;
    const float pulse = 0.24f + 0.10f * std::sin(state.elapsed * 5.4f);
    if (state.slotCursor >= 0 && state.slotCursor < static_cast<int>(state.slots.size()))
    {
        const float rowY = panelY + mLayout.slotStartY +
            static_cast<float>(state.slotCursor) * mLayout.slotRowHeight - 9.0f;
        DrawFillRect(context, highlightX, rowY, highlightW, 58.0f,
                     DirectX::XMVectorSet(0.78f, 0.54f, 0.28f, pulse));
    }

    const std::string heading = choosingNewGame
        ? LocalizationManager::Get().Text("menu.new_game_slot")
        : LocalizationManager::Get().Text("menu.load_game");
    const std::string subtitle = choosingNewGame
        ? LocalizationManager::Get().Text("menu.choose_new_game_slot")
        : LocalizationManager::Get().Text("menu.choose_load_slot");

    mTextRenderer.BeginBatch(context);
    mTextRenderer.DrawStringCenteredRaw(heading.c_str(),
                                        panelX + panelW * 0.5f,
                                        panelY + 18.0f,
                                        DirectX::Colors::White,
                                        1.22f,
                                        true);
    mTextRenderer.DrawStringCenteredRaw(subtitle.c_str(),
                                        panelX + panelW * 0.5f,
                                        panelY + 48.0f,
                                        DirectX::Colors::Silver);

    const float rowY = panelY + mLayout.slotStartY;
    for (int i = 0; i < static_cast<int>(state.slots.size()); ++i)
    {
        const TitleMenuSlotView& slot = state.slots[static_cast<size_t>(i)];
        const bool selected = (i == state.slotCursor);
        const float currentY = rowY + static_cast<float>(i) * mLayout.slotRowHeight;
        const DirectX::XMVECTOR primaryColor = (!slot.exists && !choosingNewGame)
            ? DirectX::Colors::Gray
            : (selected
                ? DirectX::XMVectorSet(1.0f, 0.92f, 0.58f, 1.0f)
                : DirectX::Colors::White);
        const DirectX::XMVECTOR secondaryColor = !slot.exists
            ? DirectX::Colors::DimGray
            : (slot.active ? DirectX::Colors::PaleGreen : DirectX::Colors::LightGray);

        char primary[128]{};
        if (selected)
        {
            std::snprintf(primary, sizeof(primary), ">  %s", slot.primary.c_str());
        }
        else
        {
            std::snprintf(primary, sizeof(primary), "%s", slot.primary.c_str());
        }

        mTextRenderer.DrawStringCenteredRaw(primary,
                                            panelX + panelW * 0.5f,
                                            currentY,
                                            primaryColor,
                                            1.0f,
                                            selected);
        mTextRenderer.DrawStringCenteredRaw(ShortenText(slot.secondary, kSlotSecondaryLimit).c_str(),
                                            panelX + panelW * 0.5f,
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
