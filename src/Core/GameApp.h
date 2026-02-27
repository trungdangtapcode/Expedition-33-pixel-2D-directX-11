#pragma once
#include <windows.h>
#include <string>
#include "GameTimer.h"

// ============================================================
// GameApp - Lớp ứng dụng chính (Application Layer)
//
// Trách nhiệm:
//   1. Tạo và quản lý cửa sổ Win32
//   2. Chứa Game Loop chính (theo chuẩn trong gameloop.md)
//   3. Kết nối D3DContext, StateManager, GameTimer với nhau
//
// GameApp KHÔNG làm:
//   - Logic game (nhân vật, va chạm, AI...)
//   - Vẽ đồ họa cụ thể (đó là việc của từng State)
//
// CÁCH DÙNG trong main.cpp:
//   GameApp app;
//   if (!app.Initialize(hInstance, L"My Game", 1280, 720))
//       return -1;
//   return app.Run();
// ============================================================
class GameApp {
public:
    GameApp();
    ~GameApp();

    // Khởi tạo cửa sổ Win32 + DirectX + StateManager
    bool Initialize(HINSTANCE hInstance, const std::wstring& title,
                    int width = 1280, int height = 720);

    // Vòng lặp chính - trả về exit code khi kết thúc
    int Run();

    // Win32 callback - phải public để WindowProc có thể gọi
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Lấy instance hiện tại (để WindowProc forward message vào)
    static GameApp* GetInstance() { return sInstance; }

private:
    // --- Khởi tạo nội bộ ---
    bool InitWindow(HINSTANCE hInstance);
    void CalculateFrameStats(); // Hiện FPS lên thanh tiêu đề

    // --- Mỗi frame ---
    void Update(float dt);
    void Render();

    // --- Xử lý thay đổi cửa sổ ---
    void OnResize(int newWidth, int newHeight);
    void OnActivate(bool active);

    // --- Dữ liệu ---
    HWND         mHwnd      = nullptr;
    HINSTANCE    mHInstance = nullptr;
    std::wstring mTitle;
    int          mWidth     = 1280;
    int          mHeight    = 720;
    bool         mPaused    = false; // Khi minimize hoặc mất focus

    GameTimer    mTimer;

    static GameApp* sInstance; // Con trỏ static để WindowProc forward message
};
