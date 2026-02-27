#include "D3DContext.h"
#include <cassert>

D3DContext& D3DContext::Get() {
    static D3DContext instance;
    return instance;
}

bool D3DContext::Initialize(HWND hWnd, int width, int height) {
    mWidth  = width;
    mHeight = height;

    // --- Mô tả Swap Chain ---
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount       = 1;
    scd.BufferDesc.Width  = (UINT)width;
    scd.BufferDesc.Height = (UINT)height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = hWnd;
    scd.SampleDesc.Count  = 1;  // Không MSAA (thêm sau nếu cần)
    scd.Windowed          = TRUE;
    scd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    // Bật Debug Layer khi build Debug để bắt lỗi DirectX
    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // GPU mặc định
        D3D_DRIVER_TYPE_HARDWARE,   // Dùng phần cứng thật
        nullptr,
        createFlags,
        nullptr, 0,                 // Tự chọn feature level cao nhất
        D3D11_SDK_VERSION,
        &scd,
        mSwapChain.GetAddressOf(),  // ComPtr: dùng GetAddressOf() thay vì &
        mDevice.GetAddressOf(),
        nullptr,
        mContext.GetAddressOf()
    );

    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"D3D11CreateDeviceAndSwapChain thất bại!", L"Lỗi DirectX", MB_OK);
        return false;
    }

    return CreateRenderTargetAndDepth();
}

bool D3DContext::CreateRenderTargetAndDepth() {
    // --- Render Target View từ back buffer ---
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                       reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = mDevice->CreateRenderTargetView(backBuffer.Get(), nullptr,
                                         mRenderTarget.GetAddressOf());
    if (FAILED(hr)) return false;
    // backBuffer ComPtr tự Release() ở đây

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

    // --- Gắn Render Target + Depth vào pipeline ---
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
    // Phải detach khỏi pipeline trước khi Release để tránh crash
    mContext->OMSetRenderTargets(0, nullptr, nullptr);
    mRenderTarget.Reset();
    mDepthStencilView.Reset();
}

void D3DContext::OnResize(int newWidth, int newHeight) {
    if (newWidth == 0 || newHeight == 0) return;
    mWidth  = newWidth;
    mHeight = newHeight;

    ReleaseRenderTargetAndDepth();

    // Thay đổi kích thước buffer của swap chain
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
    // Present: 0 = không VSync, 1 = VSync (giới hạn 60fps)
    mSwapChain->Present(0, 0);
}

void D3DContext::Shutdown() {
    ReleaseRenderTargetAndDepth();
    mContext.Reset();
    mSwapChain.Reset();
    mDevice.Reset();
}
