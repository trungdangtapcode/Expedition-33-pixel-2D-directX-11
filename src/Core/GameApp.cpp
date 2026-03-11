// ============================================================
// File: GameApp.cpp
// Responsibility: Win32 window creation, main game loop, and coordinator.
//
// Owns: HWND, GameTimer, and the top-level Update/Render calls.
//       Does NOT own game logic — that belongs to States and Systems.
//
// Lifetime: Created in main.cpp (WinMain), lives until the process exits.
//
// Important:
//   - AllocConsole() is called in Initialize (DEBUG builds only) to attach
//     a console window so LOG() output is visible without external tools.
//   - UNICODE is defined via /DUNICODE in build_src.bat; never #define it here.
// ============================================================
#include "GameApp.h"
#include "../Renderer/D3DContext.h"
#include "../States/StateManager.h"
#include "../States/MenuState.h"
#include "../Events/EventManager.h"
#include "../Audio/AudioManager.h"
#include "../Utils/Log.h"
#include <sstream>

// Static instance pointer - used in WindowProc
GameApp* GameApp::sInstance = nullptr;

// Win32 Window Procedure (global function, forward to GameApp)
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
    // Shut down the audio engine before COM is uninitialised and before
    // D3D is torn down.  All source voices must be destroyed while the
    // XAudio2 engine is still alive.
    AudioManager::Get().Shutdown();
    sInstance = nullptr;
}

bool GameApp::Initialize(HINSTANCE hInstance, const std::wstring& title,
                         int width, int height)
{
#ifdef _DEBUG
    // ------------------------------------------------------------
    // Attach a console window to this process (DEBUG builds only).
    // Why: WinMain apps have no console by default (/SUBSYSTEM:WINDOWS).
    //      AllocConsole() creates one so that LOG() output via printf()
    //      is visible immediately without needing an external tool.
    // Caveat: freopen_s must redirect stdout/stderr AFTER AllocConsole(),
    //         otherwise printf goes nowhere.
    // ------------------------------------------------------------
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);  // Route printf to console
    freopen_s(&dummy, "CONOUT$", "w", stderr);  // Route error output to console
    LOG("[GameApp] Debug console attached.");
#endif

    // ------------------------------------------------------------
    // Set the working directory to the folder that contains game.exe.
    // Why: the exe lives in bin\, but all asset paths are written relative
    //      to the workspace root (e.g. "assets/animations/verso.json").
    //      GetModuleFileNameW gives the full exe path; we strip the filename
    //      to get the parent folder (bin\), then go one level up (..\).
    //      This keeps every asset path in data files consistent regardless
    //      of how the exe is launched (VS debugger, Explorer, terminal).
    // ------------------------------------------------------------
#ifdef _DEBUG
// ------------------------------------------------------------
// In Debug builds only:
// Set working directory to workspace root so that asset paths
// like "assets/animations/verso.json" work correctly when
// running from Visual Studio.
// ------------------------------------------------------------
{
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH))
    {
        // Remove the executable name (game.exe)
        if (wchar_t* lastSlash = wcsrchr(exePath, L'\\'))
        {
            *lastSlash = L'\0';  // Now exePath = ...\bin

            // Go one level up -> workspace root
            wchar_t rootPath[MAX_PATH] = {};
            _snwprintf_s(rootPath, MAX_PATH, _TRUNCATE, L"%s\\..", exePath);

            if (SetCurrentDirectoryW(rootPath))
            {
                wchar_t cwd[MAX_PATH] = {};
                GetCurrentDirectoryW(MAX_PATH, cwd);
                LOG("[GameApp] Debug working directory: %ls", cwd);
            }
        }
    }
}
#endif

    mHInstance = hInstance;
    mTitle     = title;
    mWidth     = width;
    mHeight    = height;

    if (!InitWindow(hInstance)) return false;

    if (!D3DContext::Get().Initialize(mHwnd, mWidth, mHeight)) return false;

    // Initialise the audio engine AFTER D3D so COM is already set up.
    // A failure here is non-fatal: the game runs silently if XAudio2 is
    // unavailable (e.g., on a headless build/test machine).
    if (!AudioManager::Get().Initialize())
    {
        LOG("[GameApp] WARNING: AudioManager failed to initialize. Game will run without audio.");
    }

    // Push initial state onto the stack — MenuState is the entry point of the game.
    StateManager::Get().PushState(std::make_unique<MenuState>());
    // Push game states here if you want to skip the menu and jump straight into gameplay for testing.
    // StateManager::Get().PushState(std::make_unique<PlayState>());
    LOG("[GameApp] Initialized. First state: MenuState.");

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

    // Calculate window size so that the CLIENT area is exactly mWidth x mHeight.
    // AdjustWindowRect expands the rect to account for title bar and borders.
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

    // Show the window before D3D init to avoid a black flash on startup.
    ShowWindow(mHwnd, SW_SHOW);
    UpdateWindow(mHwnd);
    return true;
}

// ============================================================
// MAIN GAME LOOP — follows the canonical structure in docs/gameloop.md
// ============================================================
int GameApp::Run() {
    MSG msg = {};
    mTimer.Reset();

    while (msg.message != WM_QUIT) {
        // 1. Drain the Win32 message queue first.
        //    PeekMessage returns immediately (non-blocking) so the game
        //    loop never stalls waiting for input events.
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // 2. No pending messages — run one game frame.
        else {
            mTimer.Tick();

            if (!mPaused) {
                CalculateFrameStats();
                Update(mTimer.DeltaTime());
                Render();
            } else {
                // Sleep to avoid burning 100% CPU when minimized or focus lost.
                Sleep(16); // ~60 Hz idle pace
            }
        }
    }
    return (int)msg.wParam;
}

void GameApp::Update(float dt) {
    // The game loop knows nothing about which state is active.
    // Responsibility is fully delegated to StateManager.
    StateManager::Get().Update(dt);
}

void GameApp::Render() {
    // Clear the back buffer before any state draws into it.
    D3DContext::Get().BeginFrame(0.05f, 0.05f, 0.1f);

    // The active state issues all draw calls.
    StateManager::Get().Render();

    // Flip the back buffer to the screen.
    D3DContext::Get().EndFrame();
}

void GameApp::CalculateFrameStats() {
    // Accumulate frames over 1-second windows and display FPS in the title bar.
    // Displaying every frame would cause the title bar to flicker.
    static int   frameCnt    = 0;
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

    // Notify all active states so they can rebuild camera projections and
    // UI layout.  States subscribe to "window_resized" in OnEnter() and
    // unsubscribe in OnExit() to avoid dangling captures.
    EventManager::Get().Broadcast("window_resized");
    LOG("[GameApp] Window resized to %dx%d.", newWidth, newHeight);
}

void GameApp::OnActivate(bool active) {
    mPaused = !active;
    if (active)
        mTimer.Start(); // Resume the high-res timer when the window regains focus.
    else
        mTimer.Stop();  // Freeze delta time while the window is inactive.
}

// ============================================================
// WIN32 MESSAGE HANDLER
// ============================================================
LRESULT GameApp::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ACTIVATE:
        // WA_INACTIVE (0): window lost focus (minimized or alt-tabbed).
        // Pause the timer so deltaTime does not accumulate during inactivity.
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
                // Guard: only resize when the client area actually changed.
                // WM_SIZE fires during ShowWindow() with whatever the OS chose
                // as the initial window size — which may not match mWidth/mHeight
                // set in Initialize().  Skipping equal sizes avoids clobbering
                // the correct 1280x720 swap chain with the OS-measured size.
                if (newW > 0 && newH > 0 &&
                    (newW != D3DContext::Get().GetWidth() ||
                     newH != D3DContext::Get().GetHeight()))
                {
                    OnResize(newW, newH);
                }
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
