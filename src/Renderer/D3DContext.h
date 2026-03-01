#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr - quản lý COM object tự động

// ============================================================
// D3DContext - (Wrapper) whole DirectX 11 initialization
//
// PATTERN: Facade Pattern
// Hides the complexity of DirectX 11 setup (SwapChain,
// Device, RenderTarget, DepthBuffer, Viewport...) behind a simple interface.
//
// Other systems (State, SpriteBatch...) only need to call:
//   D3DContext::Get().GetDevice()
//   D3DContext::Get().GetContext()
//
// USAGE:
//   D3DContext::Get().Initialize(hWnd, width, height);
//   // --- for frame ---
//   D3DContext::Get().BeginFrame();
//   // ... draw ...
//   D3DContext::Get().EndFrame();    // Present
//   // --- on exit ---
//   D3DContext::Get().Shutdown();
//
// Microsoft::WRL::ComPtr is a "smart pointer" for COM objects:
//   - Automatically calls Release() when going out of scope → never leak
//   - Use .Get() to get the raw pointer when passing to DirectX API
// ============================================================
class D3DContext {
public:
    static D3DContext& Get();

    D3DContext(const D3DContext&) = delete;
    D3DContext& operator=(const D3DContext&) = delete;

    // --- Initialize and shutdown ---
    bool Initialize(HWND hWnd, int width, int height);
    void Shutdown();
    void OnResize(int newWidth, int newHeight); // Call when window size changes

    // --- Per frame ---
    void BeginFrame(float r = 0.1f, float g = 0.1f, float b = 0.2f); // Clear screen
    void EndFrame();   // Present swap chain

    // --- Getters (return raw pointer to pass into DirectX API) ---
    ID3D11Device*        GetDevice()  const { return mDevice.Get(); }
    ID3D11DeviceContext* GetContext() const { return mContext.Get(); }
    IDXGISwapChain*      GetSwapChain() const { return mSwapChain.Get(); }

    int GetWidth()  const { return mWidth; }
    int GetHeight() const { return mHeight; }

private:
    D3DContext() = default;

    bool CreateRenderTargetAndDepth();  // Tách ra để dùng lại khi resize
    void ReleaseRenderTargetAndDepth(); // Dọn dẹp trước khi resize

    // ComPtr tự động gọi Release() khi D3DContext bị destroy
    Microsoft::WRL::ComPtr<IDXGISwapChain>          mSwapChain;
    Microsoft::WRL::ComPtr<ID3D11Device>             mDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>      mContext;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   mRenderTarget;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   mDepthStencilView;

    int mWidth  = 0;
    int mHeight = 0;
};
