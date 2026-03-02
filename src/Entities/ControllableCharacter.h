// ============================================================
// File: ControllableCharacter.h
// Responsibility: A player-controlled character entity in world space.
//
// Implements IGameObject so SceneGraph drives it with no knowledge of
// what it is.  Internally owns a WorldSpriteRenderer — the caller never
// sees it, never configures it, and never calls Draw() on it directly.
//
// Design decisions:
//   - Input (WASD) is read inside Update() — the character is self-contained.
//     PlayState does not poll keys, does not hold velocity, and does not call
//     any draw method.  This is the Open/Closed Principle: adding a second
//     character only requires a second Spawn<ControllableCharacter>() call.
//   - Physics (velocity + friction + speed cap) lives here, not in PlayState.
//   - WorldSpriteRenderer is a member (composition), not a base class
//     (inheritance).  It is a rendering component with a narrow interface;
//     the character delegates to it, never re-implements it.
//   - All movement constants are constexpr at file scope — no magic numbers.
//   - GetPosition() exposes world position for camera-follow without
//     exposing internal mVelX/Y or any rendering state.
//
// Owns:
//   WorldSpriteRenderer  mRenderer  — GPU texture + SpriteBatch + D3D states
//
// Lifetime:
//   Created in  → SceneGraph::Spawn<ControllableCharacter>(...)  (PlayState::OnEnter)
//   Destroyed in → SceneGraph::Clear()  (PlayState::OnExit)
//                  WorldSpriteRenderer::Shutdown() is called in the destructor.
//
// Common mistakes:
//   1. Accessing character->mRenderer from PlayState — renderer is private;
//      all rendering is handled inside Render().
//   2. Calling character->Update(dt) manually — SceneGraph::Update() does this
//      for all objects; double-calling would double-advance physics.
//   3. Forgetting to rebind the RTV before SpriteBatch::Begin() when another
//      renderer clears pipeline state — WorldSpriteRenderer::Draw() does this.
// ============================================================
#pragma once
#include "../Scene/IGameObject.h"
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Renderer/Camera.h"
#include "../Renderer/SpriteSheet.h"
#include <d3d11.h>
#include <string>

// ============================================================
// Class: ControllableCharacter
// ============================================================
class ControllableCharacter : public IGameObject
{
public:
    // ------------------------------------------------------------
    // Constructor
    // Purpose:
    //   Initialize the character at a given world position.
    //   GPU resources (texture, SpriteBatch, states) are created here via
    //   WorldSpriteRenderer::Initialize().
    //
    // Parameters:
    //   device      — D3D11 device, used once for texture + state creation
    //   context     — D3D11 context, stored by SpriteBatch for Begin/End
    //   texturePath — path to the sprite-sheet PNG
    //   sheet       — SpriteSheet descriptor loaded from JSON
    //   startClip   — name of the first animation clip to play (e.g. "idle")
    //   startX/Y    — initial world-space position (pixels at zoom=1)
    //   camera      — non-owning pointer; used each frame in Render()
    //                 Caller must ensure the camera outlives this object.
    // ------------------------------------------------------------
    ControllableCharacter(ID3D11Device*        device,
                          ID3D11DeviceContext*  context,
                          const std::wstring&   texturePath,
                          const SpriteSheet&    sheet,
                          const std::string&    startClip,
                          float                 startX,
                          float                 startY,
                          Camera2D*             camera);

    // Destructor: releases GPU resources via WorldSpriteRenderer::Shutdown().
    ~ControllableCharacter() override;

    // ------------------------------------------------------------
    // IGameObject interface
    // ------------------------------------------------------------

    // Reads WASD input, applies acceleration + friction, integrates position.
    // Advances the animation frame timer.  All in delta-time scaled math.
    void Update(float dt) override;

    // Rebinds the RTV (safety guard after CircleRenderer state cleanup),
    // then delegates entirely to WorldSpriteRenderer::Draw().
    // PlayState never calls Draw() directly — it calls SceneGraph::Render().
    void Render(ID3D11DeviceContext* ctx) override;

    // World characters render at layer 50 — above background, below UI.
    int GetLayer() const override { return 50; }

    // Characters are always alive for now; extend for HP-based death later.
    bool IsAlive() const override { return mAlive; }

    // ------------------------------------------------------------
    // Narrow public interface — only what callers actually need.
    // ------------------------------------------------------------

    // Returns world-space position for camera-follow.
    // PlayState passes this to Camera2D::Follow() without knowing
    // anything else about the character.
    float GetX() const { return mPosX; }
    float GetY() const { return mPosY; }

    // Kill this character (sets IsAlive()=false; SceneGraph purges next frame).
    void Kill() { mAlive = false; }

private:
    // ------------------------------------------------------------
    // Rendering component — composition over inheritance.
    // ControllableCharacter delegates all GPU work to this; it never
    // re-implements texture loading, SpriteBatch management, or UV slicing.
    // ------------------------------------------------------------
    WorldSpriteRenderer mRenderer;

    // Non-owning observer pointer — PlayState (or SceneGraph owner) is
    // responsible for the camera lifetime.  This object never deletes it.
    Camera2D* mCamera = nullptr;

    // World-space position.  Exposed read-only via GetX()/GetY().
    float mPosX = 0.0f;
    float mPosY = 0.0f;

    // Velocity in world units per second.
    float mVelX = 0.0f;
    float mVelY = 0.0f;

    // Whether this entity is still alive in the scene.
    bool mAlive = true;

    // Whether renderer was successfully initialized.
    // If false, Render() is a no-op and Update() skips physics.
    bool mReady = false;

    // ------------------------------------------------------------
    // Movement tuning — constexpr so the compiler enforces no magic numbers.
    // All units are world pixels/second or world pixels/second^2.
    // ------------------------------------------------------------

    // Top speed the player can reach by holding a key.
    static constexpr float kMaxSpeed = 400.0f;

    // How quickly the player accelerates from rest to full speed.
    static constexpr float kAccel = 600.0f;

    // Friction coefficient: velocity *= (1 - kFriction * dt) each frame.
    // At 60 fps (dt≈0.016): decay factor ≈ 0.87 — snappy slide-to-stop.
    static constexpr float kFriction = 8.0f;
};
