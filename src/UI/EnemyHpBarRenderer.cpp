// ============================================================
// File: EnemyHpBarRenderer.cpp
// Responsibility: Three-layer enemy HP bar renderer (top-centered, height-scaled).
//
// Rendering technique:
//   Scale is derived from kTargetBarHeight (desired screen pixels tall):
//     scaleX = scaleY = kTargetBarHeight / texH
//   The bar width follows the texture aspect ratio (no independent X scale).
//   This keeps bars compact and pixel-art-sharp at all resolutions.
//
//   Three-pass rendering per frame (all active bars per pass, not per-bar):
//     Pass 1 — NonPremultiplied: draw mBgSRV    (enemy-hp-ui-background.png)
//     Pass 2 — Opaque:           draw mFillSRV  (1x1 white tinted red, scaled to ratio)
//     Pass 3 — NonPremultiplied: draw mFrameSRV (enemy-hp-ui.png chrome/frame)
//   Three Begin/End pairs per frame regardless of active bar count.
//
// Fill geometry:
//     fillPosX = barPosX + mHpLeft  * scaleX
//     fillPosY = barPosY + mHpTop   * scaleX
//     fillW    = (mHpRight - mHpLeft) * scaleX * ratio
//     fillH    = (mHpBottom - mHpTop) * scaleX
//
// Common mistakes:
//   1. Using separate scaleX and scaleY — would distort the pixel-art end-caps.
//   2. Forgetting to multiply fill coordinates by scaleX — fill misaligns.
//   3. Calling Begin/End inside the slot loop — 2*N or 3*N pairs instead of 3.
// ============================================================
#include "EnemyHpBarRenderer.h"
#include "../Utils/Log.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#undef min
#undef max

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ============================================================
// BindViewport (private helper)
// ============================================================
void EnemyHpBarRenderer::BindViewport(ID3D11DeviceContext* context)
{
    // Build and bind the viewport from stored screen dimensions.
    // Also push it into SpriteBatch via SetViewport() so SpriteBatch's
    // internal GetViewportTransform() does not call RSGetViewports() — which
    // can return 0 viewports after another renderer unsets the RS state,
    // causing SpriteBatch::End() to throw std::runtime_error.
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
    mSpriteBatch->SetViewport(vp);
}

// ============================================================
// Initialize
// ============================================================
bool EnemyHpBarRenderer::Initialize(ID3D11Device*        device,
                                     ID3D11DeviceContext*  context,
                                     const std::wstring&  bgTexturePath,
                                     const std::wstring&  frameTexturePath,
                                     const std::string&   configJsonPath,
                                     int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;

    // ----------------------------------------------------------------
    // 1. Parse layout config from JSON.
    //    minimal token search; no third-party library dependency.
    //    If the file is missing, member defaults (128x16, hp=[16,5]-[111,9])
    //    already match the shipped enemy-hp-ui.json.
    // ----------------------------------------------------------------
    {
        std::ifstream file(configJsonPath);
        if (file.is_open())
        {
            std::ostringstream buf;
            buf << file.rdbuf();
            const std::string src = buf.str();

            // Return the integer value that follows "key": in src.
            auto parseInt = [&](const std::string& key, int def) -> int
            {
                const std::string searchKey = "\"" + key + "\"";
                size_t kpos = src.find(searchKey);
                if (kpos == std::string::npos) return def;
                size_t colon = src.find(':', kpos + searchKey.size());
                if (colon == std::string::npos) return def;
                size_t vs = colon + 1;
                while (vs < src.size() && (src[vs] == ' ' || src[vs] == '\t')) ++vs;
                try { return std::stoi(src.substr(vs)); }
                catch (...) { return def; }
            };

            // Extract [x, y] integer array for "key": [x, y].
            auto parseVec2 = [&](const std::string& key, int& outX, int& outY)
            {
                const std::string searchKey = "\"" + key + "\"";
                size_t kpos = src.find(searchKey);
                if (kpos == std::string::npos) return;
                size_t bracket = src.find('[', kpos + searchKey.size());
                size_t close   = src.find(']', bracket);
                if (bracket == std::string::npos || close == std::string::npos) return;
                const std::string inner = src.substr(bracket + 1, close - bracket - 1);
                size_t comma = inner.find(',');
                if (comma == std::string::npos) return;
                try {
                    outX = std::stoi(inner.substr(0, comma));
                    outY = std::stoi(inner.substr(comma + 1));
                } catch (...) {}
            };

            mTexW = parseInt("width",  mTexW);
            mTexH = parseInt("height", mTexH);
            parseVec2("health_bar_topleft",     mHpLeft,  mHpTop);
            parseVec2("health_bar_bottomright", mHpRight, mHpBottom);

            LOG("[EnemyHpBarRenderer] Config: tex=%dx%d, HP fill [%d,%d]->[%d,%d]",
                mTexW, mTexH, mHpLeft, mHpTop, mHpRight, mHpBottom);
        }
        else
        {
            LOG("[EnemyHpBarRenderer] '%s' not found — using defaults.", configJsonPath.c_str());
        }
    }

    // ----------------------------------------------------------------
    // 2. Load background texture (enemy-hp-ui-background.png).
    //    WIC_LOADER_IGNORE_SRGB: load raw 8-bit pixels, skip gamma conversion.
    // ----------------------------------------------------------------
    HRESULT hr = CreateWICTextureFromFileEx(
        device, context,
        bgTexturePath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mBgSRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        LOG("[EnemyHpBarRenderer] Failed to load background texture (0x%08X)", (unsigned)hr);
        return false;
    }
    LOG("[EnemyHpBarRenderer] Background texture loaded.");

    // ----------------------------------------------------------------
    // 3. Load frame/chrome texture (enemy-hp-ui.png).
    //    Drawn last so the decorative chrome sits on top of the fill.
    // ----------------------------------------------------------------
    hr = CreateWICTextureFromFileEx(
        device, context,
        frameTexturePath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mFrameSRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        LOG("[EnemyHpBarRenderer] Failed to load frame texture (0x%08X)", (unsigned)hr);
        return false;
    }
    LOG("[EnemyHpBarRenderer] Frame texture loaded.");

    // ----------------------------------------------------------------
    // 4. Create 1x1 white fill texture.
    //    Color is applied at draw time via the SpriteBatch tint parameter
    //    (red for HP, shield color for shield).  Avoids a separate fill PNG.
    // ----------------------------------------------------------------
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = td.Height = 1;
        td.MipLevels         = 1;
        td.ArraySize         = 1;
        td.Format            = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count  = 1;
        td.Usage             = D3D11_USAGE_IMMUTABLE;
        td.BindFlags         = D3D11_BIND_SHADER_RESOURCE;

        const UINT32    white    = 0xFFFFFFFF;
        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem     = &white;
        init.SysMemPitch = sizeof(UINT32);

        ComPtr<ID3D11Texture2D> tex;
        hr = device->CreateTexture2D(&td, &init, tex.GetAddressOf());
        if (FAILED(hr))
        {
            LOG("[EnemyHpBarRenderer] Failed 1x1 texture (0x%08X)", (unsigned)hr);
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
        srvd.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvd.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(tex.Get(), &srvd, mFillSRV.GetAddressOf());
        if (FAILED(hr))
        {
            LOG("[EnemyHpBarRenderer] Failed fill SRV (0x%08X)", (unsigned)hr);
            return false;
        }
        LOG("[EnemyHpBarRenderer] 1x1 fill texture created.");
    }

    // ----------------------------------------------------------------
    // 5. Create SpriteBatch and CommonStates.
    //    SpriteBatch is bound to 'context'; all Begin/End must use the
    //    same context pointer passed here.
    // ----------------------------------------------------------------
    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates      = std::make_unique<CommonStates>(device);

    LOG("[EnemyHpBarRenderer] Initialized. Screen: %dx%d", screenW, screenH);
    return true;
}

// ============================================================
// SetEnemy — called every frame by BattleState
// ============================================================
void EnemyHpBarRenderer::SetEnemy(int slot, float hp, float maxHp, bool active)
{
    if (slot < 0 || slot >= kMaxSlots) return;

    mSlotActive[slot] = active;
    mMaxHP[slot]      = (maxHp > 0.0f) ? maxHp : 1.0f;
    
    // If HP decreases, reset the delay timer for the catch-up white bar
    if (hp < mTargetHP[slot])
    {
        mDelayTimer[slot] = 0.0f;
        mEffectState[slot].TriggerShake();
    }
    mTargetHP[slot]   = hp;

    // Seed displayed value on first encounter so the bar opens at the
    // correct fill level instead of lerping upward from 0 each OnEnter().
    if (mRedHP[slot] == 0.0f && hp > 0.0f)
    {
        mRedHP[slot]   = hp;
        mWhiteHP[slot] = hp;
    }
}

// ============================================================
// SetTargetScale
// ============================================================
void EnemyHpBarRenderer::SetTargetScale(int slot, float scale)
{
    if (slot >= 0 && slot < kMaxSlots)
    {
        mEffectState[slot].SetTargetScale(scale);
    }
}

// ============================================================
// Update
// ============================================================
void EnemyHpBarRenderer::Update(float dt)
{
    if (!IsInitialized()) return;

    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mSlotActive[i]) continue;

        // Exponential approach: fast for red front bar
        const float redGap = mTargetHP[i] - mRedHP[i];
        mRedHP[i] += redGap * kRedLerpSpeed * dt;
        if (std::abs(redGap) < 0.5f) mRedHP[i] = mTargetHP[i];

        // Delayed catch-up for white background bar
        if (mWhiteHP[i] > mTargetHP[i])
        {
            mDelayTimer[i] += dt;
            if (mDelayTimer[i] >= kDelayDuration)
            {
                const float whiteGap = mTargetHP[i] - mWhiteHP[i];
                mWhiteHP[i] += whiteGap * kWhiteLerpSpeed * dt;
                if (std::abs(whiteGap) < 0.5f) mWhiteHP[i] = mTargetHP[i];
            }
        }
        else
        {
            // Fully caught up or healed
            mWhiteHP[i] = mTargetHP[i];
            mDelayTimer[i] = 0.0f;
        }

        mEffectState[i].Update(dt);
    }
}

// ============================================================
// Render
// ============================================================
void EnemyHpBarRenderer::Render(ID3D11DeviceContext* context)
{
    if (!IsInitialized()) return;

    // scaleY is height-driven: bar height = kTargetBarHeight pixels exactly.
    // scaleX is width-driven: bar width = screenW * kBarWidthFactor pixels.
    // The two axes are independent so height stays compact while width fills
    // ~60% of the screen — SpriteBatch accepts XMFLOAT2 scale so this is free.
    const float baseScaleY  = kTargetBarHeight / static_cast<float>(mTexH);
    const float scaledW = static_cast<float>(mScreenW) * kBarWidthFactor;
    const float baseScaleX  = scaledW / static_cast<float>(mTexW);
    const float scaledH = kTargetBarHeight;   // = mTexH * baseScaleY by definition

    // Per-slot vertical stride = name label height + gap + bar height + spacing.
    // Adding kNameLineHeight and kNameGapY ensures the name label above slot i+1
    // does not overlap the bar of slot i.
    const float slotStride = kNameLineHeight + kNameGapY + scaledH + kBarSpacing;

    BindViewport(context);

    const RECT     srcFull  = { 0, 0, mTexW, mTexH };
    const XMFLOAT2 origin   = { 0.0f, 0.0f };

    // ----------------------------------------------------------------
    // Pass 1 — Background (enemy-hp-ui-background.png).
    //   Drawn first so the fill and chrome render on top.
    //   NonPremultiplied blend: the PNG stores straight alpha.
    //   LinearClamp: smooth at fractional scale values.
    // ----------------------------------------------------------------
    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->LinearClamp(),
        mStates->DepthNone()
    );
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mSlotActive[i]) continue;
        
        const float slotScale = mEffectState[i].GetScale();
        const float barScaleX = baseScaleX * slotScale;
        const float barScaleY = baseScaleY * slotScale;
        const float barPosX = (static_cast<float>(mScreenW) - scaledW * slotScale) * 0.5f + mEffectState[i].GetOffsetX();

        const float barPosY = kTopPadding + static_cast<float>(i) * slotStride + mEffectState[i].GetOffsetY();
        mSpriteBatch->Draw(mBgSRV.Get(),
                           XMFLOAT2(barPosX, barPosY),
                           &srcFull,
                           Colors::White,
                           0.0f, origin, XMFLOAT2(barScaleX, barScaleY));
    }
    mSpriteBatch->End();

    // ----------------------------------------------------------------
    // Pass 2 — HP fill quads (1x1 white tinted red, scaled to ratio).
    //   Opaque blend + PointClamp: the fill is solid color; nearest-neighbour
    //   is correct and avoids a semi-transparent fringe on the fill edge.
    //
    //   Fill bounds in screen pixels:
    //     fillPosX = barPosX + mHpLeft  * scaleX   (follows horizontal stretch)
    //     fillPosY = barPosY + mHpTop   * scaleY   (follows fixed height)
    //     fillW    = (mHpRight - mHpLeft) * scaleX * ratio
    //     fillH    = (mHpBottom - mHpTop) * scaleY
    // ----------------------------------------------------------------
    static constexpr XMVECTORF32 kHpFillColor = { 200.f/255.f, 50.f/255.f, 50.f/255.f, 1.0f };

    const float hpRegionW = static_cast<float>(mHpRight  - mHpLeft);
    const float hpRegionH = static_cast<float>(mHpBottom - mHpTop);

    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->Opaque(),
        mStates->PointClamp(),
        mStates->DepthNone()
    );
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mSlotActive[i]) continue;

        const float slotScale = mEffectState[i].GetScale();
        const float barScaleX = baseScaleX * slotScale;
        const float barScaleY = baseScaleY * slotScale;
        const float barPosX = (static_cast<float>(mScreenW) - scaledW * slotScale) * 0.5f + mEffectState[i].GetOffsetX();

        const float barPosY   = kTopPadding + static_cast<float>(i) * slotStride + mEffectState[i].GetOffsetY();
        const float clampedRedHP   = std::max(0.0f, std::min(mRedHP[i], mMaxHP[i]));
        const float clampedWhiteHP = std::max(0.0f, std::min(mWhiteHP[i], mMaxHP[i]));

        const float redRatio   = clampedRedHP / mMaxHP[i];
        const float whiteRatio = clampedWhiteHP / mMaxHP[i];

        const float redFillW   = hpRegionW * barScaleX * redRatio;
        const float whiteFillW = hpRegionW * barScaleX * whiteRatio;

        if (whiteFillW < 1.0f) continue;

        const XMFLOAT2 fillPos(
            barPosX + static_cast<float>(mHpLeft) * barScaleX,  // horizontal: scaleX
            barPosY + static_cast<float>(mHpTop)  * barScaleY   // vertical:   scaleY
        );

        // Draw white background bar
        const XMVECTORF32 kWhiteFillColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        mSpriteBatch->Draw(mFillSRV.Get(),
                           fillPos, nullptr, kWhiteFillColor, 0.0f, origin,
                           XMFLOAT2(whiteFillW, hpRegionH * barScaleY));

        // Draw red front bar on top, if visible
        if (redFillW >= 1.0f)
        {
            mSpriteBatch->Draw(mFillSRV.Get(),
                               fillPos, nullptr, kHpFillColor, 0.0f, origin,
                               XMFLOAT2(redFillW, hpRegionH * barScaleY)); 
        }
    }
    mSpriteBatch->End();

    // ----------------------------------------------------------------
    // Pass 3 — Frame/chrome (enemy-hp-ui.png).
    //   Drawn last so the decorative border covers the fill edges.
    //   Same blend + sampler settings as Pass 1.
    // ----------------------------------------------------------------
    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->LinearClamp(),
        mStates->DepthNone()
    );
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mSlotActive[i]) continue;
        
        const float slotScale = mEffectState[i].GetScale();
        const float barScaleX = baseScaleX * slotScale;
        const float barScaleY = baseScaleY * slotScale;
        const float barPosX = (static_cast<float>(mScreenW) - scaledW * slotScale) * 0.5f + mEffectState[i].GetOffsetX();

        const float barPosY = kTopPadding + static_cast<float>(i) * slotStride + mEffectState[i].GetOffsetY();
        mSpriteBatch->Draw(mFrameSRV.Get(),
                           XMFLOAT2(barPosX, barPosY),
                           &srcFull,
                           Colors::White,
                           0.0f, origin, XMFLOAT2(barScaleX, barScaleY));
    }
    mSpriteBatch->End();

    // ----------------------------------------------------------------
    // Pass 4 — Enemy name labels (centered above each bar).
    //   Delegated to BattleTextRenderer which owns its own SpriteBatch.
    //   Uses the batch API (Begin + N draws + End) to keep the name pass
    //   to a single Begin/End regardless of active slot count.
    //   nameY = barPosY - kNameGapY - kNameLineHeight  (just above the bar).
    // ----------------------------------------------------------------
    if (mTextRenderer && mTextRenderer->IsReady())
    {
        mTextRenderer->BeginBatch(context);
        for (int i = 0; i < kMaxSlots; ++i)
        {
            if (!mSlotActive[i] || mEnemyName[i].empty()) continue;
            
            const float slotScale = mEffectState[i].GetScale();
            const float slotBarPosX = (static_cast<float>(mScreenW) - scaledW * slotScale) * 0.5f + mEffectState[i].GetOffsetX();
            const float barCenterX = slotBarPosX + (scaledW * slotScale) * 0.5f;

            const float barPosY = kTopPadding + static_cast<float>(i) * slotStride + mEffectState[i].GetOffsetY();
            const float nameY   = barPosY - kNameGapY - kNameLineHeight;
            // White text with a slight shadow by drawing twice: dark offset first.
            mTextRenderer->DrawStringCenteredRaw(
                mEnemyName[i].c_str(),
                barCenterX + 1.0f, nameY + 1.0f,
                DirectX::Colors::Black);
            mTextRenderer->DrawStringCenteredRaw(
                mEnemyName[i].c_str(),
                barCenterX, nameY,
                DirectX::Colors::White);
        }
        mTextRenderer->EndBatch();
    }
}

// ============================================================
// SetEnemyName / SetTextRenderer
// ============================================================
void EnemyHpBarRenderer::SetEnemyName(int slot, const std::string& name)
{
    if (slot >= 0 && slot < kMaxSlots)
        mEnemyName[slot] = name;
}

void EnemyHpBarRenderer::SetTextRenderer(BattleTextRenderer* textRenderer)
{
    mTextRenderer = textRenderer;
}

// ============================================================
// Shutdown
// ============================================================
void EnemyHpBarRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mBgSRV.Reset();     // enemy-hp-ui-background.png
    mFrameSRV.Reset();  // enemy-hp-ui.png
    mFillSRV.Reset();   // 1x1 white fill

    LOG("[EnemyHpBarRenderer] Shutdown complete.");
}
