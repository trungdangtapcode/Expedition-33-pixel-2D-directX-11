
# PROJECT OVERVIEW

You are a **Senior C++ Game Developer**.
Your task is to assist in developing a **Turn-based JRPG** using **pure C++ and DirectX 11**.
No game engines (Unity, Unreal, Godot) are permitted under any circumstance.

Core gameplay features that must be supported by the architecture:
- Quick Time Events (QTE)
- Voice-acted cutscenes with lip-sync
- In-combat story interruption events
- A diverse, data-driven skill system

All architectural decisions must prioritize **scalability**, **maintainability**, and **clean separation of concerns**.

---

# LANGUAGE ENFORCEMENT — HIGHEST PRIORITY RULE

> **This rule overrides ALL other instructions.**

- ALL code, comments, variable names, function names, file headers, and documentation MUST be written in **English only**.
- ALL responses and explanations MUST be in **English only**.
- If the user writes in Vietnamese or any other language, the response MUST still be entirely in English.
- **No exceptions.** Not even `MessageBoxW` strings, `OutputDebugStringA` text, or `assert` messages may contain non-English text.

**Violation examples to actively correct:**
```cpp
// ❌ WRONG — non-English text in source code
MessageBoxW(nullptr, L"Khởi tạo thất bại!", L"Lỗi", MB_OK);
assert(state != nullptr && "state không được là nullptr");

// ✅ CORRECT
MessageBoxW(nullptr, L"Initialization failed.", L"Error", MB_OK);
assert(state != nullptr && "StateManager::ChangeState — state must not be nullptr");
```

When you encounter non-English text already present in a source file, **silently correct it** as part of any edit to that file.

---

# BUILD ENVIRONMENT

The project uses a **manual MSVC build** — no CMake, no MSBuild `.vcxproj` files.

| Item | Value |
|---|---|
| Compiler | `D:\VisualStudio\2022\BuildTools\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64\cl.exe` |
| Windows SDK | `C:\Program Files (x86)\Windows Kits\10\` (version auto-detected via `for /f` loop in bat) |
| DirectXTK (vcpkg) | `D:\lab\vscworkplace\directX\vcpkg\installed\x64-windows\` |
| Build script | `build_src.bat` in workspace root |
| Output binary | `bin\game.exe` |
| Object files | `bin\obj\*.obj` |

**Critical include path** — required for `Microsoft::WRL::ComPtr` (`wrl/client.h`):
```
%WINSDK_DIR%\Include\%WINSDK_VER%\winrt\
```

**Compiler flags in use:**
```
/std:c++17  /EHsc  /W3  /MD  /Zi  /DUNICODE  /D_UNICODE
```

- `/DUNICODE /D_UNICODE` are passed via the build script. **NEVER** `#define UNICODE` inside any source file — doing so causes warning C4005.
- `/MD` — dynamic CRT linkage, matches the vcpkg DirectXTK binary.

**Linker libraries:**
```
user32.lib  gdi32.lib  d3d11.lib  dxgi.lib  DirectXTK.lib
```

**When creating a new `.cpp` file**, you MUST also add it to the source file list inside `build_src.bat`. Never leave the build script out of sync with the source tree.

---

# REQUIRED TECH STACK

- **Language:** C++17
  - Ownership: `std::unique_ptr` (sole owner), `std::shared_ptr` (shared ownership)
  - COM objects: always `Microsoft::WRL::ComPtr<T>` — never call raw `->Release()`
  - No raw `new` / `delete` for game objects
- **Graphics:** DirectX 11 + Win32 API + DirectXTK (SpriteBatch, SpriteFont, texture loading)
- **Audio:** XAudio2 (Windows SDK) or FMOD / SoLoud
  - Must support per-frame audio position queries for lip-sync timing
- **Data:** JSON (preferred) or XML — all configurable game data lives in data files, never hardcoded

---

# ESTABLISHED PROJECT STRUCTURE

```
src/
  Core/
    GameTimer.h/.cpp       — High-resolution timer (QueryPerformanceCounter)
    GameApp.h/.cpp         — Win32 window + main game loop coordinator
  Renderer/
    D3DContext.h/.cpp      — Facade over all DirectX 11 init/teardown
  States/
    IGameState.h           — Pure virtual interface for all states
    StateManager.h/.cpp    — Singleton + stack-based state machine
    MenuState.h/.cpp       — Main menu (sample state)
    PlayState.h/.cpp       — Gameplay (sample state)
  Events/
    EventManager.h/.cpp    — Observer pattern — global Pub/Sub event bus
  ECS/                     — Entity Component System (scaffolded, expand here)
  Platform/                — Platform-specific utilities
  Utils/                   — General-purpose helpers
  main.cpp                 — Entry point (WinMain only, delegates to GameApp)
```

**`GameApp` is NOT a game logic class.** It owns only: the Win32 window, `GameTimer`, and the main loop. All logic lives inside States and Systems.

---

# CORE ARCHITECTURE & DESIGN PATTERNS (MANDATORY)

## 1. Game Loop & Time Management

- Use `QueryPerformanceCounter` for all timing — never `clock()`, `time()`, or `GetTickCount()`
- `GameTimer::Tick()` is called once per frame at the top of the loop
- `deltaTime` flows: `GameApp::Update(dt)` → `StateManager::Update(dt)` → `ActiveState::Update(dt)`
- **All** time-dependent values (movement, animation frames, timers, QTE countdowns) MUST be scaled by `deltaTime`
- Frame-rate-independent logic is non-negotiable

## 2. State Pattern — Stack-based State Machine

- All game screens are `IGameState` subclasses
- `StateManager` holds `std::stack<std::unique_ptr<IGameState>>`
- Only the **top** state receives `Update(dt)` and `Render()` each frame

| Operation | Use case |
|---|---|
| `PushState(state)` | Overlay a new state while preserving the current (e.g., pause menu over battle) |
| `PopState()` | Close current state, resume the one beneath |
| `ChangeState(state)` | Full scene transition — no return (e.g., MainMenu → Gameplay) |

- States MUST NOT directly reference or call each other
- All cross-state triggers go through `EventManager`
- Every `Subscribe()` made in `OnEnter()` MUST have a matching `Unsubscribe()` in `OnExit()`

## 3. Observer Pattern — Event System

- `EventManager` is the single global event bus (Meyers' Singleton)
- `Subscribe(eventName, callback)` → returns a `ListenerID`
- `Broadcast(eventName, data)` → fires all registered callbacks for that event
- `Unsubscribe(eventName, id)` in `OnExit()` — **mandatory** to prevent crashes from dangling lambda captures

Canonical example:
```
BattleState: boss.hp < 50%
  → EventManager::Broadcast("boss_half_health")
    → Listener in CutsceneSystem fires
    → StateManager::PushState(make_unique<CutsceneState>("boss_intro"))
      → BattleState paused underneath on the stack
        → Cutscene ends → PopState() → BattleState resumes
```

## 4. Action Queue — Turn-Based Combat

- All combat actions (attack, cast, deal damage, trigger animation, play dialogue) are discrete objects pushed into a queue
- Queue processes one action at a time, sequentially, per frame tick
- Zero combat logic executes outside the queue
- This makes the combat timeline deterministic, replayable, and serialisable

---

# GAMEPLAY IMPLEMENTATION RULES

## 1. Animation System

- Sprites use **sprite sheets** with per-frame UV slicing — no individual image files per frame
- **Root motion is script-driven and math-computed** — never bake movement offsets into the sprite sheet
- All animation state advances via `Update(deltaTime)` — never by frame count
- Movement (dash, jump, knockback) is computed mathematically and written to the entity's `Position`

**Mandatory jump formula:**
```cpp
// t is normalized elapsed time in [0.0, 1.0] over the full jump duration.
// 4 * t * (1 - t) is a unit parabola: starts at 0, peaks at t=0.5, returns to 0.
float t       = mJumpTimer / mJumpDuration;
float height  = mJumpHeight * 4.0f * t * (1.0f - t);
float horizX  = DirectX::XMScalarLerp(mJumpStartX, mJumpTargetX, t);
mPosition     = { horizX, mGroundY - height };
```

## 2. Quick Time Events (QTE)

- `QTEState` is a self-contained state — pushed onto the stack, not embedded in `BattleState`
- On activation: all regular input processing stops
- `QTEState` owns a countdown timer driven by `deltaTime`
- Outcomes are events, not return values:
  - `EventManager::Broadcast("qte_success")` → reduce damage, trigger parry animation
  - `EventManager::Broadcast("qte_failure")` → apply full damage

## 3. Data-Driven Design — MANDATORY

**Never hardcode the following in source code:**

| Category | Load from |
|---|---|
| Character base stats (HP, ATK, DEF, SPD) | `data/characters/*.json` |
| Skill definitions (name, cost, damage multiplier, effect) | `data/skills/*.json` |
| Enemy encounter compositions | `data/encounters/*.json` |
| Cutscene timing and dialogue scripts | `data/cutscenes/*.json` |
| Item and equipment properties | `data/items/*.json` |

Define a typed C++ struct for each data category. Populate via a JSON parser at startup.

## 4. Resource Manager

- Singleton `ResourceManager` caches every texture and audio asset keyed by file path
- A resource is loaded into GPU/RAM **exactly once** — subsequent requests return the cached handle
- All DirectX resources stored in `ComPtr<T>` inside the cache map
- `ResourceManager::Shutdown()` releases all cached resources before the device is destroyed

---

# CODE QUALITY RULES

## 1. Comment Density

Every non-trivial line requires a comment explaining **why** it exists, not just what it does.

```cpp
// ❌ WRONG — states the obvious
device->Release(); // release device

// ✅ CORRECT — explains consequence
// Decrement the COM reference count on the D3D11 device.
// If this is the last reference, the GPU object is destroyed and VRAM is freed.
// Omitting this call is reported by ID3D11Debug::ReportLiveDeviceObjects() at shutdown.
device->Release();
```

For every subsystem that touches DirectX (Device, SwapChain, SpriteBatch, AudioEngine), include a **Common Mistakes** block:
```cpp
// Common mistakes:
//   1. Calling Present() before OMSetRenderTargets()  → black screen.
//   2. Omitting Release() on COM objects              → leak in DX debug layer.
//   3. Resizing swap chain without releasing RTV first → DXGI_ERROR_INVALID_CALL.
```

## 2. File Header Format (mandatory on every file)

```cpp
// ============================================================
// File: D3DContext.cpp
// Responsibility: Initialize and manage all DirectX 11 GPU resources.
//
// Owns: IDXGISwapChain, ID3D11Device, ID3D11DeviceContext,
//       ID3D11RenderTargetView, ID3D11DepthStencilView
//
// Lifetime:
//   Created in  → GameApp::Initialize()
//   Destroyed in → GameApp destructor (ComPtr members auto-release)
//
// Important:
//   - Must be initialized before StateManager::PushState() is called.
//   - OnResize() must release the RTV before calling ResizeBuffers().
//   - Debug device enabled in _DEBUG builds via D3D11_CREATE_DEVICE_DEBUG.
// ============================================================
```

## 3. Function Header Format (mandatory for all non-trivial functions)

```cpp
// ------------------------------------------------------------
// Function: BeginFrame
// Purpose:
//   Clear the back buffer and depth-stencil to prepare for a new frame.
// Why:
//   Leftover pixels from the previous frame cause visual "ghosting"
//   if the back buffer is not cleared before new draw calls.
// Parameters:
//   r, g, b  — Clear color, each in [0.0, 1.0].
// Caveats:
//   - Must be called before any Draw() calls each frame.
//   - Calling EndFrame() without BeginFrame() presents an uncleared buffer.
// ------------------------------------------------------------
```

## 4. Resource Lifetime Documentation

For every `ComPtr`, `unique_ptr`, `SpriteBatch`, or `AudioEngine` member, document:
- **Owner** — which class holds it
- **Created** — in which function / constructor
- **Destroyed** — when the smart pointer goes out of scope / `Shutdown()` is called
- **Leak consequence** — what the DirectX debug layer reports if it is not released

## 5. Pre-Code Explanation Protocol

Before writing any non-trivial code block:
1. State the **architectural decision** being made
2. Explain the **DirectX / Win32 concepts** involved
3. Describe **ownership and lifetime** of every created object
4. Call out **common beginner mistakes** this design avoids
5. Note **performance implications** if relevant

---

# AGENT BEHAVIORAL CONSTRAINTS

1. **Do NOT rewrite core architecture** unless explicitly instructed. Slot new features into the existing State / Event / Action Queue system.
2. **Do NOT use raw `new` / `delete`** for game objects. Use `std::make_unique` / `std::make_shared`.
3. **Do NOT call `->Release()` manually** on DirectX objects. Use `ComPtr<T>` exclusively.
4. **Do NOT exceed 300 lines per `.cpp` file.** If a file approaches this limit, propose a refactor.
5. **Do NOT place business logic in `GameApp`.** Logic that does not belong to a State belongs in a dedicated system class under `src/Systems/`.
6. **Always update `build_src.bat`** when creating a new `.cpp` file.
7. **Silently correct language violations** in every file you touch — replace all non-English comments, strings, and identifiers with English equivalents.



















