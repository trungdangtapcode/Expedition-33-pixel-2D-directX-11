// ============================================================
// File: PlayState.cpp
// Responsibility: Camera-follow gameplay state — Verso character controlled via WASD.
//
// Architecture:
//   PlayState knows NOTHING about how ControllableCharacter moves or renders.
//   It spawns the character into SceneGraph, then calls only:
//     mScene.Update(dt)   — drives all entity logic
//     mScene.Render(ctx)  — drives all entity drawing
//   Camera follow is the only reason PlayState holds a ControllableCharacter*,
//   and only the narrow GetX()/GetY() interface is used.
//
// Scene:
//   Blue circle              — static world-space landmark, CircleRenderer SDF.
//   ControllableCharacter    — Verso sprite, WASD controlled, SceneGraph-owned.
//   Camera2D                 — follows ControllableCharacter with smooth lerp.
//
// Input:
//   W / A / S / D — move the Verso character
//   ESC           — return to MenuState
// ============================================================
#include "PlayState.h"
#include "StateManager.h"
#include "MenuState.h"
#include "BattleState.h"
#include "../Renderer/D3DContext.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"
#include <windows.h>


// ------------------------------------------------------------
// Function: OnEnter
// Purpose:
//   1. Initialize CircleRenderer (static blue landmark).
//   2. Build Camera2D from current screen dimensions.
//   3. Load the Verso SpriteSheet from JSON.
//   4. Spawn ControllableCharacter into SceneGraph — it owns the GPU resources.
//   5. Subscribe to window_resized to keep Camera2D in sync.
// ------------------------------------------------------------
void PlayState::OnEnter()
{
    LOG("[PlayState] OnEnter");
    ID3D11Device*        device  = D3DContext::Get().GetDevice();
    ID3D11DeviceContext* context = D3DContext::Get().GetContext();

    // --- CircleRenderer — still used for the blue static landmark ---
    if (!mCircleRenderer.Initialize(device)) {
        LOG("[PlayState] ERROR — CircleRenderer initialization failed.");
    }

    // --- Camera: constructed here so we know the real screen dimensions ---
    const int W = D3DContext::Get().GetWidth();
    const int H = D3DContext::Get().GetHeight();
    mCamera = std::make_unique<Camera2D>(W, H);

    // --- Load sprite sheet from JSON (shared by ControllableCharacter) ---
    SpriteSheet sheet;
    if (!JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet)) {
        LOG("[PlayState] ERROR — Failed to load verso.json.");
        return;
    }

    // --- Spawn player character into SceneGraph ---
    // SceneGraph::Spawn<T> constructs the object with make_unique<T>(args...)
    // and returns a non-owning T* for the camera follow reference.
    // ControllableCharacter initializes its own WorldSpriteRenderer internally;
    // PlayState has no knowledge of textures, clips, or SpriteBatch.
    mPlayer = mScene.Spawn<ControllableCharacter>(
        device, context,
        L"assets/animations/verso.png",
        sheet,
        std::string("idle"),
        0.0f, 0.0f,           // start at world origin
        mCamera.get()         // non-owning camera reference
    );

    // DEBUG: load the raw texture to verify SpriteBatch works at all.
    mDebugView.Load(device, context, L"assets/animations/test.png");

    // Subscribe to window resize so Camera2D stays in sync.
    mResizeListenerID = EventManager::Get().Subscribe("window_resized",
        [this](const EventData&)
        {
            const int nW = D3DContext::Get().GetWidth();
            const int nH = D3DContext::Get().GetHeight();
            if (mCamera) mCamera->SetScreenSize(nW, nH);
            LOG("[PlayState] window_resized -> %dx%d", nW, nH);
        });
}

// ------------------------------------------------------------
// Function: OnExit
// Purpose:
//   Release all GPU resources.
//   SceneGraph::Clear() destroys all entities (unique_ptr destructor calls
//   ControllableCharacter::~ControllableCharacter → mRenderer.Shutdown()).
//   mPlayer becomes a dangling pointer after Clear() — set to nullptr immediately.
// ------------------------------------------------------------
void PlayState::OnExit()
{
    LOG("[PlayState] OnExit");
    EventManager::Get().Unsubscribe("window_resized", mResizeListenerID);
    mCircleRenderer.Shutdown();
    mDebugView.Shutdown();

    // Clear ALL entities.  ControllableCharacter's destructor releases GPU resources.
    // mPlayer must be nulled BEFORE Clear() returns to prevent any stale use.
    mPlayer = nullptr;
    mScene.Clear();

    mCamera.reset();
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   1. Handle ESC → transition to MenuState.
//   2. Delegate all entity logic to SceneGraph::Update(dt).
//      ControllableCharacter::Update() runs WASD, physics, animation — all inside.
//   3. Camera follow via the narrow GetX()/GetY() interface.
//
// PlayState never reads velocity, position, or animation state directly.
// The contract is: IGameObject::Update(dt) → black box.
// ------------------------------------------------------------
void PlayState::Update(float dt)
{
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        StateManager::Get().ChangeState(std::make_unique<MenuState>());
        return;
    }

    // B key — push BattleState onto the stack.
    // PlayState stays paused underneath; it resumes when the battle ends.
    // The static flag prevents repeated triggers while the key is held down.
    static bool sBWasDown = false;
    const bool bDown = (GetAsyncKeyState('B') & 0x8000) != 0;
    if (bDown && !sBWasDown)
    {
        LOG("%s", "[PlayState] B pressed — entering BattleState");
        StateManager::Get().PushState(std::make_unique<BattleState>(D3DContext::Get()));
    }
    sBWasDown = bDown;

    // All entity logic (WASD, physics, animation) runs inside each object.
    mScene.Update(dt);

    // Camera follow — only valid use of mPlayer* here.
    // If the player is null (initialization failed), the camera stays at origin.
    if (mPlayer && mCamera) {
        mCamera->Follow(mPlayer->GetX(), mPlayer->GetY(), kCameraSmoothing, dt);
        mCamera->Update();
    }
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   1. Draw the static blue circle (world-space SDF, CPU WorldToScreen).
//   2. Call SceneGraph::Render(ctx) — draws all entities in layer order.
//      ControllableCharacter renders itself; PlayState has zero draw-call knowledge.
//
// Draw order:
//   Blue circle first  (layer 0 equivalent, drawn before SceneGraph)
//   SceneGraph entities in ascending layer order (ControllableCharacter = layer 50)
// ------------------------------------------------------------
void PlayState::Render()
{
    if (!mCamera) return;

    ID3D11DeviceContext* ctx = D3DContext::Get().GetContext();
    const int W = D3DContext::Get().GetWidth();
    const int H = D3DContext::Get().GetHeight();

    // --- Blue static circle (world-space SDF landmark) ---
    // CircleRenderer needs screen-pixel coordinates; WorldToScreen applies camera VP.
    DirectX::XMFLOAT2 blueScreen = mCamera->WorldToScreen(kBlueX, kBlueY);
    const float zoomedBlueRadius = kBlueRadius * mCamera->GetZoom();

    mDebugView.Draw(ctx, W, H, { true });

    mCircleRenderer.Draw(ctx,
        blueScreen.x, blueScreen.y, zoomedBlueRadius,
        0.15f, 0.35f, 1.0f,
        W, H);

    // --- All SceneGraph entities (sorted by layer, self-rendering) ---
    // ControllableCharacter::Render() rebinds the RTV internally before
    // calling WorldSpriteRenderer::Draw(), so CircleRenderer state cleanup
    // cannot leave the render target unbound.
    mScene.Render(ctx);
}
