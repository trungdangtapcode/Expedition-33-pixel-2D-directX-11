// ============================================================
// File: DebugTextureViewer.cpp
// Responsibility: Load + draw a raw texture centered on screen for debugging.
//
// All D3D11 state objects (blend, depth) are created ONCE in Load() and
// cached as members.  Draw() performs zero heap allocations per frame.
// ============================================================
#include "DebugTextureViewer.h"
#include "../Utils/Log.h"
#include "../Utils/HrCheck.h"
#include "../Renderer/D3DContext.h"
#include <d3d11.h>

bool DebugTextureViewer::Load(ID3D11Device* device,
                               ID3D11DeviceContext* context,
                               const wchar_t* path)
{
    // --- Load texture, also retrieve raw resource to query dimensions ---
    // WIC_LOADER_IGNORE_SRGB: prevent WIC from gamma-linearising colors.
    // Without this flag the sRGB ICC profile in PNGs causes colors to be
    // darkened when rendered to the UNORM backbuffer.
    Microsoft::WRL::ComPtr<ID3D11Resource> res;
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device, context, path,
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,
        DirectX::WIC_LOADER_IGNORE_SRGB,  // load raw pixel values, no gamma conversion
        res.GetAddressOf(),
        mSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        LOG("[DebugTextureViewer] Failed to load '%ls' (0x%08X)", path, hr);
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
    if (SUCCEEDED(res.As(&tex2d))) {
        D3D11_TEXTURE2D_DESC desc = {};
        tex2d->GetDesc(&desc);
        mTexW = desc.Width;
        mTexH = desc.Height;
    }
    LOG("[DebugTextureViewer] Loaded '%ls' — %ux%u", path, mTexW, mTexH);

    // --- Create SpriteBatch ---
    mBatch = std::make_unique<DirectX::SpriteBatch>(context);

    // --- Create DepthNone state ONCE ---
    // SpriteBatch::Begin() accepts a DepthStencilState pointer.  We pass this
    // every frame so depth testing is disabled while the sprite is drawn.
    // Creating it here (not per-frame) avoids a D3D11 object allocation every Draw().
    {
        D3D11_DEPTH_STENCIL_DESC ds = {};
        ds.DepthEnable    = FALSE;
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        ds.StencilEnable  = FALSE;
        hr = device->CreateDepthStencilState(&ds, mDepthNone.GetAddressOf());
        if (FAILED(hr)) {
            LOG("[DebugTextureViewer] CreateDepthStencilState failed (0x%08X)", hr);
            return false;
        }
    }

    // --- Create AlphaBlend state ONCE ---
    // Explicit SRC_ALPHA / INV_SRC_ALPHA blend.  Passing nullptr to
    // SpriteBatch::Begin() would use the "device default" blend, which is
    // whatever state is currently bound — unpredictable after other renderers.
    {
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable           = TRUE;
        bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device->CreateBlendState(&bd, mAlphaBlend.GetAddressOf());
        if (FAILED(hr)) {
            LOG("[DebugTextureViewer] CreateBlendState failed (0x%08X)", hr);
            return false;
        }
    }

    // --- Create OpaqueBlend state ONCE (diagnostic) ---
    // BlendEnable=FALSE: every pixel overwrites the back-buffer at full opacity,
    // ignoring the texture's alpha channel entirely.
    // Use DrawOptions::forceOpaque=true to test whether SpriteBatch IS drawing
    // but the PNG alpha=0 was making it invisible under normal blending.
    {
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable           = FALSE;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = device->CreateBlendState(&bd, mOpaqueBlend.GetAddressOf());
        if (FAILED(hr)) {
            LOG("[DebugTextureViewer] CreateBlendState (opaque) failed (0x%08X)", hr);
            return false;
        }
    }

    return true;
}

void DebugTextureViewer::Draw(ID3D11DeviceContext* context,
                               int screenW, int screenH,
                               DrawOptions opts)
{
    if (!mSRV || !mBatch) return;
	// log opts
	// LOG("[DebugTextureViewer] Draw called: SRV=%p batch=%p texW=%u texH=%u screen=%dx%d overrideDepth=%d overrideBlend=%d",
	//     mSRV.Get(), mBatch.get(), mTexW, mTexH, screenW, screenH, opts.overrideDepth, opts.overrideBlend);

    // Set viewport — SpriteBatch::SetViewport() caches it so GetViewportTransform()
    // never calls RSGetViewports().  If RSGetViewports returns count=0 (which
    // happens when RS state was partially reset by another renderer), SpriteBatch
    // throws std::runtime_error("No viewport is set") inside End(), leaving
    // mInBeginEndPair=true and permanently breaking all subsequent Begin() calls.
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(screenW);
    vp.Height   = static_cast<float>(screenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
    mBatch->SetViewport(vp);

    // Resolve which cached state pointers to forward to SpriteBatch::Begin().
    ID3D11DepthStencilState* depthState = opts.overrideDepth ? mDepthNone.Get() : nullptr;
    ID3D11BlendState*        blendState = nullptr;
    if (opts.overrideBlend) {
        blendState = opts.forceOpaque ? mOpaqueBlend.Get() : mAlphaBlend.Get();
    }

    // ---------------------------------------------------------------
    // EXPLICIT depth-state bind before Begin().
    //
    // Why this matters even though we pass depthState to Begin():
    //   SpriteBatch::PrepareForRendering() calls OMSetDepthStencilState()
    //   INSIDE End(), not Begin().  Between Begin() and End() the OM stage
    //   still has whatever depth state was left by the previous renderer.
    //   CircleRenderer::Draw() resets to OMSetDepthStencilState(nullptr,0)
    //   which is D3D11 default = DepthEnable=TRUE, func=LESS.
    //   CircleRenderer VS outputs z=0.0 for all pixels, so the depth buffer
    //   is filled with 0.0 after its draw call.
    //   SpriteBatch also emits z=0.0 → depth test: 0.0 < 0.0 = FALSE
    //   → every sprite pixel discarded before PrepareForRendering() even runs.
    //
    // Calling OMSetDepthStencilState(mDepthNone, 0) here disables depth
    // testing on the OM stage immediately, so SpriteBatch's deferred
    // draw call inside End() is not blocked by the stale depth buffer.
    // ---------------------------------------------------------------
    if (opts.overrideDepth)
        context->OMSetDepthStencilState(mDepthNone.Get(), 0);

    // ---------------------------------------------------------------
    // Save the currently-bound RTV and DSV so we can restore them after draw.
    // This is critical when Draw() is called inside a BeginCapture/EndCapture
    // region (e.g., PincushionDistortionFilter) — the current RTV may be an
    // offscreen texture, NOT the back buffer.  Hardcoding D3DContext::GetRTV()
    // here would redirect subsequent draws (Circle, SceneGraph) to the back
    // buffer, making captured characters invisible in the offscreen texture.
    // ---------------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> savedRTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> savedDSV;
    context->OMGetRenderTargets(1, savedRTV.GetAddressOf(), savedDSV.GetAddressOf());

    // Re-bind RTV without DSV — removes the depth buffer from the pipeline.
    //
    // Belt-and-suspenders: even with DepthEnable=FALSE, some driver
    // implementations honour depth writes when the DSV is bound.  Passing
    // nullptr guarantees the depth buffer is disconnected from the OM stage.
    {
        ID3D11RenderTargetView* rtv = savedRTV.Get();
        context->OMSetRenderTargets(1, &rtv, nullptr);   // DSV = nullptr → depth buffer detached
    }

    // Full-screen rect — eliminates scale/position math as a variable.
    RECT dest = { 0, 0, (LONG)screenW, (LONG)screenH };

    mBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        blendState,
        nullptr,      // sampler: LinearClamp default
        depthState
    );
    mBatch->Draw(mSRV.Get(), dest);
    mBatch->End();

    // Restore whichever RTV+DSV was bound when Draw() was called.
    // Using the saved values (not D3DContext::GetRTV) ensures correctness
    // whether this renderer is inside a pincushion capture pass or not.
    {
        ID3D11RenderTargetView* rtv = savedRTV.Get();
        ID3D11DepthStencilView* dsv = savedDSV.Get();
        context->OMSetRenderTargets(1, &rtv, dsv);
    }
}

void DebugTextureViewer::Shutdown()
{
    mOpaqueBlend.Reset();
    mAlphaBlend.Reset();
    mDepthNone.Reset();
    mBatch.reset();
    mSRV.Reset();
    mTexW = 0;
    mTexH = 0;
}
