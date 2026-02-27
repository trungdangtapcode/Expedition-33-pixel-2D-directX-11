#include "GameApp.h"
#include "../Renderer/D3DContext.h"
#include "../States/StateManager.h"
#include "../States/MenuState.h"
#include <sstream>

// Static instance pointer - dùng trong WindowProc
GameApp* GameApp::sInstance = nullptr;

// Win32 Window Procedure (global function, forward vào GameApp)
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (GameApp::GetInstance())
        return GameApp::GetInstance()->HandleMessage(hWnd, msg, wParam, lParam);
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================

GameApp::GameApp() {
    sInstance = this;
}

GameApp::~GameApp() {
    sInstance = nullptr;
}

bool GameApp::Initialize(HINSTANCE hInstance, const std::wstring& title,
                         int width, int height)
{
    mHInstance = hInstance;
    mTitle     = title;
    mWidth     = width;
    mHeight    = height;

    if (!InitWindow(hInstance)) return false;

    if (!D3DContext::Get().Initialize(mHwnd, mWidth, mHeight)) return false;

    // Đẩy State ban đầu vào stack (MenuState)
    StateManager::Get().PushState(std::make_unique<MenuState>());

    return true;
}

bool GameApp::InitWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc   = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"GameAppWindowClass";
    RegisterClassEx(&wc);

    // Tính kích thước cửa sổ sao cho vùng CLIENT đúng mWidth x mHeight
    RECT wr = { 0, 0, mWidth, mHeight };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    mHwnd = CreateWindowEx(
        0,
        L"GameAppWindowClass",
        mTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!mHwnd) return false;
    ShowWindow(mHwnd, SW_SHOW);
    UpdateWindow(mHwnd);
    return true;
}

// ============================================================
// GAME LOOP CHÍNH - Theo đúng chuẩn trong gameloop.md
// ============================================================
int GameApp::Run() {
    MSG msg = {};
    mTimer.Reset();

    while (msg.message != WM_QUIT) {
        // 1. Ưu tiên xử lý sự kiện Windows
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // 2. Không có sự kiện → chạy game
        else {
            mTimer.Tick();

            if (!mPaused) {
                CalculateFrameStats();
                Update(mTimer.DeltaTime());
                Render();
            } else {
                // Tránh ăn CPU 100% khi minimize hoặc mất focus
                Sleep(16); // ~60fps sleep
            }
        }
    }
    return (int)msg.wParam;
}

void GameApp::Update(float dt) {
    // Game Loop chỉ cần 1 dòng - không biết gì về logic cụ thể
    StateManager::Get().Update(dt);
}

void GameApp::Render() {
    // Xóa màn hình trước khi State vẽ lên
    D3DContext::Get().BeginFrame(0.05f, 0.05f, 0.1f);

    // State hiện tại tự vẽ
    StateManager::Get().Render();

    // Hiện kết quả lên màn hình
    D3DContext::Get().EndFrame();
}

void GameApp::CalculateFrameStats() {
    // Tính FPS và hiện lên thanh tiêu đề mỗi giây
    static int   frameCnt  = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;
    timeElapsed += mTimer.DeltaTime();

    if (timeElapsed >= 1.0f) {
        float fps  = (float)frameCnt / timeElapsed;
        float mspf = 1000.0f / fps; // milliseconds per frame

        std::wostringstream oss;
        oss.precision(6);
        oss << mTitle << L"  |  FPS: " << fps
            << L"  |  " << mspf << L" ms";
        SetWindowText(mHwnd, oss.str().c_str());

        frameCnt    = 0;
        timeElapsed = 0.0f;
    }
}

void GameApp::OnResize(int newWidth, int newHeight) {
    mWidth  = newWidth;
    mHeight = newHeight;
    D3DContext::Get().OnResize(newWidth, newHeight);

    // Thông báo cho State biết màn hình đã thay đổi kích thước
    // (mở rộng sau: Broadcast event "window_resize")
}

void GameApp::OnActivate(bool active) {
    mPaused = !active;
    if (active)
        mTimer.Start(); // Tiếp tục đếm thời gian
    else
        mTimer.Stop();  // Dừng đếm khi mất focus
}

// ============================================================
// XỬ LÝ SỰ KIỆN CỬA SỔ
// ============================================================
LRESULT GameApp::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ACTIVATE:
        // WA_INACTIVE = 0: cửa sổ mất focus (minimize, alt-tab)
        OnActivate(LOWORD(wParam) != WA_INACTIVE);
        return 0;

    case WM_SIZE:
        if (D3DContext::Get().GetDevice()) {
            int newW = LOWORD(lParam);
            int newH = HIWORD(lParam);
            if (wParam == SIZE_MINIMIZED) {
                mPaused = true;
                mTimer.Stop();
            } else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
                mPaused = false;
                mTimer.Start();
                if (newW > 0 && newH > 0) OnResize(newW, newH);
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
