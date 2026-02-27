// main.cpp - Entry point của ứng dụng
//
// Đây là file DUY NHẤT chứa WinMain.
// Toàn bộ logic được ủy quyền cho GameApp.
// ============================================================
// UNICODE được define qua /DUNICODE trong build script,
// không cần define lại ở đây để tránh warning C4005
#include <windows.h>
#include "Core/GameApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    GameApp app;

    if (!app.Initialize(hInstance, L"My Game - DirectX 11", 1280, 720)) {
        MessageBoxW(nullptr, L"Khởi tạo thất bại!", L"Lỗi", MB_OK | MB_ICONERROR);
        return -1;
    }

    return app.Run();
}
