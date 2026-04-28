#include "ExpBarRenderer.h"
#include "../Utils/Log.h"

using namespace DirectX;

void ExpBarRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
}

bool ExpBarRenderer::Initialize(ID3D11Device* device,
                                ID3D11DeviceContext* context,
                                int screenW, int screenH,
                                float renderX, float renderY)
{
    mScreenW = screenW;
    mScreenH = screenH;
    mRenderX = renderX;
    mRenderY = renderY;

    // Load mock python-generated assets explicitly!
    HRESULT hr = DirectX::CreateWICTextureFromFile(device, context, L"assets/UI/exp-bar-bg.png", nullptr, &mBgSRV);
    if (FAILED(hr)) {
        LOG("[ExpBarRenderer] Failed to load exp-bar-bg.png");
        return false;
    }

    hr = DirectX::CreateWICTextureFromFile(device, context, L"assets/UI/exp-bar-fill.png", nullptr, &mFillSRV);
    if (FAILED(hr)) {
        LOG("[ExpBarRenderer] Failed to load exp-bar-fill.png");
        return false;
    }

    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates      = std::make_unique<CommonStates>(device);

    return true;
}

void ExpBarRenderer::SetExp(int currentExp, int nextLevelExp)
{
    mCurrentExp = currentExp;
    mNextLevelExp = (nextLevelExp > 0) ? nextLevelExp : 1; // Prevent Div0
}

void ExpBarRenderer::SetScreenSize(int w, int h)
{
    mScreenW = w;
    mScreenH = h;
}

void ExpBarRenderer::Render(ID3D11DeviceContext* context)
{
    if (!IsInitialized()) return;

    BindViewport(context);

    mSpriteBatch->Begin(SpriteSortMode_Deferred, mStates->NonPremultiplied(), mStates->PointClamp());

    // 1. Draw Background full bounds
    mSpriteBatch->Draw(mBgSRV.Get(), XMFLOAT2(mRenderX, mRenderY));

    // 2. Draw Fill natively clipping via RECT bounds dynamically computed!
    float ratio = static_cast<float>(mCurrentExp) / static_cast<float>(mNextLevelExp);
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < 0.0f) ratio = 0.0f;

    long fillPixelWidth = static_cast<long>(kBarWidth * ratio);

    if (fillPixelWidth > 0)
    {
        RECT srcRect = { 0, 0, fillPixelWidth, static_cast<long>(kBarHeight) };
        mSpriteBatch->Draw(mFillSRV.Get(), XMFLOAT2(mRenderX, mRenderY), &srcRect);
    }

    mSpriteBatch->End();
}

void ExpBarRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mBgSRV.Reset();
    mFillSRV.Reset();
}
