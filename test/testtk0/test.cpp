#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>

// Include thư viện DirectXTK
#include "SpriteBatch.h"
#include "SimpleMath.h"

// Tự động link các thư viện chuẩn (Không cần gõ tay d3d11.lib nữa)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d11.lib")
// DirectXTK.lib vẫn phải link bằng dòng lệnh cl.exe vì nó là thư viện ngoài

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// --- CÁC BIẾN TOÀN CỤC ---
HWND                                    g_hWnd = nullptr;
ComPtr<ID3D11Device>                    g_pd3dDevice;
ComPtr<ID3D11DeviceContext>             g_pImmediateContext;
ComPtr<IDXGISwapChain>                  g_pSwapChain;
ComPtr<ID3D11RenderTargetView>          g_pRenderTargetView;

// Các biến của DirectXTK
std::unique_ptr<SpriteBatch>            g_SpriteBatch;
ComPtr<ID3D11ShaderResourceView>        g_pBlankTexture; // Dùng để chứa 1 pixel trắng

// --- HÀM TẠO 1 PIXEL TRẮNG ĐỂ VẼ HÌNH KHỐI CƠ BẢN ---
void CreateBlankTexture() {
    // Tạo 1 pixel màu trắng (Mã RGBA: FFFFFFFF)
    static const uint32_t s_pixel = 0xffffffff;

    D3D11_SUBRESOURCE_DATA initData = { &s_pixel, sizeof(uint32_t), 0 };

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> tex;
    g_pd3dDevice->CreateTexture2D(&desc, &initData, tex.GetAddressOf());
    g_pd3dDevice->CreateShaderResourceView(tex.Get(), nullptr, g_pBlankTexture.GetAddressOf());
}

// --- HÀM KHỞI TẠO DIRECTX VÀ DIRECTXTK ---
bool InitDirectX(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    // 1. Tạo Device và SwapChain (Lõi của DX11)
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, g_pSwapChain.GetAddressOf(), g_pd3dDevice.GetAddressOf(), nullptr, g_pImmediateContext.GetAddressOf())))
        return false;

    // 2. Tạo Render Target (Bảng vẽ)
    ComPtr<ID3D11Texture2D> pBackBuffer;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)pBackBuffer.GetAddressOf());
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, g_pRenderTargetView.GetAddressOf());
    g_pImmediateContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), nullptr);

    // 3. KHỞI TẠO DIRECTXTK NGAY TẠI ĐÂY
    g_SpriteBatch = std::make_unique<SpriteBatch>(g_pImmediateContext.Get());
    CreateBlankTexture();

    return true;
}

// --- HÀM VẼ (CHẠY 60 LẦN/GIÂY) ---
void Render() {
    // Xóa màn hình với màu xanh đen
    float clearColor[4] = { 0.05f, 0.05f, 0.1f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView.Get(), clearColor);

    // ----- BẮT ĐẦU DÙNG DIRECTXTK VẼ 2D -----
    g_SpriteBatch->Begin();

    // Xác định khung chữ nhật cần vẽ (x, y, chiều rộng, chiều cao)
    RECT destRect = { 300, 200, 500, 400 }; 

    // Vẽ pixel trắng vào khung chữ nhật đó, và phủ màu Cam (Orange) lên
    g_SpriteBatch->Draw(g_pBlankTexture.Get(), destRect, Colors::Orange);

    g_SpriteBatch->End();
    // ----------------------------------------

    // Đẩy khung hình ra màn hình máy tính
    g_pSwapChain->Present(1, 0);
}

// --- XỬ LÝ SỰ KIỆN CỬA SỔ ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// --- HÀM MAIN ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Đăng ký lớp cửa sổ
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"DX11Class", nullptr };
    RegisterClassEx(&wcex);

    // Tạo cửa sổ Windows
    g_hWnd = CreateWindow(L"DX11Class", L"DirectXTK - Hinh vuong mau cam", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
    ShowWindow(g_hWnd, nCmdShow);

    // Khởi tạo đồ họa
    if (!InitDirectX(g_hWnd)) return 0;

    // Vòng lặp Game (Game Loop)
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Render(); // Liên tục vẽ hình
        }
    }

    // ComPtr và std::unique_ptr sẽ tự động dọn dẹp bộ nhớ khi tắt app, không lo Memory Leak!
    return (int)msg.wParam;
}