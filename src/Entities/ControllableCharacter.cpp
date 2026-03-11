// ============================================================
// File: ControllableCharacter.cpp
// Responsibility: Implement player-controlled character logic.
//
// All rendering is delegated to WorldSpriteRenderer (composition).
// All movement physics (velocity, friction, speed cap) live here.
// PlayState only knows this object as IGameObject*.
//
// Owns: WorldSpriteRenderer mRenderer — released in destructor.
//
// Lifetime:
//   Created in  → SceneGraph::Spawn<ControllableCharacter>(...)
//   Destroyed in → SceneGraph::Clear() via unique_ptr destructor
//
// Common mistakes:
//   1. Calling mRenderer.Draw() from outside this class — Render() is the
//      only public surface.  All sprite work stays private.
//   2. Using frame count instead of dt for animation / physics — only dt
//      is frame-rate independent.
// ============================================================
#include "ControllableCharacter.h"
#include "../Renderer/D3DContext.h"
#include "../Utils/Log.h"
#include <windows.h>   // GetAsyncKeyState, VK_* constants
#include <cmath>       // std::sqrt

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
ControllableCharacter::ControllableCharacter(
    ID3D11Device*        device,
    ID3D11DeviceContext* context,
    const std::wstring&  texturePath,
    const SpriteSheet&   sheet,
    const std::string&   startClip,
    float                startX,
    float                startY,
    Camera2D*            camera)
    : mCamera(camera)
    , mPosX(startX)
    , mPosY(startY)
{
    // Initialize the rendering component — loads texture, builds SpriteBatch
    // and D3D states.  If this fails, mReady stays false and the character
    // is a no-op for Update/Render until the issue is fixed.
    mReady = mRenderer.Initialize(device, context, texturePath, sheet);
    if (!mReady) {
        LOG("[ControllableCharacter] ERROR — WorldSpriteRenderer::Initialize failed.");
        return;
    }

    // Start the first clip immediately.  If the clip name is not found,
    // PlayClip() logs a warning and the renderer stays in its default state
    // (mActiveClip = nullptr), so Draw() will no-op silently.
    mRenderer.PlayClip(startClip);
}

// ------------------------------------------------------------
// Destructor
// ------------------------------------------------------------
ControllableCharacter::~ControllableCharacter()
{
    // Explicitly shut down GPU resources before the unique_ptr is destroyed.
    // SceneGraph::Clear() destroys objects before D3DContext tears down,
    // so the device is still valid here.
    if (mReady) {
        mRenderer.Shutdown();
    }
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   1. Poll WASD — apply acceleration (world units/s^2).
//   2. Apply friction — exponential velocity decay toward zero.
//   3. Clamp speed to kMaxSpeed.
//   4. Integrate position.
//   5. Update facing direction from horizontal velocity.
//   6. Switch between "idle" and "walk" clips based on whether the
//      character is moving.
//   7. Advance animation frame timer.
//
// All values are scaled by dt — fully frame-rate independent.
// ------------------------------------------------------------
void ControllableCharacter::Update(float dt)
{
    if (!mReady) return;

    // --- WASD acceleration ---
    // +Y is downward in screen/world space; W (up on screen) decreases Y.
    if (GetAsyncKeyState('W') & 0x8000) mVelY -= kAccel * dt;
    if (GetAsyncKeyState('S') & 0x8000) mVelY += kAccel * dt;
    if (GetAsyncKeyState('A') & 0x8000) mVelX -= kAccel * dt;
    if (GetAsyncKeyState('D') & 0x8000) mVelX += kAccel * dt;

    // --- Friction: exponential decay — frame-rate independent for small dt ---
    // Factor (1 - kFriction * dt) approaches 0 as kFriction grows.
    // Velocity halves in roughly ln(2)/kFriction seconds ≈ 0.087 s at kFriction=8.
    const float friction = 1.0f - kFriction * dt;
    mVelX *= friction;
    mVelY *= friction;

    // --- Speed cap: prevent diagonal movement from exceeding kMaxSpeed ---
    // Without this cap, holding W+D produces speed * sqrt(2) ≈ 1.41x.
    const float speed = std::sqrt(mVelX * mVelX + mVelY * mVelY);
    if (speed > kMaxSpeed) {
        const float inv = kMaxSpeed / speed;
        mVelX *= inv;
        mVelY *= inv;
    }

    // --- Integrate position ---
    mPosX += mVelX * dt;
    mPosY += mVelY * dt;

    // --- Update facing direction from horizontal velocity ---
    // Only update facing when there is meaningful horizontal movement to avoid
    // flickering when the character coasts to a stop on a diagonal.
    // kFacingThreshold is well below kMaxSpeed so the flip happens the moment
    // a key is pressed, not after physics builds up speed.
    if (mVelX < -kFacingThreshold)      mFacingLeft = true;
    else if (mVelX > kFacingThreshold)  mFacingLeft = false;
    // No else: facing holds its last value when moving purely vertically.

    // --- Switch animation clip based on movement ---
    // The character is "moving" if its speed exceeds a small threshold.
    // This prevents the walk animation from playing at near-zero drift
    // after releasing a key while friction coasts the character to rest.
    const bool moving = (speed > kMoveThreshold);
    mRenderer.PlayClip(moving ? "walk" : "idle");

    // --- Advance animation ---
    // WorldSpriteRenderer::Update() advances the frame timer by dt seconds.
    // The renderer handles frame wrap-around and loop/non-loop logic internally.
    mRenderer.Update(dt);
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Rebind the RTV (required after CircleRenderer resets pipeline state),
//   then delegate the entire draw call to WorldSpriteRenderer::Draw().
//   PlayState never calls this directly — SceneGraph::Render() does.
// ------------------------------------------------------------
void ControllableCharacter::Render(ID3D11DeviceContext* ctx)
{
    if (!mReady || !mCamera) return;

    // Rebind the render target before SpriteBatch::Begin().
    // CircleRenderer's SDF pixel shader resets OMSetRenderTargets as a
    // side effect of its state cleanup; if we skip this rebind, SpriteBatch
    // draws to an unbound target and produces no visible output.
    ID3D11RenderTargetView* rtv = D3DContext::Get().GetRTV();
    ID3D11DepthStencilView* dsv = D3DContext::Get().GetDSV();
    ctx->OMSetRenderTargets(1, &rtv, dsv);

    // Delegate completely — this class has no knowledge of UV slicing,
    // pivot math, SpriteBatch modes, or D3D state objects.
    // mFacingLeft drives the horizontal flip: the sprite sheet faces right
    // by default; flipping mirrors it for left-facing movement.
    mRenderer.Draw(ctx, *mCamera, mPosX, mPosY, 1.0f, mFacingLeft);
}
