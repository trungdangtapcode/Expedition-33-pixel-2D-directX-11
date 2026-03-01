// main.cpp - Entry point of this game
//
// This is the ONLY file containing WinMain.
// All logic is delegated to GameApp.
// ============================================================
// UNICODE defined via /DUNICODE in build script,
// No need to redefine it here to avoid warning C4005
#include <windows.h>
#include "Core/GameApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    GameApp app;

    if (!app.Initialize(hInstance, L"My Game - DirectX 11", 1280, 720)) {
        MessageBoxW(nullptr, L"Initialization failed. See previous error dialog for details.",
                    L"Fatal Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    return app.Run();
}
