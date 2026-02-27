// ============================================================
// test_tk.cpp - Ví dụ DirectX 11 + DirectXTK
//
// DirectXTK (DirectX Tool Kit) là bộ thư viện tiện ích của Microsoft
// giúp đơn giản hóa các tác vụ phổ biến trong DirectX 11:
//   - SpriteBatch: vẽ hình 2D / sprite nhanh chóng
//   - SpriteFont : vẽ chữ lên màn hình
//   - SimpleMath : Vector2, Vector3, Matrix, Color... dễ dùng hơn DirectXMath thuần
//   - GeometricPrimitive: tạo hình khối 3D cơ bản (cube, sphere, teapot...)
//   - CommonStates: các trạng thái render thông dụng (blend, depth, rasterizer...)
//
// Ví dụ này sẽ:
//   1. Tạo cửa sổ Windows
//   2. Khởi tạo DirectX 11
//   3. Dùng SpriteBatch + SpriteFont để vẽ chữ "Hello, DirectXTK!" lên màn hình
//   4. Dùng GeometricPrimitive để vẽ một hình lập phương 3D xoay
// ============================================================

#define UNICODE
#define _UNICODE

#include <windows.h>        // API Windows cơ bản
#include <d3d11.h>          // DirectX 11 core
#include <directxmath.h>    // DirectXMath: XMMATRIX, XMMatrixPerspectiveFovLH...

// --- Headers của DirectXTK ---
#include <directxtk/SpriteBatch.h>         // Vẽ 2D sprite / hình ảnh
#include <directxtk/SpriteFont.h>          // Vẽ chữ lên màn hình
#include <directxtk/GeometricPrimitive.h>  // Hình khối 3D cơ bản
#include <directxtk/CommonStates.h>        // Blend state, depth state...
#include <directxtk/SimpleMath.h>          // Vector2, Vector3, Matrix, Color...

#include <memory>   // std::unique_ptr (quản lý bộ nhớ tự động)
#include <stdexcept>

// Tự động liên kết thư viện khi biên dịch bằng MSVC
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d3d11.lib")
// DirectXTK.lib nằm trong vcpkg, cần được truyền vào lệnh biên dịch bằng /LIBPATH

// Dùng namespace để viết ngắn hơn:
// DirectX::SimpleMath::Vector3 → chỉ cần viết Vector3
using namespace DirectX;
using namespace DirectX::SimpleMath;

// ============================================================
// CÁC BIẾN TOÀN CỤC - DirectX 11 core objects
// ============================================================
IDXGISwapChain*         g_swapChain  = nullptr; // Quản lý double buffer (front/back)
ID3D11Device*           g_device     = nullptr; // Đại diện GPU, tạo tài nguyên
ID3D11DeviceContext*    g_context    = nullptr; // Gửi lệnh vẽ đến GPU
ID3D11RenderTargetView* g_renderTarget = nullptr; // Vùng màu (color buffer) để vẽ lên
ID3D11DepthStencilView* g_depthStencilView = nullptr; // Depth buffer: xác định vật nào ở trước/sau

// ============================================================
// CÁC BIẾN TOÀN CỤC - DirectXTK objects (dùng unique_ptr để tự động giải phóng)
// ============================================================
std::unique_ptr<SpriteBatch>        g_spriteBatch;  // Vẽ 2D sprite/text
std::unique_ptr<SpriteFont>         g_font;         // Font chữ để vẽ text
std::unique_ptr<GeometricPrimitive> g_cube;         // Hình lập phương 3D
std::unique_ptr<CommonStates>       g_states;       // Các trạng thái render thông dụng

// Góc xoay của hình lập phương (tăng dần theo thời gian → tạo hiệu ứng xoay)
float g_rotationAngle = 0.0f;

// Kích thước cửa sổ
const int WINDOW_WIDTH  = 800;
const int WINDOW_HEIGHT = 600;

// ============================================================
// HÀM KHỞI TẠO DIRECTX 11
// ============================================================
void InitD3D(HWND hWnd) {
    // --- Tạo SwapChain + Device + DeviceContext ---
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount       = 1;
    scd.BufferDesc.Width  = WINDOW_WIDTH;
    scd.BufferDesc.Height = WINDOW_HEIGHT;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Định dạng màu 32-bit RGBA
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = hWnd;
    scd.SampleDesc.Count  = 1; // Không dùng MSAA
    scd.Windowed          = TRUE;

    D3D11CreateDeviceAndSwapChain(
        nullptr,                  // GPU mặc định
        D3D_DRIVER_TYPE_HARDWARE, // Dùng phần cứng thật (GPU)
        nullptr, 0,               // Không có software renderer
        nullptr, 0,               // Tự chọn feature level cao nhất
        D3D11_SDK_VERSION,
        &scd,
        &g_swapChain,
        &g_device,
        nullptr,
        &g_context
    );

    // --- Tạo Render Target View từ back buffer ---
    // Back buffer là texture 2D mà GPU sẽ vẽ hình lên
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    g_device->CreateRenderTargetView(pBackBuffer, nullptr, &g_renderTarget);
    pBackBuffer->Release(); // Không cần giữ con trỏ này nữa

    // --- Tạo Depth Buffer (depth stencil) ---
    // Depth buffer lưu độ sâu (Z) của mỗi pixel để xác định vật nào ở phía trước
    // Không có depth buffer → các mặt của hình 3D sẽ chồng chéo sai
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width     = WINDOW_WIDTH;
    depthDesc.Height    = WINDOW_HEIGHT;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT; // 24-bit depth + 8-bit stencil
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage     = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* pDepthStencil = nullptr;
    g_device->CreateTexture2D(&depthDesc, nullptr, &pDepthStencil);
    g_device->CreateDepthStencilView(pDepthStencil, nullptr, &g_depthStencilView);
    pDepthStencil->Release();

    // --- Gắn Render Target + Depth Buffer vào Output Merger stage ---
    // Output Merger là giai đoạn cuối của pipeline: quyết định màu cuối cùng của pixel
    g_context->OMSetRenderTargets(1, &g_renderTarget, g_depthStencilView);

    // --- Cấu hình Viewport ---
    // Viewport định nghĩa vùng màn hình mà DirectX sẽ vẽ lên
    // (có thể dùng để chia màn hình thành nhiều vùng khác nhau)
    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)WINDOW_WIDTH;
    vp.Height   = (float)WINDOW_HEIGHT;
    vp.MinDepth = 0.0f; // Giá trị depth nhỏ nhất (gần nhất)
    vp.MaxDepth = 1.0f; // Giá trị depth lớn nhất (xa nhất)
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    g_context->RSSetViewports(1, &vp); // RS = Rasterizer Stage

    // ============================================================
    // KHỞI TẠO CÁC ĐỐI TƯỢNG DIRECTXTK
    // ============================================================

    // CommonStates: tạo sẵn các blend/depth/rasterizer state thông dụng
    // Thay vì phải tự tạo từng state, chỉ cần gọi g_states->AlphaBlend(),...
    g_states = std::make_unique<CommonStates>(g_device);

    // SpriteBatch: tối ưu hóa việc vẽ nhiều 2D sprite cùng lúc
    // Gom chúng thành 1 draw call thay vì nhiều call riêng lẻ → nhanh hơn
    g_spriteBatch = std::make_unique<SpriteBatch>(g_context);

    // SpriteFont: load file .spritefont (định dạng font của DirectXTK)
    // File này được tạo từ MakeSpriteFont.exe hoặc tải từ DirectXTK samples
    // !!! Nếu không có file font, chương trình sẽ throw exception !!!
    // Tải font mẫu từ: https://github.com/microsoft/DirectXTK/tree/main/MakeSpriteFont
    try {
        g_font = std::make_unique<SpriteFont>(g_device, L"assets/fonts/Arial.spritefont");
    } catch (...) {
        // Nếu không tìm thấy file font, g_font = nullptr → sẽ bỏ qua phần vẽ chữ
        g_font = nullptr;
    }

    // GeometricPrimitive: tạo hình lập phương với kích thước cạnh = 1.0
    // DirectXTK tự động tạo vertex buffer + index buffer cho ta
    g_cube = GeometricPrimitive::CreateCube(g_context, 1.0f);
}

// ============================================================
// HÀM VẼ MỖI FRAME
// ============================================================
void RenderFrame() {
    // --- Xóa màn hình ---
    // Màu nền: xanh đậm
    float bgColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    g_context->ClearRenderTargetView(g_renderTarget, bgColor);

    // Xóa depth buffer về giá trị 1.0 (xa nhất) trước mỗi frame
    // Nếu không xóa, kết quả depth test của frame trước sẽ ảnh hưởng frame này
    g_context->ClearDepthStencilView(g_depthStencilView,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, // Xóa cả depth và stencil
        1.0f,  // Giá trị depth ban đầu = 1.0 (xa nhất)
        0);    // Giá trị stencil ban đầu = 0

    // -------------------------------------------------------
    // VẼ HÌNH LẬP PHƯƠNG 3D (dùng GeometricPrimitive của DirectXTK)
    // -------------------------------------------------------

    // Tăng góc xoay theo thời gian (0.01 radian/frame ≈ 0.57°/frame)
    g_rotationAngle += 0.01f;

    // --- Tạo ma trận World (biến đổi từ tọa độ vật thể → tọa độ thế giới) ---
    // Ma trận World định nghĩa: vị trí, xoay, và tỉ lệ của vật thể trong không gian 3D
    Matrix world = Matrix::CreateRotationY(g_rotationAngle)  // Xoay quanh trục Y
                 * Matrix::CreateRotationX(g_rotationAngle * 0.5f); // Xoay nhẹ quanh trục X

    // --- Tạo ma trận View (camera) ---
    // View matrix biến đổi từ tọa độ thế giới → tọa độ camera
    // Giống như đặt máy quay vào không gian
    Matrix view = Matrix::CreateLookAt(
        Vector3(0.0f, 1.5f, 4.0f),   // Vị trí camera: ở trên và phía sau
        Vector3(0.0f, 0.0f, 0.0f),   // Camera nhìn về điểm gốc (0,0,0)
        Vector3(0.0f, 1.0f, 0.0f)    // Vector "lên" của camera = trục Y dương (0,1,0)
    );

    // --- Tạo ma trận Projection (phối cảnh) ---
    // Projection matrix biến đổi từ tọa độ camera → tọa độ clip (2D màn hình)
    // Tạo hiệu ứng phối cảnh: vật ở xa trông nhỏ hơn vật ở gần
    Matrix proj = Matrix::CreatePerspectiveFieldOfView(
        XMConvertToRadians(60.0f), // FOV (Field of View): góc nhìn 60 độ
        (float)WINDOW_WIDTH / WINDOW_HEIGHT, // Tỷ lệ khung hình (aspect ratio)
        0.1f,   // Near plane: vật gần hơn 0.1 đơn vị sẽ bị cắt bỏ
        100.0f  // Far plane: vật xa hơn 100 đơn vị sẽ bị cắt bỏ
    );

    // Vẽ hình lập phương với:
    // - Ma trận World/View/Proj đã tạo ở trên
    // - wireframe = false → vẽ đặc (solid), không phải khung lưới
    // - CommonStates::CullCounterClockwise: ẩn mặt sau (backface culling) theo chiều ngược kim đồng hồ
    g_cube->Draw(world, view, proj,
        Colors::CornflowerBlue,         // Màu hình lập phương: xanh cornflower
        nullptr,                        // Không dùng texture
        false,                          // Không vẽ wireframe
        [&]() {                         // Lambda để thiết lập state bổ sung
            // Bật depth test để các mặt được sắp xếp đúng (mặt trước che mặt sau)
            g_context->OMSetDepthStencilState(g_states->DepthDefault(), 0);
        }
    );

    // -------------------------------------------------------
    // VẼ CHỮ 2D (dùng SpriteBatch + SpriteFont của DirectXTK)
    // -------------------------------------------------------
    if (g_font) { // Chỉ vẽ nếu đã load font thành công
    // Begin() bắt đầu batch vẽ sprite/text
    // SpriteSortMode_Deferred: gom tất cả lệnh vẽ lại, đẩy GPU 1 lần khi End()
    // Truyền rõ ràng ma trận transform (XMMatrixIdentity) để tránh tham chiếu đến
    // biến static bên trong DirectXTK (MatrixIdentity) gây lỗi linker khi dùng dllimport
    // Truyền đầy đủ tham số placeholder để đặt transform ở tham số cuối
    g_spriteBatch->Begin(SpriteSortMode_Deferred,
                 g_states->AlphaBlend(),
                 nullptr,   // sampler state (mặc định)
                 nullptr,   // depth stencil state (mặc định)
                 nullptr,   // rasterizer state (mặc định)
                 []() {},   // custom state setup lambda (rỗng)
                 XMMatrixIdentity());

        // Vẽ chữ tại vị trí (10, 10) tính từ góc trên-trái màn hình
        // Gọi DrawString với origin rõ ràng (0,0) và rotation=0 để tránh tham chiếu
        // tới Float2Zero (biến static nội bộ của SpriteFont)
        g_font->DrawString(
            g_spriteBatch.get(),         // SpriteBatch để vẽ
            L"Hello, DirectXTK!",       // Chuỗi cần vẽ (Unicode)
            XMFLOAT2(10.0f, 10.0f),       // Vị trí (X, Y) trên màn hình
            Colors::White,                // Màu chữ: trắng
            0.0f,                         // rotation
            XMFLOAT2(0.0f, 0.0f)          // origin (không dịch gốc)
        );

        g_font->DrawString(
            g_spriteBatch.get(),
            L"Cube dang xoay!",
            XMFLOAT2(10.0f, 50.0f),
            Colors::Yellow,
            0.0f,
            XMFLOAT2(0.0f, 0.0f)
        );

        // End() đẩy tất cả sprite đã gom vào GPU để vẽ
        g_spriteBatch->End();

        // Sau khi SpriteBatch->End(), nó thay đổi một số state của GPU
        // Cần reset lại Render Target để đảm bảo depth test hoạt động đúng ở frame sau
        g_context->OMSetRenderTargets(1, &g_renderTarget, g_depthStencilView);
    }

    // Hoán đổi front/back buffer để hiển thị frame vừa vẽ lên màn hình
    // 0 = không giới hạn FPS (không đồng bộ VSync)
    g_swapChain->Present(0, 0);
}

// ============================================================
// HÀM DỌN DẸP BỘ NHỚ
// unique_ptr sẽ tự động gọi destructor → không cần Release() thủ công cho DirectXTK objects
// Chỉ cần Release() các COM objects DirectX thuần
// ============================================================
void CleanD3D() {
    // unique_ptr tự giải phóng: g_cube, g_font, g_spriteBatch, g_states
    g_cube.reset();
    g_font.reset();
    g_spriteBatch.reset();
    g_states.reset();

    // Tắt fullscreen trước khi release swap chain (tránh crash)
    if (g_swapChain)      g_swapChain->SetFullscreenState(FALSE, nullptr);
    if (g_depthStencilView) g_depthStencilView->Release();
    if (g_renderTarget)   g_renderTarget->Release();
    if (g_swapChain)      g_swapChain->Release();
    if (g_device)         g_device->Release();
    if (g_context)        g_context->Release();
}

// ============================================================
// HÀM XỬ LÝ SỰ KIỆN CỬA SỔ
// ============================================================
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            // Người dùng đóng cửa sổ → gửi WM_QUIT để thoát vòng lặp chính
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            // Nhấn ESC → thoát
            if (wParam == VK_ESCAPE) PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// ============================================================
// WINMAIN - Điểm khởi đầu của ứng dụng Windows
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // --- Đăng ký class cửa sổ ---
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // Nền đen
    wc.lpszClassName = L"DXTKWindowClass";
    RegisterClassEx(&wc);

    // --- Tính toán kích thước cửa sổ sao cho vùng client đúng 800x600 ---
    // AdjustWindowRect tính toán kích thước cửa sổ bao gồm cả thanh tiêu đề và viền
    RECT wr = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    // --- Tạo cửa sổ ---
    HWND hWnd = CreateWindowEx(
        0, L"DXTKWindowClass",
        L"DirectX 11 + DirectXTK - SpriteBatch & GeometricPrimitive",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,       // Vị trí: để Windows tự chọn
        wr.right - wr.left,                 // Chiều rộng đã điều chỉnh
        wr.bottom - wr.top,                 // Chiều cao đã điều chỉnh
        nullptr, nullptr, hInstance, nullptr
    );
    ShowWindow(hWnd, nCmdShow);

    // --- Khởi tạo DirectX + DirectXTK ---
    InitD3D(hWnd);

    // --- Vòng lặp chính ---
    MSG msg = {};
    while (true) {
        // Xử lý hết tất cả sự kiện đang chờ trước khi vẽ
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (msg.message == WM_QUIT) break;

        RenderFrame(); // Vẽ frame tiếp theo
    }

    CleanD3D();
    return (int)msg.wParam;
}
