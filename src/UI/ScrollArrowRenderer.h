// ============================================================
// File: ScrollArrowRenderer.h
// Responsibility: Draw a single scroll-direction arrow sprite with a
//                 looping vertical bob animation, optionally flipped
//                 vertically so one texture covers both up and down
//                 chevrons.
//
// Why a dedicated class instead of reusing PointerRenderer:
//   PointerRenderer is wired to the existing target-cursor pipeline
//   (driven by enemy slot positions, hardcoded y_offset of -260.0,
//   no flip support).  The scroll chevron has different needs:
//     - tiny size (icon-sized, not target-marker-sized)
//     - vertical flip for the "up" instance reusing one PNG
//     - bob direction tied to the chevron's pointing direction so the
//       motion feels like the arrow leaning into the off-screen items
//     - no JSON-driven y_offset; the caller decides the position
//   Forking PointerRenderer would have required adding flip + motion
//   inversion to a class that other callers depend on.  A focused
//   30-line renderer is the cheaper change.
//
// Animation:
//   The arrow oscillates vertically by `bobAmplitude` pixels at
//   `bobSpeed` rad/s.  When `flipVertical == true` the bob direction
//   inverts so the arrow always leans AWAY from the menu and TOWARD
//   the off-screen items it represents.
//
// Lifetime:
//   Created in  -> BattleState::OnEnter (one instance for up, one for down)
//   Destroyed in -> BattleState destructor (ComPtr / unique_ptr auto-release)
//
// Common mistakes:
//   1. Forgetting to call Update(dt) every frame  -> arrow does not bob.
//   2. Calling Draw before Initialize             -> silent no-op (guarded).
//   3. Passing a world matrix when the menu is in screen space (or vice
//      versa) -> sprite drawn at the wrong coordinates; SpriteBatch
//      will not warn.
// ============================================================
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <string>
#include <memory>

class ScrollArrowRenderer
{
public:
    // ------------------------------------------------------------
    // Initialize
    //   texturePath  — wide-string path to a PNG (RGBA recommended)
    //   screenW/H    — render-target dimensions, used to bind viewport
    //   bobSpeed     — radians/second for the sin oscillation
    //   bobAmplitude — peak vertical offset in world pixels
    // Returns false on texture-load failure (a missing PNG is logged
    // but not fatal — the chevron simply does not draw).
    // ------------------------------------------------------------
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                    const std::wstring& texturePath,
                    int screenW, int screenH,
                    float bobSpeed = 4.0f, float bobAmplitude = 6.0f);

    // Advance the bob phase.  Must be called every frame the menu is open.
    void Update(float dt);

    // Draw at world position (x, y).  flipVertical=true rotates the
    // sprite 180 degrees around its pivot AND inverts the bob direction
    // so a single PNG covers both up and down chevrons.
    void Draw(ID3D11DeviceContext* context,
              float x, float y,
              bool flipVertical,
              float scale,
              DirectX::CXMMATRIX transform,
              DirectX::XMVECTOR color);

    // Release the SpriteBatch and texture (called from destructor or on
    // explicit teardown when switching battle scenes).
    void Shutdown();

    // ------------------------------------------------------------
    // Source-texture dimension accessors.
    //
    // Populated by Initialize() from the loaded PNG via D3D's texture
    // descriptor — NOT a hardcoded constant.  Callers compute their
    // own world-space scale as `targetWorldSize / GetWidth()` so that
    // dropping in a higher-resolution PNG renders sharper without
    // changing the on-screen size.
    //
    // Returns 0 if Initialize() has not yet succeeded — callers should
    // guard against divide-by-zero when computing scale.
    // ------------------------------------------------------------
    int GetWidth()  const { return mWidth;  }
    int GetHeight() const { return mHeight; }

private:
    void BindViewport(ID3D11DeviceContext* context);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mTextureSRV;
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates>           mStates;

    int   mWidth        = 64;
    int   mHeight       = 64;
    float mPivotX       = 32.0f;
    float mPivotY       = 32.0f;
    int   mScreenW      = 1280;
    int   mScreenH      = 720;

    float mElapsedTime  = 0.0f;
    float mBobSpeed     = 4.0f;
    float mBobAmplitude = 6.0f;
};
