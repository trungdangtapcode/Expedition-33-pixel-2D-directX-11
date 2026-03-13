// ============================================================
// File: BattleCameraController.h
// Responsibility: Manage the battle scene camera across three cinematic states.
//
// Camera phases:
//   OVERVIEW      — default: fixed wide shot, all combatants visible.
//                   pos=(0,0), zoom=1.0
//
//   ACTOR_FOCUS   — player selected a skill: camera glides and zooms toward
//                   the acting combatant so the player reads its stats / clip.
//                   pos=actorWorldPos, zoom=1.6
//
//   TARGET_FOCUS  — player is choosing a target: camera settles at a weighted
//                   blend: 80% toward the target, 20% toward the actor.
//                   zoom returns to 1.0 so the full enemy formation is visible.
//
// Transitions:
//   All phase changes lerp smoothly using exponential decay:
//     current += (desired - current) * kSmoothSpeed * dt
//   kSmoothSpeed = 5.0f  →  ~95% closure in ~0.6 s at 60 fps.
//   This is frame-rate-independent because the coefficient is multiplied by dt.
//
// Coordinate convention:
//   All positions are WORLD SPACE (screen pixel - screenW/2, screenH/2).
//   BattleRenderer slot tables store screen pixels; callers convert before
//   passing to SetActorPos / SetTargetPos.
//
// Ownership:
//   Owned by BattleRenderer (value member).
//   BattleState calls BattleRenderer::SetCameraPhase() to drive transitions.
//
// Lifetime:
//   Initialize() called from BattleRenderer::Initialize().
//   Update(dt) called from BattleRenderer::Update(dt).
//   The Camera2D reference is retrieved via GetCamera() for Draw() calls.
//
// Common mistakes:
//   1. Passing screen pixels as world coords — sprite appears far off-screen.
//      Convert: worldX = screenX - screenW/2, worldY = screenY - screenH/2.
//   2. Forgetting Update() after SetPhase() — camera snaps instead of lerping.
//   3. Calling GetCamera() before Initialize() — Camera2D is unbuilt.
// ============================================================
#pragma once
#include "../Renderer/Camera.h"
#include <memory>

// ------------------------------------------------------------
// BattleCameraPhase: the three named cinematic states.
// ------------------------------------------------------------
enum class BattleCameraPhase
{
    OVERVIEW,       // wide: pos=(0,0) zoom=1.0 — whole field visible
    ACTOR_FOCUS,    // close-up on the acting combatant
    TARGET_FOCUS,   // blend toward the target while keeping the actor visible
};

class BattleCameraController
{
public:
    BattleCameraController() = default;

    // ------------------------------------------------------------
    // Initialize: build the Camera2D for the given render-target size.
    //   Sets phase to OVERVIEW so the first frame is already correct.
    // ------------------------------------------------------------
    void Initialize(int screenW, int screenH);

    // ------------------------------------------------------------
    // SetActorPos: record the world-space position of the ACTING combatant.
    //   Must be called before SetPhase(ACTOR_FOCUS) or SetPhase(TARGET_FOCUS).
    //   Values persist until overwritten — callers only need to update them
    //   when the active combatant changes.
    // ------------------------------------------------------------
    void SetActorPos(float worldX, float worldY);

    // ------------------------------------------------------------
    // SetTargetPos: record the world-space position of the TARGET combatant.
    //   Must be called before SetPhase(TARGET_FOCUS).
    // ------------------------------------------------------------
    void SetTargetPos(float worldX, float worldY);

    // ------------------------------------------------------------
    // SetPhase: request a transition to a new camera state.
    //   The camera does not snap — it lerps toward the new desired
    //   position/zoom each frame in Update().
    // ------------------------------------------------------------
    void SetPhase(BattleCameraPhase phase);

    // ------------------------------------------------------------
    // Update: advance the lerp by dt seconds.
    //   Must be called once per frame BEFORE BattleRenderer::Render().
    //   Calls Camera2D::Update() internally so the matrix is always fresh.
    // ------------------------------------------------------------
    void Update(float dt);

    // ------------------------------------------------------------
    // GetCamera: return the Camera2D so BattleRenderer::Render() can pass
    //   it to WorldSpriteRenderer::Draw().
    // ------------------------------------------------------------
    Camera2D& GetCamera() { return *mCamera; }

    BattleCameraPhase GetPhase() const { return mPhase; }

private:
    // Smooth approach speed — 5.0f closes ~95% of the gap in ~0.6 s.
    static constexpr float kSmoothSpeed   = 5.0f;

    // Zoom levels for each phase.
    static constexpr float kZoomOverview  = 1.0f;
    static constexpr float kZoomFocus     = 1.6f;   // actor focus: close-up
    static constexpr float kZoomTarget    = 1.0f;   // target select: wide again

    // Camera rotation for each phase (radians). Positive = counter-clockwise.
    static constexpr float kRotationOverview = 0.0f;
    static constexpr float kRotationFocus    = 10.0f * (3.14159265f / 180.0f);
    static constexpr float kRotationTarget   = 0.0f;

    // Blend weight: how much the camera leans toward the TARGET in TARGET_FOCUS.
    // 0.8 = 80% target, 20% actor — target is dominant but actor stays on-screen.
    static constexpr float kTargetBlend   = 0.8f;

    // ------------------------------------------------------------
    // ComputeDesired: calculate the (posX, posY, zoom, rotation) that the current
    //   phase demands.  Called each frame in Update() to drive the lerp.
    // ------------------------------------------------------------
    void ComputeDesired(float& outX, float& outY, float& outZoom, float& outRotation) const;

    std::unique_ptr<Camera2D> mCamera;

    BattleCameraPhase mPhase = BattleCameraPhase::OVERVIEW;

    // Current interpolated camera state (what we actually set on the camera).
    float mCurrentX    = 0.0f;
    float mCurrentY    = 0.0f;
    float mCurrentZoom = 1.0f;
    float mCurrentRotation = 0.0f;


    // Stored world positions for ACTOR_FOCUS and TARGET_FOCUS computations.
    float mActorX  = 0.0f;
    float mActorY  = 0.0f;
    float mTargetX = 0.0f;
    float mTargetY = 0.0f;
};
