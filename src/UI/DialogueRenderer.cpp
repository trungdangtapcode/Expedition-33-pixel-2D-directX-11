// ============================================================
// File: DialogueRenderer.cpp
// Responsibility: Implement the story dialogue overlay renderer.
//
// Rendering pipeline:
//   1. Draw a light scrim so the dialogue text stays readable.
//   2. Draw the data-driven framed panel near the lower screen edge.
//   3. Draw speaker name, wrapped dialogue body, and line-advance prompt.
//
// Common mistakes:
//   1. Parsing dialogue text here duplicates DialogueManager responsibility.
//   2. Hardcoding panel dimensions here prevents language layout tuning.
//   3. Splitting UTF-8 text by raw bytes can corrupt localized glyphs.
// ============================================================
#define NOMINMAX
#include "DialogueRenderer.h"
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
        return JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(src, key),
            fallback);
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

    std::size_t Utf8GlyphCount(const std::string& text)
    {
        std::size_t count = 0;
        for (unsigned char c : text)
        {
            if ((c & 0xC0u) != 0x80u) ++count;
        }
        return count;
    }
}

DialogueRenderer::~DialogueRenderer()
{
    Shutdown();
}

bool DialogueRenderer::Initialize(ID3D11Device* device,
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

    mPanelRendererReady = false;
    if (!mLayout.panelTexturePath.empty() && !mLayout.panelJsonPath.empty())
    {
        mPanelRenderer = std::make_unique<NineSliceRenderer>();
        mPanelRendererReady = mPanelRenderer->Initialize(device,
                                                         context,
                                                         ToWidePath(mLayout.panelTexturePath),
                                                         mLayout.panelJsonPath,
                                                         mScreenW,
                                                         mScreenH);
        if (!mPanelRendererReady)
        {
            LOG("[DialogueRenderer] Panel asset failed to load; using rectangle fallback.");
            mPanelRenderer.reset();
        }
    }

    LOG("[DialogueRenderer] Initialized with '%s'.", layoutPath.c_str());
    return true;
}

void DialogueRenderer::SetScreenSize(int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
    mTextRenderer.SetScreenSize(screenW, screenH);
    if (mPanelRenderer)
    {
        mPanelRenderer->SetScreenSize(screenW, screenH);
    }
}

void DialogueRenderer::Render(ID3D11DeviceContext* context,
                              const DialogueRenderState& state)
{
    if (!mSpriteBatch || !mStates || !mFillSRV) return;

    DrawFillRect(context,
                 0.0f,
                 0.0f,
                 static_cast<float>(mScreenW),
                 static_cast<float>(mScreenH),
                 DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, Clamp01(mLayout.dimAlpha)));

    const float panelX = (static_cast<float>(mScreenW) - mLayout.panelWidth) * 0.5f;
    const float panelY = static_cast<float>(mScreenH) - mLayout.panelBottom - mLayout.panelHeight;
    DrawPanel(context, panelX, panelY, mLayout.panelWidth, mLayout.panelHeight, mLayout.panelAlpha);

    const std::size_t maxGlyphs =
        static_cast<std::size_t>(std::max(18.0f, mLayout.textMaxWidth / (8.5f * std::max(0.5f, mLayout.textScale))));
    const std::vector<std::string> lines = WrapText(state.text, maxGlyphs);

    const float promptAlpha = state.lineComplete
        ? 0.55f + 0.45f * (0.5f + 0.5f * std::sin(state.elapsed * mLayout.promptBlinkSpeed))
        : 0.0f;

    mTextRenderer.BeginBatch(context);
    mTextRenderer.DrawStringRawScaled(state.speakerName.c_str(),
                                      panelX + mLayout.speakerOffsetX,
                                      panelY + mLayout.speakerOffsetY,
                                      DirectX::XMVectorSet(0.96f, 0.84f, 0.56f, 1.0f),
                                      mLayout.speakerScale,
                                      true);

    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
    {
        mTextRenderer.DrawStringRawScaled(lines[i].c_str(),
                                          panelX + mLayout.textOffsetX,
                                          panelY + mLayout.textOffsetY + i * mLayout.lineHeight,
                                          DirectX::XMVectorSet(0.90f, 0.90f, 0.86f, 1.0f),
                                          mLayout.textScale,
                                          true);
    }

    if (promptAlpha > 0.0f)
    {
        mTextRenderer.DrawStringRawScaled(state.prompt.c_str(),
                                          panelX + mLayout.promptOffsetX,
                                          panelY + mLayout.promptOffsetY,
                                          DirectX::XMVectorSet(0.96f, 0.84f, 0.56f, promptAlpha),
                                          mLayout.promptScale,
                                          true);
    }
    mTextRenderer.EndBatch();
}

void DialogueRenderer::Shutdown()
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

bool DialogueRenderer::LoadLayout(const std::string& layoutPath)
{
    std::string src;
    if (!ReadTextFile(layoutPath, src))
    {
        LOG("[DialogueRenderer] Layout '%s' missing; using defaults.", layoutPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, layoutPath);

    mLayout.fontPath = ReadJsonString(src, "fontPath", mLayout.fontPath);
    mLayout.panelTexturePath = ReadJsonString(src, "panelTexturePath", mLayout.panelTexturePath);
    mLayout.panelJsonPath = ReadJsonString(src, "panelJsonPath", mLayout.panelJsonPath);
    mLayout.confirmSfxId = ReadJsonString(src, "confirmSfxId", mLayout.confirmSfxId);
    mLayout.backSfxId = ReadJsonString(src, "backSfxId", mLayout.backSfxId);
    mLayout.dimAlpha = Clamp01(ReadJsonFloat(src, "dimAlpha", mLayout.dimAlpha));
    mLayout.panelWidth = ReadJsonFloat(src, "panelWidth", mLayout.panelWidth);
    mLayout.panelHeight = ReadJsonFloat(src, "panelHeight", mLayout.panelHeight);
    mLayout.panelBottom = ReadJsonFloat(src, "panelBottom", mLayout.panelBottom);
    mLayout.panelAlpha = Clamp01(ReadJsonFloat(src, "panelAlpha", mLayout.panelAlpha));
    mLayout.panelSliceScale = std::max(0.1f, ReadJsonFloat(src, "panelSliceScale", mLayout.panelSliceScale));
    mLayout.panelFallbackBorder = ReadJsonFloat(src, "panelFallbackBorder", mLayout.panelFallbackBorder);
    mLayout.speakerOffsetX = ReadJsonFloat(src, "speakerOffsetX", mLayout.speakerOffsetX);
    mLayout.speakerOffsetY = ReadJsonFloat(src, "speakerOffsetY", mLayout.speakerOffsetY);
    mLayout.speakerScale = ReadJsonFloat(src, "speakerScale", mLayout.speakerScale);
    mLayout.textOffsetX = ReadJsonFloat(src, "textOffsetX", mLayout.textOffsetX);
    mLayout.textOffsetY = ReadJsonFloat(src, "textOffsetY", mLayout.textOffsetY);
    mLayout.textScale = ReadJsonFloat(src, "textScale", mLayout.textScale);
    mLayout.lineHeight = ReadJsonFloat(src, "lineHeight", mLayout.lineHeight);
    mLayout.textMaxWidth = ReadJsonFloat(src, "textMaxWidth", mLayout.textMaxWidth);
    mLayout.promptOffsetX = ReadJsonFloat(src, "promptOffsetX", mLayout.promptOffsetX);
    mLayout.promptOffsetY = ReadJsonFloat(src, "promptOffsetY", mLayout.promptOffsetY);
    mLayout.promptScale = ReadJsonFloat(src, "promptScale", mLayout.promptScale);
    mLayout.promptBlinkSpeed = ReadJsonFloat(src, "promptBlinkSpeed", mLayout.promptBlinkSpeed);
    mLayout.charsPerSecond = ReadJsonFloat(src, "charsPerSecond", mLayout.charsPerSecond);

    LOG("[DialogueRenderer] Layout loaded from '%s'.", layoutPath.c_str());
    return true;
}

bool DialogueRenderer::CreateFillTexture(ID3D11Device* device)
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
        LOG("[DialogueRenderer] Failed to create fill texture (0x%08X).",
            static_cast<unsigned>(hr));
        return false;
    }

    const HRESULT srvHr = device->CreateShaderResourceView(texture.Get(), nullptr, mFillSRV.GetAddressOf());
    if (FAILED(srvHr))
    {
        LOG("[DialogueRenderer] Failed to create fill SRV (0x%08X).",
            static_cast<unsigned>(srvHr));
        return false;
    }

    return true;
}

void DialogueRenderer::BindViewport(ID3D11DeviceContext* context)
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

void DialogueRenderer::DrawFillRect(ID3D11DeviceContext* context,
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

void DialogueRenderer::DrawPanel(ID3D11DeviceContext* context,
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

    const float border = std::max(1.0f, mLayout.panelFallbackBorder);
    DrawFillRect(context, x, y, width, height, DirectX::XMVectorSet(0.03f, 0.03f, 0.035f, alpha));
    DrawFillRect(context, x, y, width, border, DirectX::XMVectorSet(0.82f, 0.68f, 0.42f, 0.78f));
    DrawFillRect(context, x, y + height - border, width, border, DirectX::XMVectorSet(0.82f, 0.68f, 0.42f, 0.78f));
    DrawFillRect(context, x, y, border, height, DirectX::XMVectorSet(0.82f, 0.68f, 0.42f, 0.78f));
    DrawFillRect(context, x + width - border, y, border, height, DirectX::XMVectorSet(0.82f, 0.68f, 0.42f, 0.78f));
}

std::vector<std::string> DialogueRenderer::WrapText(const std::string& text,
                                                    std::size_t maxGlyphsPerLine) const
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string word;
    std::string current;

    while (stream >> word)
    {
        const std::size_t currentGlyphs = Utf8GlyphCount(current);
        const std::size_t wordGlyphs = Utf8GlyphCount(word);
        const std::size_t candidateGlyphs = current.empty()
            ? wordGlyphs
            : currentGlyphs + 1 + wordGlyphs;

        if (!current.empty() && candidateGlyphs > maxGlyphsPerLine)
        {
            lines.push_back(current);
            current = word;
            continue;
        }

        if (!current.empty()) current += " ";
        current += word;
    }

    if (!current.empty()) lines.push_back(current);
    if (lines.empty()) lines.push_back(std::string());
    return lines;
}

std::wstring DialogueRenderer::ToWidePath(const std::string& path)
{
    return std::wstring(path.begin(), path.end());
}
