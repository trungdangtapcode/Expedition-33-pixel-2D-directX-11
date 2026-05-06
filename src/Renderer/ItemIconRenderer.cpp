// ============================================================
// File: ItemIconRenderer.cpp
// ============================================================
#include "ItemIconRenderer.h"
#include "../Utils/Log.h"

bool ItemIconRenderer::Initialize(ID3D11Device* device,
                                  ID3D11DeviceContext* context,
                                  int screenW,
                                  int screenH)
{
    Shutdown();

    mScreenW = screenW;
    mScreenH = screenH;

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates      = std::make_unique<DirectX::CommonStates>(device);

    if (!mSpriteBatch || !mStates)
    {
        LOG("[ItemIconRenderer] Failed to construct SpriteBatch / CommonStates.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// Draw
//   Wraps the SpriteBatch begin/end pair around a single Draw call.
//   The destination rect is computed from caller-supplied pixel
//   coordinates; source rect is nullptr so SpriteBatch uses the
//   full texture.
//
//   We open a fresh Begin/End pair per icon rather than batching
//   externally because every icon may have a different transform
//   (e.g. world-space camera matrix vs. screen-space identity)
//   and SpriteBatch requires one transform per Begin().  The cost
//   is one state-set per icon -- negligible at our menu sizes
//   (typically <= 12 icons on screen at once).
// ------------------------------------------------------------
void ItemIconRenderer::Draw(ID3D11DeviceContext* context,
                            ID3D11ShaderResourceView* texSRV,
                            float destX, float destY, float destW, float destH,
                            DirectX::CXMMATRIX transform,
                            DirectX::XMVECTOR color)
{
    if (!mSpriteBatch || !mStates) return;
    if (!texSRV)                    return;
    if (destW <= 0.0f || destH <= 0.0f) return;

    BindViewport(context);

    // LinearClamp: smooth-stretch the icon.  Item art is generally
    // shaded (per idea/asset-todo.md), so bilinear filtering looks
    // correct even at non-integer scale ratios.  PointClamp would
    // produce stair-stepped artifacts on the typical 32->~60-100
    // pixel destination sizes used by the inventory grid + battle
    // menu.
    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->AlphaBlend(),
        mStates->LinearClamp(),
        mStates->DepthNone(),
        nullptr,
        nullptr,
        transform);

    RECT dst = {
        static_cast<LONG>(destX),
        static_cast<LONG>(destY),
        static_cast<LONG>(destX + destW),
        static_cast<LONG>(destY + destH)
    };

    // sourceRect = nullptr -> SpriteBatch samples the entire texture.
    // No per-frame source rect bookkeeping -- one PNG per item.
    mSpriteBatch->Draw(texSRV, dst, nullptr, color);

    mSpriteBatch->End();
}

void ItemIconRenderer::BindViewport(ID3D11DeviceContext* context)
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

void ItemIconRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
}
