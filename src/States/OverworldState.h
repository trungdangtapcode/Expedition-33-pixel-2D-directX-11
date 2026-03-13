#pragma once
#include "IGameState.h"
#include "../Renderer/CircleRenderer.h"
#include "../Renderer/IScreenFilter.h"
#include "../Renderer/Camera.h"
#include "../Scene/SceneGraph.h"
#include "../Entities/ControllableCharacter.h"
#include "../Entities/OverworldEnemy.h"
#include "../Battle/EnemyEncounterData.h"
#include "../Debug/DebugTextureViewer.h"
#include <memory>
#include <vector>

// ============================================================
// File: OverworldState.h
// Responsibility: Camera-follow gameplay state — overworld exploration.
//
// Scene:
//   - Blue circle (static NPC)     — fixed world position, never moves.
//   - ControllableCharacter        — WASD-controlled Verso sprite (via SceneGraph).
//   - OverworldEnemy (1..N)        — stationary enemies with encounter data.
//   - Camera2D                     — follows the player character with smooth lerp.
//
// Architecture:
//   OverworldState does NOT know how the player moves, how many draw calls it needs,
//   or what texture it uses.  It only knows IGameObject* (via SceneGraph).
//   ControllableCharacter* mPlayer is kept solely to call GetX()/GetY()
//   for camera follow — that is the entire extent of this class's player knowledge.
//
//   OverworldEnemy* entries in mOverworldEnemies are non-owning; SceneGraph
//   holds sole ownership.  OverworldState uses them ONLY to check:
//     - IsPlayerNearby(px, py)    — proximity for "press B" prompt
//     - GetEncounterData()        — data package to hand to BattleState
//
// Battle trigger flow (2-phase, no iris in overworld):
//   1. PINCUSHION  — B pressed + enemy nearby:
//                    pincushion distortion ramps up over kPincushionDuration seconds
//                    using UI-clock dt (unaffected by slow-motion).
//                    TimeSystem::SetSlowMotion(0.25) slows gameplay simultaneously.
//   2. Push (IDLE) — intensity reached 1.0:
//                    slow-motion reset, BattleState pushed immediately onto the stack.
//
//   BattleState::OnEnter() starts its own iris at radius=0 then opens outward,
//   producing the classic circle-wipe reveal on the battle side.
//   BattleState pops → OverworldState resumes normally (no iris to manage).
//
// World vs Screen:
//   Circles live in WORLD coordinates; Camera2D::WorldToScreen() converts them
//   each frame for CircleRenderer (SDF shader, no GPU VP matrix path).
// ============================================================

// Battle transition phase — controls the two-step trigger sequence.
// Declared outside the class so BattleState or other states never need to
// include OverworldState.h to read this enum; OverworldState is the sole owner.
enum class BattleTransitionPhase {
    IDLE,         // no transition active; overworld runs normally
    PINCUSHION    // pincushion distortion ramping up + slow-motion active
};

class OverworldState : public IGameState {
public:
    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "OverworldState"; }

private:
    // ---------------------------------------------------------------
    // SDF circle renderer — still used for the static blue landmark.
    // ---------------------------------------------------------------
    CircleRenderer mCircleRenderer;

    // ---------------------------------------------------------------
    // Pincushion distortion filter — fullscreen post-process applied to the
    // captured scene during the PINCUSHION transition phase.
    // Interface pointer (IScreenFilter*) — caller code never binds to the
    // concrete type; the filter is swappable without changing any other code.
    // Created in OnEnter, destroyed in OnExit.
    // ---------------------------------------------------------------
    std::unique_ptr<IScreenFilter> mPincushionFilter;

    // ---------------------------------------------------------------
    // Battle transition phase state machine (IDLE → PINCUSHION → IDLE).
    // ---------------------------------------------------------------
    BattleTransitionPhase mBattleTransitionPhase = BattleTransitionPhase::IDLE;

    // Accumulates UI-clock dt during PINCUSHION phase.
    // Uses UI clock (not gameplay clock) so the timer is wall-clock accurate
    // even when TimeSystem::SetSlowMotion() reduces the gameplay clock rate.
    float mPincushionTimer = 0.0f;

    // Duration in seconds to ramp pincushion from intensity 0 → 1.
    static constexpr float kPincushionDuration = 1.0f;

    // ---------------------------------------------------------------
    // Static blue circle — fixed world position, never updated.
    // Kept as a visual landmark to demonstrate world-space rendering.
    // ---------------------------------------------------------------
    static constexpr float kBlueX      = 400.0f;
    static constexpr float kBlueY      = 200.0f;
    static constexpr float kBlueRadius = 35.0f;

    // ---------------------------------------------------------------
    // Camera — follows the player character with smooth lerp.
    // std::unique_ptr defers construction until screen dimensions are known.
    // ---------------------------------------------------------------
    std::unique_ptr<Camera2D> mCamera;

    // Camera follow smoothing: higher = snappier (5 = fast, 2 = cinematic).
    static constexpr float kCameraSmoothing = 5.0f;

    // ---------------------------------------------------------------
    // SceneGraph — owns all IGameObject instances.
    // OverworldState calls only Update(dt) and Render(ctx) on it; it has no
    // knowledge of what entities live inside.
    // ---------------------------------------------------------------
    SceneGraph mScene;

    // Non-owning observer pointer to the spawned player character.
    // Used ONLY for GetX()/GetY() to drive camera follow.
    // SceneGraph retains sole ownership via unique_ptr.
    ControllableCharacter* mPlayer = nullptr;

    // ---------------------------------------------------------------
    // Overworld enemies — non-owning observer pointers.
    // SceneGraph owns the actual entities via unique_ptr.
    // Used by Update() for proximity checks and GetEncounterData().
    // Cleared in OnExit() BEFORE SceneGraph::Clear() frees the entities.
    // ---------------------------------------------------------------
    std::vector<OverworldEnemy*> mOverworldEnemies;

    // Encounter data copied from the nearby enemy when B is pressed.
    // Passed to BattleState constructor after the iris closes.
    EnemyEncounterData mPendingEncounter;

    // Non-owning pointer to the overworld enemy that triggered the last battle.
    // Set on B-press, cleared after victory callback fires (or on OnExit).
    // Used to call MarkDefeated() so the enemy disappears after a win.
    OverworldEnemy* mPendingEnemySource = nullptr;

    // One-press B key tracking — member variable (no static local) for clean
    // lifecycle management (reset to false in OnExit via destruction).
    bool mBWasDown = false;

    // DEBUG: raw texture viewer — bypasses all sprite sheet / pivot math.
    DebugTextureViewer mDebugView;

    // ListenerID for "window_resized" — stored so we can Unsubscribe in OnExit.
    int mResizeListenerID = -1;

    // ListenerID for "battle_end_victory" — marks the source overworld enemy
    // as defeated so SceneGraph::PurgeDead() removes it on the next frame.
    int mVictoryListenerID = -1;
};
