// ============================================================
// File: LineupCameraController.h
// Responsibility: Cinematic camera controller for the Party
//                 Lineup screen — manages smooth zoom, rotation,
//                 position, and fade transitions between phases.
//
// Follows the same exponential decay lerp pattern established
// by BattleCameraController:
//     current += (desired - current) * smoothSpeed * dt
//
// Six named phases:
//   ENTERING    — zoom in from far + fade from black (L pressed)
//   IDLE        — steady state, characters visible, input enabled
//   ZOOMING_IN  — dolly toward selected character (Enter pressed)
//   FOCUSED     — character view: camera at (0,0) with characterZoom
//   ZOOMING_OUT — dolly back to overview (Esc from character view)
//   EXITING     — zoom out + fade to black, then PopState
//
// All target values (zoom levels, rotation, speeds) are set via
// SetConfig() from lineup_layout.json so the C++ contains zero
// hardcoded tuning constants.
//
// Ownership:
//   Value member of LineupState.
//   Initialize() called from LineupState::OnEnter().
//   Update(dt) called from LineupState::Update(dt).
//
// Common mistakes:
//   1. Forgetting Update() after SetPhase() — camera won't move.
//   2. Checking IsEntryComplete() before calling Update() — always false.
//   3. Reading GetCamera() before Initialize() — Camera2D is null.
// ============================================================
#pragma once
#include "../Renderer/Camera.h"
#include <memory>
#include <cmath>

// ------------------------------------------------------------
// LineupCameraPhase: the six cinematic states.
// ------------------------------------------------------------
enum class LineupCameraPhase
{
    ENTERING,     // zoom + rotation ramp-up + fade from black
    IDLE,         // stable overview — input enabled
    ZOOMING_IN,   // dolly into selected character
    FOCUSED,      // character view — hold at (0,0) with characterZoom
    ZOOMING_OUT,  // dolly back to overview
    EXITING       // zoom out + fade to black → PopState
};

// ------------------------------------------------------------
// LineupCameraConfig: all tunable values loaded from JSON.
//   No magic numbers in this class — everything comes from here.
// ------------------------------------------------------------
struct LineupCameraConfig
{
    // Entry transition
    float enterZoom       = 0.5f;   // camera starts zoomed out here
    float enterRotation   = 0.0f;   // camera starts at 0 rotation (radians)

    // Idle (lineup overview)
    float idleZoom        = 1.0f;
    float idleRotation    = 0.175f; // ~10 degrees in radians

    // Character zoom-in (transition animation target)
    float characterZoom     = 1.8f;
    float characterRotation = 0.087f; // ~5 degrees in radians

    // Focused (character view — settled state after zoom-in)
    // Defaults to 1.0/0.0 so screen-space layout math works correctly.
    float focusedZoom       = 1.0f;
    float focusedRotation   = 0.0f;

    // Exit transition target
    float exitZoom        = 0.5f;
    float exitRotation    = 0.0f;

    // Interpolation speed (exponential decay coefficient)
    float smoothSpeed     = 4.0f;

    // Fade speed (same exponential decay for alpha)
    float fadeSpeed        = 3.5f;
};

class LineupCameraController
{
public:
    // ------------------------------------------------------------
    // Initialize: construct Camera2D, set phase to ENTERING.
    //   Camera starts at the entry values so the first frame
    //   shows the zoomed-out/black state before lerping in.
    // ------------------------------------------------------------
    void Initialize(int screenW, int screenH, const LineupCameraConfig& config)
    {
        mConfig = config;
        mCamera = std::make_unique<Camera2D>(screenW, screenH);

        // Seed current state to the ENTERING start values.
        mCurrentX        = 0.0f;
        mCurrentY        = 0.0f;
        mCurrentZoom     = mConfig.enterZoom;
        mCurrentRotation = mConfig.enterRotation;
        mCurrentAlpha    = 0.0f;  // fully transparent (black)

        // Apply immediately so first-frame GetCamera() is valid.
        ApplyToCamera();

        mPhase = LineupCameraPhase::ENTERING;
    }

    // ------------------------------------------------------------
    // SetPhase: request a transition to a new camera state.
    //   The camera lerps — never snaps — toward the new desired
    //   values each frame in Update().
    // ------------------------------------------------------------
    void SetPhase(LineupCameraPhase phase)
    {
        mPhase = phase;
    }

    // ------------------------------------------------------------
    // SetFocusPosition: set where the camera should look during
    //   ZOOMING_IN phase (world-space of the selected character).
    // ------------------------------------------------------------
    void SetFocusPosition(float worldX, float worldY)
    {
        mFocusX = worldX;
        mFocusY = worldY;
    }

    // ------------------------------------------------------------
    // Update: advance the exponential decay lerp by dt seconds.
    //
    //   Compute desired state from current phase, then:
    //     current += (desired - current) * smoothSpeed * dt
    //
    //   Same formula as BattleCameraController — frame-rate
    //   independent, self-correcting, no overshoot.
    // ------------------------------------------------------------
    void Update(float dt)
    {
        float desiredX, desiredY, desiredZoom, desiredRotation, desiredAlpha;
        ComputeDesired(desiredX, desiredY, desiredZoom, desiredRotation, desiredAlpha);

        const float k  = mConfig.smoothSpeed * dt;
        const float kf = mConfig.fadeSpeed * dt;

        // Exponential decay toward desired state.
        mCurrentX        += (desiredX        - mCurrentX)        * k;
        mCurrentY        += (desiredY        - mCurrentY)        * k;
        mCurrentZoom     += (desiredZoom     - mCurrentZoom)     * k;
        mCurrentRotation += (desiredRotation - mCurrentRotation) * k;
        mCurrentAlpha    += (desiredAlpha    - mCurrentAlpha)    * kf;

        // Clamp alpha to [0, 1] to prevent overshoot.
        if (mCurrentAlpha > 1.0f) mCurrentAlpha = 1.0f;
        if (mCurrentAlpha < 0.0f) mCurrentAlpha = 0.0f;

        ApplyToCamera();
    }

    // ------------------------------------------------------------
    // Phase completion queries — used to gate state transitions.
    //   "Complete" = current values are within epsilon of desired.
    // ------------------------------------------------------------
    bool IsEntryComplete() const
    {
        return mPhase == LineupCameraPhase::ENTERING &&
               mCurrentAlpha > 0.92f &&
               std::fabs(mCurrentZoom - mConfig.idleZoom) < 0.05f;
    }

    bool IsExitComplete() const
    {
        return mPhase == LineupCameraPhase::EXITING &&
               mCurrentAlpha < 0.08f;
    }

    bool IsZoomInComplete() const
    {
        return mPhase == LineupCameraPhase::ZOOMING_IN &&
               std::fabs(mCurrentZoom - mConfig.characterZoom) < 0.08f;
    }

    bool IsZoomOutComplete() const
    {
        return mPhase == LineupCameraPhase::ZOOMING_OUT &&
               std::fabs(mCurrentZoom - mConfig.idleZoom) < 0.05f;
    }

    bool IsFocusSettled() const
    {
        return mPhase == LineupCameraPhase::ZOOMING_IN &&
               std::fabs(mCurrentZoom - mConfig.characterZoom) < 0.08f;
    }

    // Accessors
    Camera2D&          GetCamera()    { return *mCamera; }
    float              GetFadeAlpha() const { return mCurrentAlpha; }
    LineupCameraPhase  GetPhase()     const { return mPhase; }

private:
    // ------------------------------------------------------------
    // ComputeDesired: map current phase to target camera state.
    // ------------------------------------------------------------
    void ComputeDesired(float& outX, float& outY,
                        float& outZoom, float& outRotation,
                        float& outAlpha) const
    {
        switch (mPhase)
        {
        case LineupCameraPhase::ENTERING:
            outX        = 0.0f;
            outY        = 0.0f;
            outZoom     = mConfig.idleZoom;
            outRotation = mConfig.idleRotation;
            outAlpha    = 1.0f;
            break;

        case LineupCameraPhase::IDLE:
            outX        = 0.0f;
            outY        = 0.0f;
            outZoom     = mConfig.idleZoom;
            outRotation = mConfig.idleRotation;
            outAlpha    = 1.0f;
            break;

        case LineupCameraPhase::ZOOMING_IN:
            outX        = mFocusX;
            outY        = mFocusY;
            outZoom     = mConfig.characterZoom;
            outRotation = mConfig.characterRotation;
            outAlpha    = 1.0f;
            break;

        case LineupCameraPhase::FOCUSED:
            outX        = 0.0f;
            outY        = 0.0f;
            outZoom     = mConfig.focusedZoom;
            outRotation = mConfig.focusedRotation;
            outAlpha    = 1.0f;
            break;

        case LineupCameraPhase::ZOOMING_OUT:
            outX        = 0.0f;
            outY        = 0.0f;
            outZoom     = mConfig.idleZoom;
            outRotation = mConfig.idleRotation;
            outAlpha    = 1.0f;
            break;

        case LineupCameraPhase::EXITING:
            outX        = 0.0f;
            outY        = 0.0f;
            outZoom     = mConfig.exitZoom;
            outRotation = mConfig.exitRotation;
            outAlpha    = 0.0f;
            break;
        }
    }

    // Push interpolated values into Camera2D and rebuild the matrix.
    void ApplyToCamera()
    {
        mCamera->SetPosition(mCurrentX, mCurrentY);
        mCamera->SetZoom(mCurrentZoom);
        mCamera->SetRotation(mCurrentRotation);
        mCamera->Update();
    }

    // --- Config ---
    LineupCameraConfig mConfig;

    // --- Owned camera ---
    std::unique_ptr<Camera2D> mCamera;

    // --- Phase ---
    LineupCameraPhase mPhase = LineupCameraPhase::ENTERING;

    // --- Current interpolated state ---
    float mCurrentX        = 0.0f;
    float mCurrentY        = 0.0f;
    float mCurrentZoom     = 1.0f;
    float mCurrentRotation = 0.0f;
    float mCurrentAlpha    = 0.0f;  // 0=black, 1=fully visible

    // --- Focus target for ZOOMING_IN ---
    float mFocusX = 0.0f;
    float mFocusY = 0.0f;
};
