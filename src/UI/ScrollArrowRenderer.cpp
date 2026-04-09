// ============================================================
// File: ScrollArrowRenderer.cpp
// Responsibility: Load + draw a single chevron sprite with looping
//                 vertical bob, mirrorable to cover both up and down.
// ============================================================
#include "ScrollArrowRenderer.h"
#include "../Utils/Log.h"

#include <WICTextureLoader.h>
#include <cmath>

bool ScrollArrowRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                                       const std::wstring& texturePath,
                                       int screenW, int screenH,
                                       float bobSpeed, float bobAmplitude)
{
    Shutdown();

    mScreenW      = screenW;
    mScreenH      = screenH;
    mBobSpeed     = bobSpeed;
    mBobAmplitude = bobAmplitude;

    // Load the chevron texture.  WIC_LOADER_IGNORE_SRGB matches the
    // convention used by every other UI texture in this codebase so
    // colors don't drift.
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        texturePath.c_str(),
        0,                                     // maxsize: full source
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,
        DirectX::WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mTextureSRV.GetAddressOf());

    if (FAILED(hr))
    {
        // A missing PNG is non-fatal — the chevron simply does not
        // draw.  Asset pipeline can land later without a code change.
        LOG("[ScrollArrowRenderer] Failed to load texture: %ls", texturePath.c_str());
        return false;
    }

    // ------------------------------------------------------------
    // Auto-detect the actual texture dimensions from the underlying
    // ID3D11Texture2D resource.  This is the ONLY correct way to size
    // a sprite generically — hardcoding 64x64 (the previous version
    // of this file) silently cropped any non-64 PNG to the top-left
    // quadrant when an artist replaced the asset.
    //
    // Procedure:
    //   1. SRV -> ID3D11Resource (always succeeds for SRVs we create)
    //   2. Resource -> ID3D11Texture2D via QueryInterface (As<>)
    //   3. GetDesc() to read Width / Height
    //
    // The pivot is set to the texture center so the 180-degree rotation
    // used for the up chevron flips around the visual center, regardless
    // of texture size.
    // ------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11Resource>  resource;
    mTextureSRV->GetResource(resource.GetAddressOf());

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
    if (resource)
    {
        // ComPtr::As() is the COM-safe QueryInterface wrapper.
        // Returns S_OK and populates texture2D when the underlying
        // resource is a 2D texture.
        resource.As(&texture2D);
    }

    if (texture2D)
    {
        D3D11_TEXTURE2D_DESC desc{};
        texture2D->GetDesc(&desc);
        mWidth  = static_cast<int>(desc.Width);
        mHeight = static_cast<int>(desc.Height);
        LOG("[ScrollArrowRenderer] Loaded %ls (%dx%d).",
            texturePath.c_str(), mWidth, mHeight);
    }
    else
    {
        // Unreachable for normal 2D textures created via WIC, but the
        // guard prevents a crash if a 1D / 3D / cube texture sneaks in
        // through a future asset pipeline change.  64x64 is the safest
        // last-resort fallback because it matches the legacy convention
        // used by every other UI sprite in this codebase.
        LOG("[ScrollArrowRenderer] WARN: could not query texture descriptor for %ls; falling back to 64x64.",
            texturePath.c_str());
        mWidth  = 64;
        mHeight = 64;
    }

    mPivotX = mWidth  * 0.5f;
    mPivotY = mHeight * 0.5f;

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates      = std::make_unique<DirectX::CommonStates>(device);

    return true;
}

void ScrollArrowRenderer::Update(float dt)
{
    // Accumulate phase only — the actual offset is computed at Draw time
    // so two instances driven by the same elapsed value still produce
    // independent flip-aware motion.
    mElapsedTime += dt;
}

void ScrollArrowRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
}

void ScrollArrowRenderer::Draw(ID3D11DeviceContext* context,
                                 float x, float y,
                                 bool flipVertical,
                                 float scale,
                                 DirectX::CXMMATRIX transform,
                                 DirectX::XMVECTOR color)
{
    if (!mSpriteBatch || !mTextureSRV) return;

    // Bob direction is tied to flipVertical so the chevron always leans
    // away from the menu and toward the off-screen items it represents:
    //   - down chevron (flipVertical=false): bob downward (positive Y)
    //   - up   chevron (flipVertical=true) : bob upward   (negative Y)
    // sin oscillates symmetrically; the sign flip on flipVertical does
    // the directional inversion.
    const float bob = std::sin(mElapsedTime * mBobSpeed) * mBobAmplitude;
    const float yOffset = flipVertical ? -bob : bob;

    BindViewport(context);

    // Same blend / sampler / depth state as PointerRenderer so the two
    // chevrons composite correctly with the rest of the battle UI.
    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->AlphaBlend(),
        mStates->PointClamp(),
        mStates->DepthNone(),
        nullptr, nullptr,
        transform);

    RECT srcRect = { 0, 0, mWidth, mHeight };
    DirectX::XMFLOAT2 origin(mPivotX, mPivotY);
    DirectX::XMFLOAT2 pos(x, y + yOffset);

    // SpriteBatch::Draw with rotation = PI rotates 180 degrees around
    // the pivot.  This is cleaner than SpriteEffects_FlipVertically
    // because the rotation also flips the bob's perceived "tip
    // direction" — what looks like the arrow's nose stays the nose
    // even when the user is reading top-to-bottom.
    const float rotation = flipVertical ? DirectX::XM_PI : 0.0f;

    mSpriteBatch->Draw(
        mTextureSRV.Get(),
        pos,
        &srcRect,
        color,
        rotation,
        origin,
        scale);

    mSpriteBatch->End();
}

void ScrollArrowRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mTextureSRV.Reset();
}
