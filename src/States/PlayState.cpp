// ============================================================
// File: PlayState.cpp
// Responsibility: Camera-follow gameplay state — overworld exploration.
//
// Architecture:
//   PlayState knows NOTHING about how ControllableCharacter moves or renders.
//   It spawns the character into SceneGraph, then calls only:
//     mScene.Update(dt)   — drives all entity logic
//     mScene.Render(ctx)  — drives all entity drawing
//   Camera follow is the only reason PlayState holds a ControllableCharacter*,
//   and only the narrow GetX()/GetY() interface is used.
//
//   OverworldEnemy entities are spawned the same way.  PlayState only holds
//   non-owning pointers to call IsPlayerNearby() and GetEncounterData().
//
// Battle trigger:
//   B pressed while near an enemy → push BattleState(encounter) immediately.
//   BattleState::OnEnter() starts its own iris at radius=0 then opens outward,
//   producing the classic circle-wipe reveal.
//   BattleState pops → PlayState resumes with its own iris already IDLE (open).
//
// Scene:
//   Blue circle              — static world-space landmark, CircleRenderer SDF.
//   ControllableCharacter    — Verso sprite, WASD controlled, SceneGraph-owned.
//   OverworldEnemy (1..N)    — stationary enemies, SceneGraph-owned.
//   Camera2D                 — follows ControllableCharacter with smooth lerp.
//   IrisTransitionRenderer   — fullscreen iris overlay for battle transitions.
//
// Input:
//   W / A / S / D — move the Verso character
//   ESC           — return to MenuState
//   B (near enemy) — trigger battle transition
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
//   4. Spawn ControllableCharacter into SceneGraph.
//   5. Spawn OverworldEnemy entities from data/enemies/*.json.
//   6. Initialize IrisTransitionRenderer and start opening (fade-in).
//   7. Subscribe to window_resized to keep Camera2D in sync.
// ------------------------------------------------------------
void PlayState::OnEnter()
{
    LOG("[PlayState] OnEnter");

    EventManager::Get().Broadcast("bgm_play_overworld", {});
    ID3D11Device*        device  = D3DContext::Get().GetDevice();
    ID3D11DeviceContext* context = D3DContext::Get().GetContext();
    const int W = D3DContext::Get().GetWidth();
    const int H = D3DContext::Get().GetHeight();

    // --- CircleRenderer — still used for the blue static landmark ---
    if (!mCircleRenderer.Initialize(device)) {
        LOG("[PlayState] ERROR — CircleRenderer initialization failed.");
    }

    // --- Camera ---
    mCamera = std::make_unique<Camera2D>(W, H);

    // --- Load Verso sprite sheet ---
    SpriteSheet sheet;
    if (!JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet)) {
        LOG("[PlayState] ERROR — Failed to load verso.json.");
        return;
    }

    // --- Spawn player character ---
    mPlayer = mScene.Spawn<ControllableCharacter>(
        device, context,
        L"assets/animations/verso.png",
        sheet,
        std::string("idle"),
        0.0f, 0.0f,
        mCamera.get()
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
            LOG("[PlayState] WARNING — Could not load skeleton.json; solo enemy not spawned.");
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
            LOG("[PlayState] WARNING — Could not load skeleton_group.json; group enemy not spawned.");
        }
    }

    // --- Iris transition: start fully closed, open to reveal the overworld ---
    // Initialize uses the current screen dimensions to set mMaxRadius and mCenterX/Y.
    if (mIris.Initialize(device, W, H))
    {
        // mRadius starts at 0 (fully black) then grows to mMaxRadius (fully open).
        mIris.StartOpen(800.0f);
    }
    else
    {
        LOG("[PlayState] WARNING — IrisTransitionRenderer init failed; transitions disabled.");
    }

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

    // Subscribe to "battle_end_victory" to remove the defeated overworld enemy.
    // This fires inside BattleState::Update() (deferred-exit block) while PlayState
    // is still alive underneath the stack.  It is safe to access mOverworldEnemies
    // because PlayState has NOT been destroyed yet.
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

                LOG("[PlayState] battle_end_victory -> enemy defeated and removed from overworld.");
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
void PlayState::OnExit()
{
    LOG("[PlayState] OnExit");

    // Stop BGM when PlayState is fully dismissed (e.g., transitioning to MenuState).
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
    mDebugView.Shutdown();

    // Release iris overlay GPU resources.
    mIris.Shutdown();

    // Clear non-owning pointers BEFORE SceneGraph::Clear() frees the entities.
    // Accessing these pointers after Clear() is a use-after-free.
    mPlayer = nullptr;
    mOverworldEnemies.clear();

    // Destroy all SceneGraph entities (ControllableCharacter, OverworldEnemy, etc.).
    mScene.Clear();

    mCamera.reset();
    mBWasDown = false;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   1. Handle ESC → transition to MenuState.
//   2. Advance the iris transition (opens on enter, closes on battle trigger).
//   3. Delegate all entity logic to SceneGraph::Update(dt).
//   4. Check proximity to overworld enemies; if B pressed near one → trigger battle.
//   5. Camera follow via the narrow GetX()/GetY() interface.
//
// Battle trigger sequence:
//   - B key pressed AND player is near an enemy AND iris is fully open (IDLE):
//       Copy encounter data from the nearby OverworldEnemy.
//       Push BattleState(encounter) immediately — no close animation needed.
//         BattleState::Initialize() sets iris radius=0 (black).
//         BattleState::OnEnter() calls StartOpen() → circle expands to reveal battle.
//
// Why defer the push to the iris callback?
//   The iris must be fully black before the state switches so the player
//   cannot see the scene pop.  Calling PushState() before the iris closes
//   would show a jarring one-frame flash of the battle scene.
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

    // Advance iris animation (OPENING → IDLE, or CLOSING → CLOSED → callback).
    // Must update before checking IsIdle() so the flag reflects this frame's state.
    mIris.Update(dt);

    // All entity logic (WASD, physics, animation, enemy idle) runs here.
    mScene.Update(dt);

    // ---------------------------------------------------------------
    // Battle trigger: B key + enemy proximity + iris not transitioning.
    // The iris guard prevents double-triggering if B is held across frames.
    // ---------------------------------------------------------------
    const bool bDown = (GetAsyncKeyState('B') & 0x8000) != 0;
    const bool bPressed = bDown && !mBWasDown;
    mBWasDown = bDown;

    if (bPressed && mIris.IsIdle() && mPlayer)
    {
        const float px = mPlayer->GetX();
        const float py = mPlayer->GetY();

        // Find the closest enemy within contact radius.
        // First match wins — ties are resolved by vector order (spawn order).
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
            // Copy the encounter data from the triggered enemy.
            mPendingEncounter = target->GetEncounterData();

            // Store a reference to the triggering overworld enemy so the
            // "battle_end_victory" listener can call MarkDefeated() on the
            // correct instance when the battle ends.
            mPendingEnemySource = target;

            LOG("[PlayState] Battle triggered vs '%s' — pushing BattleState.",
                mPendingEncounter.name.c_str());

            // Push BattleState immediately — no iris close needed here.
            // BattleState::Initialize() starts the iris at radius=0 (black),
            // then StartOpen() expands the circle outward to reveal the battle
            // scene (the classic JRPG circle-wipe OUT reveal).
            StateManager::Get().PushState(
                std::make_unique<BattleState>(D3DContext::Get(), mPendingEncounter));
        }
        else
        {
            // No enemy nearby — log a hint so the developer can tune contactRadius.
            LOG("[PlayState] B pressed but no enemy in range (contactRadius check failed).");
        }
    }

    // Camera follow — only valid use of mPlayer* here.
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
//   3. Draw the iris transition overlay on top of everything.
//
// Draw order (front-to-back in reverse — last call = topmost):
//   Blue circle      → drawn first (background landmark)
//   SceneGraph       → ascending layer order (enemies @48, player @50)
//   IrisTransition   → drawn last to overlay the full scene
// ------------------------------------------------------------
void PlayState::Render()
{
    if (!mCamera) return;

    ID3D11DeviceContext* ctx = D3DContext::Get().GetContext();
    const int W = D3DContext::Get().GetWidth();
    const int H = D3DContext::Get().GetHeight();

    // --- Blue static circle (world-space SDF landmark) ---
    DirectX::XMFLOAT2 blueScreen = mCamera->WorldToScreen(kBlueX, kBlueY);
    const float zoomedBlueRadius = kBlueRadius * mCamera->GetZoom();

    mDebugView.Draw(ctx, W, H, { true });

    mCircleRenderer.Draw(ctx,
        blueScreen.x, blueScreen.y, zoomedBlueRadius,
        0.15f, 0.35f, 1.0f,
        W, H);

    // --- All SceneGraph entities (sorted by layer, self-rendering) ---
    mScene.Render(ctx);

    // --- Iris transition overlay (fullscreen black with circular hole) ---
    // Drawn LAST so it composites over all game content.
    // No-op when IrisPhase == IDLE (iris fully open — nothing to draw).
    mIris.Render(ctx);
}
