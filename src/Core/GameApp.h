#pragma once
#include <windows.h>
#include <string>
#include "GameTimer.h"

// ============================================================
// GameApp - Main layer of the application (Application Layer)
//
// Responsibilities:
//   1. Create and manage the Win32 window
//   2. Contain the main Game Loop (according to the standard in gameloop.md)
//   3. Connect D3DContext, StateManager, and GameTimer together
//
// GameApp DOES NOT:
//   - Handle game logic (characters, collisions, AI...)
//   - Render specific graphics (that is the responsibility of each State)
//
// USAGE in main.cpp:
//   GameApp app;
//   if (!app.Initialize(hInstance, L"My Game", 1280, 720))
//       return -1;
//   return app.Run();
// ============================================================
class GameApp {
public:
    GameApp();
    ~GameApp();

    // Initialize Win32 window + DirectX + StateManager
    bool Initialize(HINSTANCE hInstance, const std::wstring& title,
                    int width = 1280, int height = 720);

    // Main loop - returns exit code when finished
    int Run();

    // Win32 callback - must be public for WindowProc to call
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Get the current instance (for WindowProc to forward messages into)
    static GameApp* GetInstance() { return sInstance; }

private:
    // --- Internal initialization ---
    bool InitWindow(HINSTANCE hInstance);
    void CalculateFrameStats(); // Display FPS in the window title

    // --- Per-frame ---
    void Update(float dt);
    void Render();

    // --- Handle window resize ---
    void OnResize(int newWidth, int newHeight);
    void OnActivate(bool active);

    // --- Data ---
    HWND         mHwnd      = nullptr;
    HINSTANCE    mHInstance = nullptr;
    std::wstring mTitle;
    int          mWidth     = 1280;
    int          mHeight    = 720;
    bool         mPaused    = false; // When minimized or lost focus

    GameTimer    mTimer;

    // Static instance pointer - used for WindowProc to forward messages
    // Because WindowProc is a global function, it cannot be a member of GameApp. To allow it to forward messages to the correct GameApp instance, we use a static pointer that is set when the GameApp is created. This way, WindowProc can call GameApp::GetInstance() to get the current instance and call its HandleMessage method.
    static GameApp* sInstance; // Static pointer for WindowProc to forward messages
};
