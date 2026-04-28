// ============================================================
// File: HealthBarRenderer.cpp
// Responsibility: Implement the three-layer HP bar UI renderer.
//
// Rendering technique — CLIPPING, not scaling:
//   The HP fill is drawn by trimming the RIGHT edge of the source RECT:
//     srcRect.right = hpBarLeft + (int)(HpBarWidth * ratio)
//   DirectX samples only the pixels inside srcRect; the trimmed-right
//   pixels are simply not drawn.  The sprite is never stretched.
//
// Lerp smoothing:
//   mDisplayedHP tracks mTargetHP via exponential approach each Update().
//   The delta (mTargetHP - mDisplayedHP) shrinks by factor kLerpSpeed*dt
//   each frame, giving a smooth deceleration regardless of frame rate.
//
// Event flow:
//   BattleManager::Log() calls EventManager::Broadcast("verso_hp_changed")
//   with EventData.value = new HP (as float).
//   The lambda stored in mHpListenerID updates mTargetHP instantly.
//   mDisplayedHP then lerps toward it over subsequent frames.
//
// Common mistakes:
//   1. Drawing fill at world coords instead of screen coords — always
//      pass screen pixel coordinates (hpBarLeft, hpBarTop) to Draw().
//   2. Forgetting to clamp ratio to [0, 1] — negative HP would try to
//      draw a reversed srcRect and trigger a DX debug warning.
//   3. Calling Render() after Shutdown() — mSpriteBatch is null; guard
//      prevents crash but logs a warning.
// ============================================================
#include "HealthBarRenderer.h"
#include "../Utils/Log.h"
// Undefine Windows.h min/max macros that clash with std::min/std::max.
// These macros are injected by <Windows.h> (pulled in via DirectXTK headers)
// and cannot be blocked with NOMINMAX after the header is already included.
#undef min
#undef max
#include <algorithm>        // std::max, std::min

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ============================================================
// BindViewport (private)
// ============================================================
void HealthBarRenderer::BindViewport(ID3D11DeviceContext* context)
{
    // Build the viewport from stored screen dimensions and bind it to the RS stage.
    // Also forward it to SpriteBatch so it bypasses its own RSGetViewports() call.
    // If RSGetViewports() returns 0 viewports (can happen after another renderer
    // unsets the RS state), SpriteBatch::End() throws std::runtime_error.
    // SetViewport() sets an internal flag that short-circuits that codepath.
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
bool HealthBarRenderer::Initialize(ID3D11Device* device,
                                   ID3D11DeviceContext* context,
                                   const std::wstring& bgTexturePath,
                                   const std::wstring& frameTexPath,
                                   const std::string& configJsonPath,
                                   int screenW, int screenH,
                                   const std::string& hpEventTopic,
                                   float renderX, float renderY)
{
    mScreenW = screenW;
    mScreenH = screenH;
    mHpEventTopic = hpEventTopic;
    mRenderX = renderX;
    mRenderY = renderY;

    // -- 1. Load JSON config --
    if (!mConfig.LoadFromJson(configJsonPath))
        return false;

    // -- 2. Load background texture (UI_hp_background.png) --
    // WIC_LOADER_IGNORE_SRGB: tell WIC to ignore the embedded sRGB ICC profile
    // and load raw 8-bit values as-is.  Without this flag WIC detects the sRGB
    // profile, promotes the format to R8G8B8A8_UNORM_SRGB, and the GPU
    // linearises the values before they reach the UNORM backbuffer — producing
    // the "darker than expected" look (e.g. #B5E61D → #76CA03).
    HRESULT hr = CreateWICTextureFromFileEx(
        device, context,
        bgTexturePath.c_str(),
        0,                              // maxsize — 0 = auto
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,                           // no CPU access, no misc flags
        WIC_LOADER_IGNORE_SRGB,         // load raw pixel values, no gamma conversion
        nullptr,
        mBgSRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        LOG("[HealthBarRenderer] Failed to load background texture (0x%08X)", (unsigned)hr);
        return false;
    }
    LOG("[HealthBarRenderer] Background texture loaded.");

    // -- 3. Load frame + portrait texture (UI_verso_hp.png) --
    // Same IGNORE_SRGB flag — keeps pixel art colors identical to the source file.
    hr = CreateWICTextureFromFileEx(
        device, context,
        frameTexPath.c_str(),
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
        LOG("[HealthBarRenderer] Failed to load frame texture (0x%08X)", (unsigned)hr);
        return false;
    }
    LOG("[HealthBarRenderer] Frame + portrait texture loaded.");

    // -- 3.5. Load dead overlay texture --
    hr = CreateWICTextureFromFileEx(
        device, context,
        L"assets/UI/dead-overlay.png",
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mDeadOverlaySRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        LOG("[HealthBarRenderer] WARNING: Failed to load dead overlay asset (0x%08X)", (unsigned)hr);
        // Do not return false; it's just an overlay, skip gracefully.
    }
    
    // Parse mapping scalars natively inside JSON
    JsonLoader::LoadDeadOverlayConfig("assets/UI/dead-overlay.json", mDeadOverlayConfig);

    // -- 4. Create 1x1 white fill texture --
    // The BG PNG has no fill color — it is only a dark semi-transparent overlay.
    // We generate a 1x1 white D3D11 texture at runtime and tint it to the
    // HP-bar color (red) via the SpriteBatch Draw `color` parameter.
    // SpriteBatch scales the 1x1 sprite to (fillWidth, barHeight) via the
    // `scale` parameter, avoiding any external asset dependency for fill color.
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width            = 1;
        td.Height           = 1;
        td.MipLevels        = 1;
        td.ArraySize        = 1;
        td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage            = D3D11_USAGE_IMMUTABLE;
        td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

        // White opaque texel — color is applied via the Draw tint parameter.
        const UINT32 white = 0xFFFFFFFF;
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem     = &white;
        initData.SysMemPitch = sizeof(UINT32);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        HRESULT hr2 = device->CreateTexture2D(&td, &initData, tex.GetAddressOf());
        if (FAILED(hr2))
        {
            LOG("[HealthBarRenderer] Failed to create 1x1 fill texture (0x%08X)", (unsigned)hr2);
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
        // srvd.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvd.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvd.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;
        hr2 = device->CreateShaderResourceView(tex.Get(), &srvd, mFillSRV.GetAddressOf());
        if (FAILED(hr2))
        {
            LOG("[HealthBarRenderer] Failed to create fill SRV (0x%08X)", (unsigned)hr2);
            return false;
        }
        LOG("[HealthBarRenderer] 1x1 fill texture created.");
    }

    // -- 5. Create SpriteBatch --
    // Bound to 'context' — all Begin/End calls must use the same context.
    mSpriteBatch = std::make_unique<SpriteBatch>(context);

    // -- 6. Create CommonStates --
    // DepthNone() — disables depth testing so the HP bar is always visible
    //   even when drawn after CircleRenderer has written z=0 to the depth buffer.
    // NonPremultiplied() — correct alpha blend for WIC-loaded PNGs (SRC_ALPHA /
    //   INV_SRC_ALPHA), which store straight (non-premultiplied) alpha.
    mStates = std::make_unique<CommonStates>(device);

    // -- 7. Subscribe to "verso_hp_changed" events --
    // The lambda captures `this` — it is safe as long as HealthBarRenderer
    // outlives any broadcast.  Shutdown() calls Unsubscribe to enforce this.
    mHpListenerID = EventManager::Get().Subscribe(
        mHpEventTopic,
        [this](const EventData& data) {
            float hp = data.value;
            if (hp < mTargetHP)
            {
                // Reset delay timer heavily when fresh damage is taken
                mDelayTimer = 0.0f;
                // Trigger shake effect for UI
                mEffectState.TriggerShake();
            }
            // data.value = new HP as float (cast from int by BattleManager).
            mTargetHP = hp;
            LOG("[HealthBarRenderer] HP event received: target HP = %.0f / %.0f",
                mTargetHP, mMaxHP);
        }
    );

    LOG("[HealthBarRenderer] Initialized. Screen: %dx%d", screenW, screenH);
    return true;
}

// ============================================================
// SetHP / SetMaxHP
// ============================================================
void HealthBarRenderer::SetHP(float hp)
{
    // Seed both displayed and target so there is no lerp on the first frame.
    mTargetHP    = hp;
    mRedHP       = hp;
    mWhiteHP     = hp;
    mDelayTimer  = 0.0f;
}

void HealthBarRenderer::SetMaxHP(float maxHp)
{
    // Guard against divide-by-zero in ratio calculation.
    mMaxHP = (maxHp > 0.0f) ? maxHp : 1.0f;
}

// ============================================================
// SetScreenSize
// ============================================================
void HealthBarRenderer::SetScreenSize(int w, int h)
{
    mScreenW = w;
    mScreenH = h;
}

// ============================================================
// Update
// ============================================================
void HealthBarRenderer::Update(float dt)
{
    if (!IsInitialized()) return;
    
    // AAA Layout Hot-Reloading mapping cleanly (Poll JSON every 1 second explicitly)
    mHotReloadTimer += dt;
    if (mHotReloadTimer > 1.0f)
    {
        JsonLoader::LoadDeadOverlayConfig("assets/UI/dead-overlay.json", mDeadOverlayConfig);
        mHotReloadTimer = 0.0f;
    }

    // Fast approach for the red front bar
    const float redGap = mTargetHP - mRedHP;
    mRedHP += redGap * kRedLerpSpeed * dt;
    if (std::abs(redGap) < 0.5f) mRedHP = mTargetHP;

    // Delayed approach for the white background bar
    if (mWhiteHP > mTargetHP)
    {
        // We took damage, wait for the delay timer
        mDelayTimer += dt;
        if (mDelayTimer >= kDelayDuration)
        {
            const float whiteGap = mTargetHP - mWhiteHP;
            mWhiteHP += whiteGap * kWhiteLerpSpeed * dt;
            if (std::abs(whiteGap) < 0.5f) mWhiteHP = mTargetHP;
        }
    }
    else
    {
        // Healing or reset, align immediately securely
        mWhiteHP = mTargetHP;
    }

    // Evaluate AAA sink state mathematically evaluating death cleanly
    if (mTargetHP <= 0.0f)
    {
        // Lerp tint alpha natively dynamically reaching exactly 0.4 opacity over time
        mSinkAlpha = std::max(0.4f, mSinkAlpha - 1.5f * dt);
        // Lerp towards DarkGray tint for immersive blending structurally
        mTintColor.f[0] = std::max(DirectX::Colors::DarkGray.f[0], mTintColor.f[0] - 1.5f * dt);
        mTintColor.f[1] = std::max(DirectX::Colors::DarkGray.f[1], mTintColor.f[1] - 1.5f * dt);
        mTintColor.f[2] = std::max(DirectX::Colors::DarkGray.f[2], mTintColor.f[2] - 1.5f * dt);
    }
    else
    {
        mSinkAlpha = std::min(1.0f, mSinkAlpha + 3.0f * dt);
        mTintColor.f[0] = std::min(DirectX::Colors::White.f[0], mTintColor.f[0] + 3.0f * dt);
        mTintColor.f[1] = std::min(DirectX::Colors::White.f[1], mTintColor.f[1] + 3.0f * dt);
        mTintColor.f[2] = std::min(DirectX::Colors::White.f[2], mTintColor.f[2] + 3.0f * dt);
    }



    mEffectState.Update(dt);
}

// ============================================================
// Render
// ============================================================
void HealthBarRenderer::Render(ID3D11DeviceContext* context)
{
    if (!IsInitialized()) return;

    // Clamp values so ratios are always in [0, 1].
    const float clampedRedHP   = std::max(0.0f, std::min(mRedHP, mMaxHP));
    const float clampedWhiteHP = std::max(0.0f, std::min(mWhiteHP, mMaxHP));

    const float redRatio   = clampedRedHP / mMaxHP;
    const float whiteRatio = clampedWhiteHP / mMaxHP;

    // Compute fill widths
    const int redFillWidth   = static_cast<int>(mConfig.HpBarWidth() * redRatio);
    const int whiteFillWidth = static_cast<int>(mConfig.HpBarWidth() * whiteRatio);

    // Anchor the widget to the bottom-right corner of the screen.
    // Ensure anchor takes current scaling into account. Let's pivot around the bottom right corner.
    const float scale = mEffectState.GetScale();
    const float offsetX = mEffectState.GetOffsetX();
    const float offsetY = mEffectState.GetOffsetY();

    // Use dynamic coordinates supplied via Initialization
    const float originX = mRenderX + offsetX;
    const float originY = mRenderY + offsetY;

    // Bind viewport — must happen before Begin() so SpriteBatch's internal
    // GetViewportTransform() uses our stored dimensions instead of querying
    // the rasterizer (which may return 0 viewports after another renderer resets it).
    BindViewport(context);

    // ----------------------------------------------------------------
    // Layer 1: Background overlay (UI_hp_background.png)
    //   Semi-transparent dark shadow drawn over the entire bar area.
    //   The BG PNG contains NO fill color — it is only a dark overlay.
    //   Fill color comes from the 1x1 tinted quad in Layer 2.
    // ----------------------------------------------------------------
    // ----------------------------------------------------------------
    // Evaluate Tint Scale globally incorporating Sink states efficiently
    // ----------------------------------------------------------------
    DirectX::XMVECTORF32 dynamicColor = mTintColor;
    dynamicColor.f[3] = mSinkAlpha; // overwrite alpha strictly 

    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),  // BG has alpha=107 (42%) — needs alpha blend
        mStates->LinearClamp(),
        mStates->DepthNone()
    );
    {
        RECT srcFull = { 0, 0, mConfig.textureWidth, mConfig.textureHeight };
        const XMFLOAT2 pos(originX, originY);
        const XMFLOAT2 pivot(0.0f, 0.0f);
        mSpriteBatch->Draw(mBgSRV.Get(), pos, &srcFull,
                           dynamicColor, 0.0f, pivot, scale);
    }
    mSpriteBatch->End();

    // ----------------------------------------------------------------
    // Layer 2: HP Fill — solid color quad, scaled to fill area size.
    //   The BG PNG has no fill color (was removed from the source asset).
    //   We use the runtime-created 1x1 white texture and tint it.
    //   SpriteBatch scales the 1x1 texel to (fillWidth, barHeight) via
    //   the XMFLOAT2 scale parameter.
    //
    //   We render two layers here:
    //   1. White delayed bar (behind the red bar, draws full delayed width).
    //   2. Red front bar (draws on top of the white bar).
    // ----------------------------------------------------------------
    if (whiteFillWidth > 0 || redFillWidth > 0)
    {
        mSpriteBatch->Begin(
            SpriteSortMode_Deferred,
            mStates->Opaque(),       // fill is fully opaque — no blending needed
            mStates->PointClamp(),   // 1x1 texture — nearest-neighbour is correct
            mStates->DepthNone()
        );

        const XMFLOAT2 fillPos(
            originX + static_cast<float>(mConfig.hpBarLeft) * scale,
            originY + static_cast<float>(mConfig.hpBarTop) * scale
        );
        const XMFLOAT2 pivot(0.0f, 0.0f);
        
        // Draw the white bar beneath cleanly scaled with sink Alpha
        if (whiteFillWidth > 0)
        {
            const XMFLOAT2 whiteScale(
                static_cast<float>(whiteFillWidth) * scale,
                static_cast<float>(mConfig.HpBarHeight()) * scale
            );
            DirectX::XMVECTORF32 whiteColor = { 1.0f, 1.0f, 1.0f, mSinkAlpha };
            mSpriteBatch->Draw(mFillSRV.Get(), fillPos, nullptr,
                               whiteColor, 0.0f, pivot, whiteScale);
        }

        // Draw the red bar on top strictly aligned mapping sink
        if (redFillWidth > 0)
        {
            const XMFLOAT2 redScale(
                static_cast<float>(redFillWidth) * scale,
                static_cast<float>(mConfig.HpBarHeight()) * scale
            );
            // Apply generic Tint math maintaining Red proportion accurately!
            DirectX::XMVECTORF32 redColor = { 
                200.f/255.f * mTintColor.f[0], 
                50.f/255.f * mTintColor.f[1], 
                50.f/255.f * mTintColor.f[2], 
                mSinkAlpha 
            };
            mSpriteBatch->Draw(mFillSRV.Get(), fillPos, nullptr,
                               redColor, 0.0f, pivot, redScale);
        }

        mSpriteBatch->End();
    }

    // ----------------------------------------------------------------
    // Layer 3: Frame + Portrait (UI_verso_hp.png)
    //   Full 256x256 quad at (0, 0), drawn LAST so the decorative border
    //   and character portrait appear on top of the fill.
    // ----------------------------------------------------------------
    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),  // frame PNG has transparent window areas
        mStates->LinearClamp(),
        mStates->DepthNone()
    );
    {
        RECT srcFull = { 0, 0, mConfig.textureWidth, mConfig.textureHeight };
        const XMFLOAT2 pos(originX, originY);
        const XMFLOAT2 pivot(0.0f, 0.0f);
        mSpriteBatch->Draw(mFrameSRV.Get(), pos, &srcFull,
                           dynamicColor, 0.0f, pivot, scale);
                           
        // ----------------------------------------------------------------
        // Layer 4: Dead Overlay natively rendered on top if target passed structurally away
        // ----------------------------------------------------------------
        if (mTargetHP <= 0.0f && mDeadOverlaySRV)
        {
            // Dynamically scaled bounding mapping JSON natively securely inside
            const float overlayScale = (mDeadOverlayConfig.scaleTarget / mDeadOverlayConfig.width) * scale;
            
            // Fading alpha logic strictly mapping visually inwards
            DirectX::XMVECTORF32 overlayTint = DirectX::Colors::White;
            overlayTint.f[3] = 1.0f - ((mSinkAlpha - 0.4f) / 0.6f); // 0.0f to 1.0f inversely mapped relative to sink
            
            // Centralize Pivot natively aligning scaling math perfectly onto the absolute middle!
            const DirectX::XMFLOAT2 centerPos(
                pos.x + (mConfig.textureWidth / 2.0f) * scale + (mDeadOverlayConfig.offsetX * scale),
                pos.y + (mConfig.textureHeight / 2.0f) * scale + (mDeadOverlayConfig.offsetY * scale)
            );
            const DirectX::XMFLOAT2 customPivot(
                mDeadOverlayConfig.pivotX,
                mDeadOverlayConfig.pivotY
            );
            
            mSpriteBatch->Draw(mDeadOverlaySRV.Get(), centerPos, nullptr, overlayTint, 0.0f, customPivot, overlayScale);
        }
    }
    mSpriteBatch->End();
}

// ============================================================
// Shutdown
// ============================================================
void HealthBarRenderer::Shutdown()
{
    // Unsubscribe BEFORE releasing resources so the lambda cannot fire
    // after mTargetHP/mDisplayedHP go out of scope.
    if (mHpListenerID >= 0)
    {
        EventManager::Get().Unsubscribe(mHpEventTopic, mHpListenerID);
        mHpListenerID = -1;
    }

    mSpriteBatch.reset();   // releases VB/IB/CB
    mStates.reset();         // releases blend/depth/raster/sampler states
    mBgSRV.Reset();          // decrement GPU texture ref count
    mFillSRV.Reset();        // 1x1 white fill texture
    mFrameSRV.Reset();
    mDeadOverlaySRV.Reset(); // free overlay asset cleanly

    LOG("[HealthBarRenderer] Shutdown complete.");
}
