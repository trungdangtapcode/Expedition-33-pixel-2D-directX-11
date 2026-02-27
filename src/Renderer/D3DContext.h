#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr - quản lý COM object tự động

// ============================================================
// D3DContext - Bọc (Wrapper) toàn bộ DirectX 11 initialization
//
// PATTERN: Facade Pattern
// Ẩn toàn bộ sự phức tạp của DirectX 11 setup (SwapChain,
// Device, RenderTarget, DepthBuffer, Viewport...) sau 1 interface đơn giản.
//
// Các hệ thống khác (State, SpriteBatch...) CHỈ cần gọi:
//   D3DContext::Get().GetDevice()
//   D3DContext::Get().GetContext()
//
// CÁCH DÙNG:
//   D3DContext::Get().Initialize(hWnd, width, height);
//   // --- mỗi frame ---
//   D3DContext::Get().BeginFrame();
//   // ... vẽ ...
//   D3DContext::Get().EndFrame();    // Present
//   // --- khi thoát ---
//   D3DContext::Get().Shutdown();
//
// Microsoft::WRL::ComPtr là "smart pointer" cho COM object:
//   - Tự gọi Release() khi ra khỏi scope → không bao giờ leak
//   - Dùng .Get() để lấy raw pointer khi cần truyền vào DirectX API
// ============================================================
class D3DContext {
public:
    static D3DContext& Get();

    D3DContext(const D3DContext&) = delete;
    D3DContext& operator=(const D3DContext&) = delete;

    // --- Khởi tạo và giải phóng ---
    bool Initialize(HWND hWnd, int width, int height);
    void Shutdown();
    void OnResize(int newWidth, int newHeight); // Gọi khi cửa sổ thay đổi kích thước

    // --- Mỗi frame ---
    void BeginFrame(float r = 0.1f, float g = 0.1f, float b = 0.2f); // Xóa màn hình
    void EndFrame();   // Present swap chain

    // --- Getters (trả về raw pointer để truyền vào DirectX API) ---
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
