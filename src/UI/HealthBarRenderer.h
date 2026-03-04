// ============================================================
// File: HealthBarRenderer.h
// Responsibility: Render a three-layer HP bar UI widget.
//
// Visual layer stack (bottom to top):
//   1. Background  — drawn first (assets/UI/UI_hp_background.png)
//                    The gray/dark pixels that show when HP is low.
//   2. HP Fill     — drawn over the background, RIGHT-CLIPPED to HP ratio
//                    Using clipping (srcRect trim), NOT scaling, so the
//                    pixel art is never distorted.
//                    Fill comes from the SAME background texture: we draw
//                    only the fill-area strip (hpBarLeft → hpBarRight trimmed).
//   3. Frame + portrait — drawn last (assets/UI/UI_verso_hp.png)
//                    The decorative border and character portrait on top.
//
// HP smoothing (Lerp / Tweening):
//   mTargetHP    — updated instantly when the "verso_hp_changed" event fires.
//   mDisplayedHP — moves toward mTargetHP each Update() via:
//     mDisplayedHP += (mTargetHP - mDisplayedHP) * kLerpSpeed * dt
//   This produces a smooth bar drain animation independent of frame rate.
//
// Observer:
//   Subscribe to "verso_hp_changed" in Initialize().
//   Unsubscribe in Shutdown() using the stored ListenerID.
//   EventData.value carries the NEW absolute HP value (int cast to float).
//
// Ownership:
//   Two ComPtr<ID3D11ShaderResourceView> — one per texture.
//   One SpriteBatch shared for all layers (Begin/Draw/Draw/Draw/End per frame).
//   CommonStates — depth-none + non-premultiplied blend for correct alpha.
//
// Screen position:
//   Always top-left anchored (matches JSON "align": "top-left").
//   Rendered at pixel (0, 0) — the full 256x256 background sits in the corner.
//
// Lifetime:
//   Initialize() called from BattleState::OnEnter().
//   Shutdown()   called from BattleState::OnExit().
//
// Common mistakes:
//   1. Scaling the fill instead of clipping → pixel art stretches.
//   2. Not unsubscribing from EventManager → dangling lambda capture crashes.
//   3. Forgetting DepthNone → fill invisible behind CircleRenderer's depth writes.
//   4. Calling SetMaxHP(0) → division by zero in ratio calculation.
// ============================================================
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <WICTextureLoader.h>
#include <memory>
#include <string>
#include "../UI/HealthBarConfig.h"
#include "../Events/EventManager.h"

class HealthBarRenderer
{
public:
    // ----------------------------------------------------------------
    // Initialize
    // Purpose:
    //   Load both textures, create the SpriteBatch + CommonStates,
    //   parse the JSON config, and subscribe to "verso_hp_changed".
    // Parameters:
    //   device         — used once for texture upload + state creation
    //   context        — stored by SpriteBatch for Begin/End
    //   bgTexturePath  — path to UI_hp_background.png
    //   frameTexPath   — path to UI_verso_hp.png (frame + portrait)
    //   configJsonPath — path to HP_description.json
    //   screenW/H      — render target dimensions (for viewport binding)
    // Returns:
    //   true if all resources loaded successfully.
    // ----------------------------------------------------------------
    bool Initialize(ID3D11Device*        device,
                    ID3D11DeviceContext*  context,
                    const std::wstring&  bgTexturePath,
                    const std::wstring&  frameTexPath,
                    const std::string&   configJsonPath,
                    int screenW, int screenH);

    // ----------------------------------------------------------------
    // SetHP / SetMaxHP
    // Purpose:
    //   Direct setters used by BattleState on init to seed the current HP
    //   without firing an event (the EventManager isn't available yet
    //   from Initialize, and we don't want a spurious lerp on first frame).
    // ----------------------------------------------------------------
    void SetHP(float hp);
    void SetMaxHP(float maxHp);

    // ----------------------------------------------------------------
    // SetScreenSize
    // Purpose:
    //   Update stored dimensions after a window resize.
    //   Must be called before the next Render().
    // ----------------------------------------------------------------
    void SetScreenSize(int w, int h);

    // ----------------------------------------------------------------
    // Update
    // Purpose:
    //   Advance the HP lerp: mDisplayedHP smoothly approaches mTargetHP.
    //   dt must be the frame delta time in seconds (from GameTimer).
    //
    //   Formula:
    //     mDisplayedHP += (mTargetHP - mDisplayedHP) * kLerpSpeed * dt
    //
    //   At kLerpSpeed = 4.0f the bar takes ~0.5 s to drain fully.
    //   Higher values = faster drain; lower = slower, more dramatic.
    // ----------------------------------------------------------------
    void Update(float dt);

    // ----------------------------------------------------------------
    // Render
    // Purpose:
    //   Issue one SpriteBatch Begin/End pair that draws all three layers.
    //
    //   Layer order:
    //     1. Full background quad    (256 x 256, position = (0,0))
    //     2. HP fill strip           (clipped to hp ratio width)
    //     3. Frame + portrait quad   (256 x 256, same position)
    //
    //   Clipping math for the fill:
    //     ratio       = mDisplayedHP / mMaxHP          (clamped to [0,1])
    //     fillWidth   = config.HpBarWidth() * ratio    (pixel width to draw)
    //     srcRect     = { hpBarLeft, hpBarTop,
    //                     hpBarLeft + fillWidth, hpBarBottom }
    //     destPos     = (hpBarLeft, hpBarTop)  [screen pixels, top-left aligned]
    //
    //   Because we shrink srcRect.right instead of changing the scale,
    //   the remaining pixels are simply not sampled — no stretching occurs.
    // ----------------------------------------------------------------
    void Render(ID3D11DeviceContext* context);

    // ----------------------------------------------------------------
    // Shutdown
    // Purpose:
    //   Release all GPU resources and unsubscribe from EventManager.
    //   Must be called before D3DContext::Shutdown().
    // ----------------------------------------------------------------
    void Shutdown();

    bool IsInitialized() const { return mSpriteBatch != nullptr; }

private:
    // -- GPU resources --
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mBgSRV;     // background overlay (semi-transparent dark shadow)
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFrameSRV;  // frame + portrait texture
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;   // 1x1 white texture — tinted to HP-bar color at draw time
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates>           mStates;

    // -- Layout config (from JSON) --
    HealthBarConfig mConfig;

    // -- Screen dimensions (for viewport binding) --
    int mScreenW = 1280;
    int mScreenH = 720;

    // -- HP state --
    float mMaxHP       = 100.0f;   // maximum HP (set from combatant at battle start)
    float mTargetHP    = 100.0f;   // actual current HP (updated by event)
    float mDisplayedHP = 100.0f;   // smoothly lerped display value

    // Lerp speed: higher = faster drain animation.
    // 4.0 means the gap halves every ~0.25 s (exponential approach).
    static constexpr float kLerpSpeed = 4.0f;

    // -- Event subscription --
    ListenerID mHpListenerID = -1;  // used to unsubscribe in Shutdown()

    // -- Helpers --
    // Bind the viewport and set mSpriteBatch's internal viewport (bypasses
    // RSGetViewports inside SpriteBatch::End to avoid the 0-viewport throw).
    void BindViewport(ID3D11DeviceContext* context);
};
