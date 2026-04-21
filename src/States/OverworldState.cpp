// ============================================================
// File: OverworldState.cpp
// Responsibility: Camera-follow gameplay state — overworld exploration.
//
// Architecture:
//   OverworldState knows NOTHING about how ControllableCharacter moves or renders.
//   It spawns the character into SceneGraph, then calls only:
//     mScene.Update(dt)   — drives all entity logic
//     mScene.Render(ctx)  — drives all entity drawing
//   Camera follow is the only reason OverworldState holds a ControllableCharacter*,
//   and only the narrow GetX()/GetY() interface is used.
//
//   OverworldEnemy entities are spawned the same way.  OverworldState only holds
//   non-owning pointers to call IsPlayerNearby() and GetEncounterData().
//
// Battle trigger (2-phase sequence — NO iris in overworld):
//   1. PINCUSHION  — B pressed near enemy:
//                    TimeSystem::SetSlowMotion(0.25) slows gameplay.
//                    PincushionDistortionFilter ramps intensity 0→1 over
//                    kPincushionDuration seconds using the UI clock (wall-accurate).
//   2. IDLE (push) — intensity reached 1.0:
//                    slow-motion reset to 1.0, BattleState pushed immediately.
//
//   BattleState::OnEnter() starts its own iris at radius=0 (black) then opens.
//   BattleState pops → OverworldState resumes normally (no iris state to manage).
//
// Scene:
//   Blue circle              — static world-space landmark, CircleRenderer SDF.
//   ControllableCharacter    — Verso sprite, WASD controlled, SceneGraph-owned.
//   OverworldEnemy (1..N)    — stationary enemies, SceneGraph-owned.
//   Camera2D                 — follows ControllableCharacter with smooth lerp.
//   PincushionDistortionFilter — fullscreen warp effect during transition phase.
//
// Input:
//   W / A / S / D — move the Verso character
//   ESC           — return to MenuState
//   B (near enemy) — trigger battle transition (pincushion → push BattleState)
// ============================================================
#include "OverworldState.h"
#include "StateManager.h"
#include "MenuState.h"
#include "BattleState.h"
#include "InventoryState.h"
#include "../Renderer/D3DContext.h"
#include "../Systems/ZoomPincushionTransitionController.h"
#include "../Core/TimeSystem.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"
#include <algorithm>
#include <windows.h>


// ------------------------------------------------------------
// Function: OnEnter
// Purpose:
//   1. Initialize CircleRenderer (static blue landmark).
//   2. Build Camera2D from current screen dimensions.
//   3. Load the Verso SpriteSheet from JSON.
//   4. Spawn ControllableCharacter into SceneGraph.
//   5. Spawn OverworldEnemy entities from data/enemies/*.json.
//   6. Initialize PincushionDistortionFilter for the battle transition.
//   7. Subscribe to window_resized to keep Camera2D in sync.
//   8. Subscribe to battle_end_victory to remove defeated enemies.
// ------------------------------------------------------------
void OverworldState::OnEnter()
{
    LOG("[OverworldState] OnEnter");

    EventManager::Get().Broadcast("bgm_play_overworld", {});
    ID3D11Device*        device  = D3DContext::Get().GetDevice();
    ID3D11DeviceContext* context = D3DContext::Get().GetContext();
    const int W = D3DContext::Get().GetWidth();
    const int H = D3DContext::Get().GetHeight();

    // --- CircleRenderer — still used for the blue static landmark ---
    if (!mCircleRenderer.Initialize(device)) {
        LOG("[OverworldState] ERROR — CircleRenderer initialization failed.");
    }

    // --- Tile Map ---
    if (!mTileMap.Initialize(device, context, "assets/environments/overworld_map.json")) {
        LOG("[OverworldState] WARNING — Tile map failed to load.");
    }

    // --- Camera ---
    mCamera = std::make_unique<Camera2D>(W, H);

    // --- Load Verso sprite sheet ---
    SpriteSheet sheet;
    if (!JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet)) {
        LOG("[OverworldState] ERROR — Failed to load verso.json.");
        return;
    }

    // --- Spawn player character ---
    mPlayer = mScene.Spawn<ControllableCharacter>(
        device, context,
        L"assets/animations/verso.png",
        sheet,
        std::string("idle"),
        0.0f, 0.0f,
        mCamera.get(),
        &mTileMap.GetData().colliders
    );

    // --- Spawn overworld enemies ---
    // Each overworld enemy is loaded from its own JSON, which describes both
    // the overworld sprite (top-level fields) and the battle party (battleParty[]).
    // Spawn positions are representative overworld locations.
    // In a full game these would come from a level/map JSON file.

    // Spawn a solo skeleton (1-on-1 fight) at position (300, 150).
    {
        EnemyEncounterData soloData{};
        if (JsonLoader::LoadEnemyEncounterData("data/enemies/skeleton.json", soloData))
        {
            OverworldEnemy* e = mScene.Spawn<OverworldEnemy>(
                device, context, soloData, 300.0f, 150.0f, mCamera.get());
            if (e) mOverworldEnemies.push_back(e);
        }
        else
        {
            LOG("[OverworldState] WARNING — Could not load skeleton.json; solo enemy not spawned.");
        }
    }

    // Spawn a skeleton group (3-on-party fight) at position (-250, -100).
    // The group uses a separate JSON so encounter composition is data-driven,
    // not controlled by spawn-count in code.
    {
        EnemyEncounterData groupData{};
        if (JsonLoader::LoadEnemyEncounterData("data/enemies/skeleton_group.json", groupData))
        {
            OverworldEnemy* e = mScene.Spawn<OverworldEnemy>(
                device, context, groupData, -250.0f, -100.0f, mCamera.get());
            if (e) mOverworldEnemies.push_back(e);
        }
        else
        {
            LOG("[OverworldState] WARNING — Could not load skeleton_group.json; group enemy not spawned.");
        }
    }

    // --- Pincushion and Camera Effect for battle transition ---
    // Created via the concrete type but stored behind the interface.
    mTransitionController = std::make_unique<ZoomPincushionTransitionController>();
    if (!mTransitionController->Initialize(device, W, H))
    {
        LOG("[OverworldState] WARNING — ZoomPincushionTransitionController init failed; battle transition will skip distortion.");
        mTransitionController.reset();  // disable filter rather than crash on use
    }

    // Reset transition state in case this state is re-entered (e.g., returning from battle).
    mBattleTransitionPhase = BattleTransitionPhase::IDLE;

    // DEBUG: load the raw texture to verify SpriteBatch works at all.
    mDebugView.Load(device, context, L"assets/animations/test.png");

    // Subscribe to window resize so Camera2D stays in sync.
    mResizeListenerID = EventManager::Get().Subscribe("window_resized",
        [this](const EventData&)
        {
            const int nW = D3DContext::Get().GetWidth();
            const int nH = D3DContext::Get().GetHeight();
            if (mCamera) mCamera->SetScreenSize(nW, nH);
            if (mTransitionController) mTransitionController->OnResize(nW, nH);
            LOG("[OverworldState] window_resized -> %dx%d", nW, nH);
        });

    // Subscribe to "battle_end_victory" to remove the defeated overworld enemy.
    // This fires inside BattleState::Update() (deferred-exit block) while OverworldState
    // is still alive underneath the stack.  It is safe to access mOverworldEnemies
    // because OverworldState has NOT been destroyed yet.
    //
    // Why erase from mOverworldEnemies immediately?
    //   MarkDefeated() sets mAlive=false, and SceneGraph::PurgeDead() will free the
    //   UniquePtr on the next Update() pass.  If we leave the raw pointer in
    //   mOverworldEnemies past that point, the proximity loop would dereference
    //   freed memory.  Erasing here, before PurgeDead() runs, prevents that.
    mVictoryListenerID = EventManager::Get().Subscribe("battle_end_victory",
        [this](const EventData&)
        {
            if (mPendingEnemySource)
            {
                // Mark the entity dead so SceneGraph::PurgeDead() frees it next frame.
                mPendingEnemySource->MarkDefeated();

                // Remove the raw pointer from the tracking vector before PurgeDead()
                // frees the entity; any later access would be a use-after-free.
                mOverworldEnemies.erase(
                    std::remove(mOverworldEnemies.begin(),
                                mOverworldEnemies.end(),
                                mPendingEnemySource),
                    mOverworldEnemies.end());

                LOG("[OverworldState] battle_end_victory -> enemy defeated and removed from overworld.");
                mPendingEnemySource = nullptr;
            }
        });
}

// ------------------------------------------------------------
// Function: OnExit
// Purpose:
//   Release all GPU resources.
//   SceneGraph::Clear() destroys all entities (unique_ptr destructor calls
//   ControllableCharacter/OverworldEnemy destructors → Shutdown()).
//   mPlayer and mOverworldEnemies become dangling after Clear() — clear them first.
// ------------------------------------------------------------
void OverworldState::OnExit()
{
    LOG("[OverworldState] OnExit");

    // Stop BGM when OverworldState is fully dismissed (e.g., transitioning to MenuState).
    // Not broadcast when BattleState is pushed — that push triggers its own
    // "bgm_play_battle" event, so the audio transitions cleanly without a gap.
    EventManager::Get().Broadcast("bgm_stop", {});

    EventManager::Get().Unsubscribe("window_resized", mResizeListenerID);
    EventManager::Get().Unsubscribe("battle_end_victory", mVictoryListenerID);

    // Clear the source pointer regardless of whether a battle was in progress.
    // Prevents any stale pointer dereference if OnExit() is called mid-battle
    // (e.g., a forced state change that bypasses the normal victory path).
    mPendingEnemySource = nullptr;

    mCircleRenderer.Shutdown();
    mTileMap.Shutdown();
    mDebugView.Shutdown();

    // Release transition controller GPU resources.
    if (mTransitionController)
    {
        mTransitionController->Shutdown();
        mTransitionController.reset();
    }

    // Ensure transition state is clean for potential re-entry.
    mBattleTransitionPhase = BattleTransitionPhase::IDLE;

    // Clear non-owning pointers BEFORE SceneGraph::Clear() frees the entities.
    // Accessing these pointers after Clear() is a use-after-free.
    mPlayer = nullptr;
    mOverworldEnemies.clear();

    // Destroy all SceneGraph entities (ControllableCharacter, OverworldEnemy, etc.).
    mScene.Clear();

    mCamera.reset();
    mBWasDown = false;
    mIWasDown = false;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   1. Handle ESC → transition to MenuState.
//   2. Delegate all entity logic to SceneGraph::Update(dt).
//   3. Check proximity to overworld enemies; if B pressed near one → start transition.
//   4. Handle pincushion phase: ramp filter intensity using UI clock dt.
//   5. Push BattleState directly when pincushion completes (no iris in overworld).
//   6. Camera follow via the narrow GetX()/GetY() interface.
//
// Battle trigger sequence:
//   Phase IDLE:
//     B pressed + IsPlayerNearby() (no already-transitioning guard needed):
//       → set mBattleTransitionPhase = PINCUSHION
//       → reset mPincushionTimer to 0
//       → TimeSystem::SetSlowMotion(0.25) — gameplay slows to 25%
//
//   Phase PINCUSHION (each frame):
//     → mPincushionTimer += UI clock dt (wall-accurate, ignores slow-mo)
//     → intensity = mPincushionTimer / kPincushionDuration  clamped to [0,1]
//     → mPincushionFilter->Update(uiDt, intensity)
//     → when timer >= kPincushionDuration:
//         → TimeSystem::SetSlowMotion(1.0) — restore normal speed
//         → StateManager::PushState(BattleState) immediately
//         → mBattleTransitionPhase = IDLE
//
//   Why use UI clock for pincushion timer?
//     The pincushion should last kPincushionDuration real-world seconds regardless
//     of the slow-motion scale.  Using the gameplay clock (which runs at 0.25x)
//     would stretch the distortion to 2.4s instead of 0.6s — inconsistent feel.
// ------------------------------------------------------------
void OverworldState::Update(float dt)
{
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
        // Restore normal time scale before leaving, in case transition was active.
        TimeSystem::Get().SetSlowMotion(1.0f);
        StateManager::Get().ChangeState(std::make_unique<MenuState>());
        return;
    }

    // ---------------------------------------------------------------
    // 'I' key — open the inventory.  One-press semantics via mIWasDown
    // so the same press that opens InventoryState does not also
    // immediately close it on the next frame (InventoryState's own
    // OnEnter starts with mIWasDown=true to absorb the held key).
    //
    // PushState rather than ChangeState so the overworld is preserved
    // beneath the stack and resumes unchanged on PopState.  Same
    // pattern BattleState uses to overlay on top of overworld.
    // ---------------------------------------------------------------
    {
        const bool iDown    = (GetAsyncKeyState('I') & 0x8000) != 0;
        const bool iPressed = iDown && !mIWasDown;
        mIWasDown = iDown;
        if (iPressed)
        {
            StateManager::Get().PushState(std::make_unique<InventoryState>());
            return;  // do NOT run the rest of Update — the new state owns this frame
        }
    }

    // All entity logic (WASD, physics, animation, enemy idle) runs here.
    // dt is gameplay-clock-scaled so entities respect slow-motion automatically.
    mScene.Update(dt);

    // ---------------------------------------------------------------
    // Battle trigger: B key + enemy proximity + IDLE phase only.
    // The IDLE phase guard prevents re-triggering while a transition is active.
    // ---------------------------------------------------------------
    const bool bDown    = (GetAsyncKeyState('B') & 0x8000) != 0;
    const bool bPressed = bDown && !mBWasDown;
    mBWasDown = bDown;

    if (bPressed && mBattleTransitionPhase == BattleTransitionPhase::IDLE && mPlayer)
    {
        const float px = mPlayer->GetX();
        const float py = mPlayer->GetY();

        // Find the closest enemy within contact radius.
        // First match wins — ties resolved by vector order (spawn order).
        OverworldEnemy* target = nullptr;
        for (OverworldEnemy* enemy : mOverworldEnemies)
        {
            if (enemy && enemy->IsAlive() && enemy->IsPlayerNearby(px, py))
            {
                target = enemy;
                break;
            }
        }

        if (target)
        {
            // Copy encounter data before beginning the transition; the enemy
            // entity must NOT be accessed from the iris-close callback because
            // it might have been purged by the time the callback fires.
            mPendingEncounter   = target->GetEncounterData();
            mPendingEnemySource = target;

            // Start the transition phase: slow gameplay and begin distortion ramp.
            mBattleTransitionPhase = BattleTransitionPhase::PINCUSHION;
            if (mTransitionController)
            {
                mTransitionController->StartTransition(mPendingEncounter, mPendingEnemySource);
            }
            TimeSystem::Get().SetSlowMotion(0.25f);

            LOG("[OverworldState] Battle triggered vs '%s' — transition started.",
                mPendingEncounter.name.c_str());
        }
        else
        {
            LOG("[OverworldState] B pressed but no enemy in range (contactRadius check failed).");
        }
    }

    // ---------------------------------------------------------------
    // PINCUSHION phase: transition visual effect update.
    // ---------------------------------------------------------------
    if (mBattleTransitionPhase == BattleTransitionPhase::PINCUSHION)
    {
        // UI clock dt: ignores slow-motion
        const float uiDt = TimeSystem::Get().GetUIClock().GetDeltaTime();

        if (mTransitionController)
        {
            mTransitionController->Update(uiDt, mCamera.get());

            if (mTransitionController->IsFinished())
            {
                // Restore normal speed before handing control to BattleState.
                TimeSystem::Get().SetSlowMotion(1.0f);

                // Push BattleState — OverworldState stays alive underneath the stack.
                StateManager::Get().PushState(
                    std::make_unique<BattleState>(D3DContext::Get(), mPendingEncounter));

                // Reset transition state so OverworldState is ready when it resumes.
                mBattleTransitionPhase = BattleTransitionPhase::IDLE;
                mTransitionController->ClearPending();

                if (mCamera)
                {
                    mCamera->SetZoom(1.0f);
                    mCamera->SetRotation(0.0f);
                }

                LOG("[OverworldState] Transition complete — BattleState pushed, slow-motion restored.");
            }
        }
        else
        {
            // Fallback if no transition controller exists
            TimeSystem::Get().SetSlowMotion(1.0f);
            StateManager::Get().PushState(
                std::make_unique<BattleState>(D3DContext::Get(), mPendingEncounter));
            mBattleTransitionPhase = BattleTransitionPhase::IDLE;
            LOG("[OverworldState] Filter missing — BattleState pushed immediately.");
        }
    }

    // Camera follow — only valid use of mPlayer* here.
    if (mPlayer && mCamera && mBattleTransitionPhase == BattleTransitionPhase::IDLE) {
        mCamera->Follow(mPlayer->GetX(), mPlayer->GetY(), kCameraSmoothing, dt);
        mCamera->Update();
    }
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   1. If pincushion filter is active: redirect scene draws to offscreen RT.
//   2. Draw the static blue circle (world-space SDF, CPU WorldToScreen).
//   3. Call SceneGraph::Render(ctx) — draws all entities in layer order.
//   4. If pincushion filter is active: restore back buffer, apply warp.
//   5. Draw the iris transition overlay on top of everything.
//
// Draw order:
//   [BeginCapture if filter active]
//   Blue circle      → background landmark (world-space)
//   SceneGraph       → ascending layer order (enemies @48, player @50)
//   [EndCapture + Render filter if active — replaces back buffer with warped scene]
//
// No iris in OverworldState — BattleState owns its own iris that opens on entry.
// ------------------------------------------------------------
void OverworldState::Render()
{
    if (!mCamera) return;

    ID3D11DeviceContext* ctx = D3DContext::Get().GetContext();
    const int W = D3DContext::Get().GetWidth();
    const int H = D3DContext::Get().GetHeight();

    // Determine whether the pincushion filter should capture this frame.
    // IsActive() returns false when intensity is below kActivationThreshold,
    // avoiding the overhead of an offscreen RT bind when not needed.
    const bool filterActive = mTransitionController && mTransitionController->IsActive();

    // --- Redirect scene draws to offscreen render target ---
    if (filterActive)
        mTransitionController->BeginCapture(ctx);

    // --- Blue static circle (world-space SDF landmark) ---
    DirectX::XMFLOAT2 blueScreen = mCamera->WorldToScreen(kBlueX, kBlueY);
    const float zoomedBlueRadius = kBlueRadius * mCamera->GetZoom();

    mDebugView.Draw(ctx, W, H, { true });

    mTileMap.Render(ctx, *mCamera);

    mCircleRenderer.Draw(ctx,
        blueScreen.x, blueScreen.y, zoomedBlueRadius,
        0.15f, 0.35f, 1.0f,
        W, H);

    // --- All SceneGraph entities (sorted by layer, self-rendering) ---
    mScene.Render(ctx);

    // --- Restore back buffer and apply pincushion distortion ---
    // EndCapture restores the saved RTV; Render draws the warped scene quad.
    if (filterActive)
    {
        mTransitionController->EndCaptureAndRender(ctx);
    }

}
