#pragma once
#include "IGameState.h"
#include "../Renderer/CircleRenderer.h"
#include "../Renderer/Camera.h"
#include "../Scene/SceneGraph.h"
#include "../Entities/ControllableCharacter.h"
#include "../Debug/DebugTextureViewer.h"
#include <memory>

// ============================================================
// File: PlayState.h
// Responsibility: Camera-follow gameplay state.
//
// Scene:
//   - Blue circle (static NPC)     — fixed world position, never moves.
//   - Orange circle (reference)    — REMOVED; replaced by ControllableCharacter.
//   - ControllableCharacter        — WASD-controlled Verso sprite (via SceneGraph).
//   - Camera2D                     — follows the player character with smooth lerp.
//
// Architecture:
//   PlayState does NOT know how the player moves, how many draw calls it needs,
//   or what texture it uses.  It only knows IGameObject* (via SceneGraph).
//   ControllableCharacter* mPlayer is kept solely to call GetX()/GetY()
//   for camera follow — that is the entire extent of this class's player knowledge.
//
// World vs Screen:
//   Circles live in WORLD coordinates; Camera2D::WorldToScreen() converts them
//   each frame for CircleRenderer (SDF shader, no GPU VP matrix path).
// ============================================================
class PlayState : public IGameState {
public:
    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "PlayState"; }

private:
    // ---------------------------------------------------------------
    // SDF circle renderer — still used for the static blue landmark.
    // ---------------------------------------------------------------
    CircleRenderer mCircleRenderer;

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
    // PlayState calls only Update(dt) and Render(ctx) on it; it has no
    // knowledge of what entities live inside.
    // ---------------------------------------------------------------
    SceneGraph mScene;

    // Non-owning observer pointer to the spawned player character.
    // Used ONLY for GetX()/GetY() to drive camera follow.
    // SceneGraph retains sole ownership via unique_ptr.
    ControllableCharacter* mPlayer = nullptr;

    // DEBUG: raw texture viewer — bypasses all sprite sheet / pivot math.
    DebugTextureViewer mDebugView;

    // ListenerID for "window_resized" — stored so we can Unsubscribe in OnExit.
    int mResizeListenerID = -1;
};