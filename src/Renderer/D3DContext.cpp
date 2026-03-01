#include "D3DContext.h"
#include "../Utils/Log.h"
#include <cassert>
#include <wchar.h>   // swprintf_s

D3DContext& D3DContext::Get() {
    static D3DContext instance;
    return instance;
}

bool D3DContext::Initialize(HWND hWnd, int width, int height) {
    mWidth  = width;
    mHeight = height;

    // --- Describe Swap Chain ---
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount       = 1; // Buffercount is for flip model, we can set to 2 or more for better performance, but it requires more complex synchronization. For now, we use 1 for simplicity.
    scd.BufferDesc.Width  = (UINT)width;
    scd.BufferDesc.Height = (UINT)height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = hWnd;
    scd.SampleDesc.Count  = 1;  // No MSAA (can be added later if needed)
    scd.Windowed          = TRUE;
    scd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    // ------------------------------------------------------------
    // Request the debug layer only in _DEBUG builds.
    // Why: D3D11_CREATE_DEVICE_DEBUG requires "Graphics Tools" to be installed
    //      (Windows Settings → Optional Features). On machines without it,
    //      D3D11CreateDeviceAndSwapChain returns 0x887A002D
    //      (DXGI_ERROR_SDK_COMPONENT_MISSING) and the device fails to create.
    //
    // Strategy: try with debug layer first; if it fails with
    //      DXGI_ERROR_SDK_COMPONENT_MISSING, warn and retry without it.
    //      Any other failure is a real error that we surface immediately.
    // ------------------------------------------------------------
    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    auto TryCreate = [&](UINT flags) -> HRESULT {
        // Reset ComPtrs before retrying — GetAddressOf() requires them to be null.
        mSwapChain.Reset();
        mDevice.Reset();
        mContext.Reset();
        return D3D11CreateDeviceAndSwapChain(
            nullptr,                   // Use default GPU adapter
            D3D_DRIVER_TYPE_HARDWARE,  // Use real GPU hardware, not software rasterizer
            nullptr,
            flags,
            nullptr, 0,                // Let DirectX pick the highest supported feature level
            D3D11_SDK_VERSION,
            &scd,
            mSwapChain.GetAddressOf(),
            mDevice.GetAddressOf(),
            nullptr,
            mContext.GetAddressOf()
        );
    };

    HRESULT hr = TryCreate(createFlags);

#ifdef _DEBUG
    // 0x887A002D = DXGI_ERROR_SDK_COMPONENT_MISSING
    // Means "Graphics Tools" optional feature is not installed.
    // Fall back to a non-debug device so the game still runs.
    if (hr == static_cast<HRESULT>(0x887A002D)) {
        LOG("[D3DContext] WARNING: Debug layer unavailable (DXGI_ERROR_SDK_COMPONENT_MISSING).");
        LOG("[D3DContext] Retrying without D3D11_CREATE_DEVICE_DEBUG.");
        LOG("[D3DContext] To enable: Settings > Optional Features > Graphics Tools");
        hr = TryCreate(0);
    }
#endif

    if (FAILED(hr)) {
        // Format the HRESULT code as hex so the exact failure reason is visible.
        // Common codes:
        //   0x887A0004 (DXGI_ERROR_UNSUPPORTED)           — feature level not supported by GPU
        //   0x887A0022 (DXGI_ERROR_SDK_COMPONENT_MISSING) — debug layer not installed;
        //              fix: Windows Settings → Optional Features → "Graphics Tools"
        //   0x80004005 (E_FAIL)                           — generic failure (bad HWND / driver)
        //   0x80070057 (E_INVALIDARG)                     — bad field in DXGI_SWAP_CHAIN_DESC
        wchar_t msg[512];
        swprintf_s(msg, 512,
            L"D3D11CreateDeviceAndSwapChain failed!\n"
            L"HRESULT: 0x%08X\n\n"
            L"Common causes:\n"
            L"  0x887A0022 - Install Graphics Tools\n"
            L"               (Settings > Optional Features)\n"
            L"  0x887A0004 - GPU feature level unsupported\n"
            L"  0x80070057 - Invalid swap chain parameter",
            (unsigned)hr);
        MessageBoxW(nullptr, msg, L"DirectX Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return CreateRenderTargetAndDepth();
}

// ------------------------------------------------------------
// Function: CreateRenderTargetAndDepth
// Purpose:
//   Create the Render Target View (RTV) and Depth/Stencil View (DSV)
//   that the GPU draws into every frame, then bind them to the pipeline.
//
// Render Target View (RTV):
//   A "canvas" backed by the swap chain's back buffer.
//   Stores RGBA color per pixel. The GPU writes final pixel colors here.
//   On EndFrame() / Present(), this buffer is flipped to the screen.
//
// Depth/Stencil View (DSV):
//   Stores two values per pixel in DXGI_FORMAT_D24_UNORM_S8_UINT:
//     - 24-bit depth  : distance from camera to each pixel (Z value in [0,1]).
//                       The GPU uses this to discard pixels hidden behind
//                       closer geometry (depth test). Without it, draw order
//                       is arbitrary and objects clip through each other.
//     - 8-bit stencil : a per-pixel mask for advanced effects (outlines,
//                       shadows, portal rendering, mirror reflections).
//   Depth test logic per pixel:
//     new depth < stored depth → GPU writes the pixel and updates depth buffer.
//     new depth ≥ stored depth → GPU discards the pixel (it is occluded).
//
// Why separated into its own function:
//   OnResize() must release and recreate these views whenever the window
//   changes size. Isolating the logic avoids duplicating ~40 lines.
//
// Caveats:
//   - Must call ReleaseRenderTargetAndDepth() BEFORE ResizeBuffers().
//   - Forgetting OMSetRenderTargets() after recreation → nothing renders.
// ------------------------------------------------------------
bool D3DContext::CreateRenderTargetAndDepth() {
    // --- Render Target View from back buffer ---
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                       reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = mDevice->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                         mRenderTarget.GetAddressOf());
    if (FAILED(hr)) return false;
    // backBuffer ComPtr will automatically Release() here

    // --- Depth/Stencil Buffer ---
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width     = (UINT)mWidth;
    depthDesc.Height    = (UINT)mHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT; // 24-bit depth + 8-bit stencil
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage     = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTex;
    hr = mDevice->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = mDevice->CreateDepthStencilView(depthTex.Get(), nullptr,
                                          mDepthStencilView.GetAddressOf());
    if (FAILED(hr)) return false;

    // --- Bind Render Target + Depth to pipeline ---
    ID3D11RenderTargetView* rtv = mRenderTarget.Get();
    mContext->OMSetRenderTargets(1, &rtv, mDepthStencilView.Get());

    // --- Viewport ---
    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)mWidth;
    vp.Height   = (float)mHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    mContext->RSSetViewports(1, &vp);

    return true;
}

void D3DContext::ReleaseRenderTargetAndDepth() {
    // Must detach from pipeline before releasing to avoid crash
    mContext->OMSetRenderTargets(0, nullptr, nullptr);
    mRenderTarget.Reset();
    mDepthStencilView.Reset();
}

void D3DContext::OnResize(int newWidth, int newHeight) {
    if (newWidth == 0 || newHeight == 0) return;
    mWidth  = newWidth;
    mHeight = newHeight;

    ReleaseRenderTargetAndDepth();

    // Resize swap chain buffers
    mSwapChain->ResizeBuffers(0,
        (UINT)newWidth, (UINT)newHeight,
        DXGI_FORMAT_UNKNOWN, 0);

    CreateRenderTargetAndDepth();
}

void D3DContext::BeginFrame(float r, float g, float b) {
    float clearColor[4] = { r, g, b, 1.0f };
    mContext->ClearRenderTargetView(mRenderTarget.Get(), clearColor);
    mContext->ClearDepthStencilView(mDepthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void D3DContext::EndFrame() {
    // Present: 0 = no VSync, 1 = VSync (limit to 60fps)
    mSwapChain->Present(0, 0);
}

void D3DContext::Shutdown() {
    ReleaseRenderTargetAndDepth();
    mContext.Reset();
    mSwapChain.Reset();
    mDevice.Reset();
}
