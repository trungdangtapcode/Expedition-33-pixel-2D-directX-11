// ============================================================
// File: EnemyHpBarRenderer.h
// Responsibility: Render up to 3 enemy HP bars centered at the top of
//                 the screen during a battle.
//
// Layout:
//   Bars are centered horizontally.  Scale is derived from kTargetBarHeight
//   (the desired rendered height in screen pixels) so the bar stays compact
//   regardless of resolution.  Width follows the texture aspect ratio.
//   Stacked vertically from kTopPadding below the top edge, separated by
//   kBarSpacing pixels.  Slot 0 = topmost; slot 2 = lowest.
//
// Visual layers per bar (three SpriteBatch passes — all active bars per pass):
//   Pass 1 — background  (enemy-hp-ui-background.png, non-premultiplied)
//   Pass 2 — HP fill     (1x1 white tinted red, opaque, clipped to ratio)
//   Pass 3 — frame/chrome(enemy-hp-ui.png, non-premultiplied)
//   This ordering matches the player HealthBarRenderer and guarantees the
//   frame chrome always sits on top of the fill.
//
// HP smoothing (exponential approach, same as HealthBarRenderer):
//   mDisplayedHP[i] += (mTargetHP[i] - mDisplayedHP[i]) * kLerpSpeed * dt
//   Seeded to actual HP on the first SetEnemy() call — no lerp from 0.
//
// Data flow — polling, not events:
//   BattleState::Update() calls SetEnemy(slot, hp, maxHp, active) every
//   frame after mBattle.Update() resolves actions.
//
// Ownership:
//   BattleState owns one EnemyHpBarRenderer by value.
//   GPU resources are released in Shutdown() before D3DContext teardown.
//
// Common mistakes:
//   1. Not calling SetEnemy(slot, 0, max, false) when an enemy dies —
//      bar stays visible.
//   2. Passing maxHp=0 — division by zero; guarded by max(maxHp, 1).
//   3. Forgetting BindViewport() before SpriteBatch::Begin() — throws
//      std::runtime_error when RS viewports were cleared by another renderer.
// ============================================================
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <WICTextureLoader.h>
#include <memory>
#include <string>
#include "BattleTextRenderer.h"
#include "UIEffectState.h"

class EnemyHpBarRenderer
{
public:
    // Maximum HP bars — one per possible enemy slot.
    static constexpr int kMaxSlots = 3;

    // ----------------------------------------------------------------
    // Initialize
    // Purpose:
    //   Load both bar textures, create the 1x1 white fill texture, parse
    //   the JSON layout config, and create the SpriteBatch + CommonStates.
    //
    // Parameters:
    //   device/context   — D3D11 device and immediate context
    //   bgTexturePath    — path to enemy-hp-ui-background.png (wide string)
    //   frameTexturePath — path to enemy-hp-ui.png (frame/chrome, wide string)
    //   configJsonPath   — path to enemy-hp-ui.json (UTF-8 string)
    //   screenW/H        — render target dimensions (for layout + viewport)
    //
    // Returns: true if all GPU resources were created successfully.
    // ----------------------------------------------------------------
    bool Initialize(ID3D11Device*        device,
                    ID3D11DeviceContext*  context,
                    const std::wstring&  bgTexturePath,
                    const std::wstring&  frameTexturePath,
                    const std::string&   configJsonPath,
                    int screenW, int screenH);

    // ----------------------------------------------------------------
    // SetEnemy
    // Purpose:
    //   Synchronize one slot from BattleManager every frame.
    //   active=false hides the bar; active=true shows it.
    //   First call with hp>0 seeds mDisplayedHP so the bar opens at the
    //   correct fill level with no lerp-from-zero animation.
    // ----------------------------------------------------------------
    void SetEnemy(int slot, float hp, float maxHp, bool active);

    // ----------------------------------------------------------------
    // SetEnemyName
    // Purpose:
    //   Store the display name for a slot.  Call once during battle setup
    //   (names do not change mid-battle).  Stored as a plain std::string;
    //   drawn centered above the HP bar in Render() via BattleTextRenderer.
    // ----------------------------------------------------------------
    void SetEnemyName(int slot, const std::string& name);

    // ----------------------------------------------------------------
    // SetTextRenderer
    // Purpose:
    //   Inject the shared BattleTextRenderer owned by BattleState.
    //   Must be called before the first Render() for names to appear.
    //   Pass nullptr to suppress name rendering without crashing.
    // ----------------------------------------------------------------
    void SetTextRenderer(BattleTextRenderer* textRenderer);

    // Enable effect scaling per active slot
    void SetTargetScale(int slot, float scale);

    // Update all HP lerps and effects
    void Update(float dt);

    // Draw all active bars.  Four passes: BG, fill, frame, names.
    void Render(ID3D11DeviceContext* context);

    // Release all GPU resources.
    void Shutdown();

    // Update stored screen dimensions after a window resize.
    void SetScreenSize(int w, int h) { mScreenW = w; mScreenH = h; }

    bool IsInitialized() const { return mSpriteBatch != nullptr; }

private:
    // -- GPU resources --
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mBgSRV;    // enemy-hp-ui-background.png
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFrameSRV; // enemy-hp-ui.png (frame/chrome)
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;  // 1x1 white, tinted at draw time
    std::unique_ptr<DirectX::SpriteBatch>             mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates>            mStates;

    // -- Layout config loaded from JSON (enemy-hp-ui.json) --
    // Defaults match the shipped file so the renderer works even if parsing fails.
    int mTexW     = 128;   // full texture width  (pixels)
    int mTexH     = 16;    // full texture height (pixels)
    int mHpLeft   = 16;    // HP fill region left  edge (texture pixel)
    int mHpTop    = 5;     // HP fill region top   edge
    int mHpRight  = 111;   // HP fill region right edge
    int mHpBottom = 9;     // HP fill region bottom edge

    // -- Per-slot HP and name state --
    float       mTargetHP   [kMaxSlots] = {};
    float       mRedHP      [kMaxSlots] = {};
    float       mWhiteHP    [kMaxSlots] = {};
    float       mDelayTimer [kMaxSlots] = {};
    float       mMaxHP      [kMaxSlots] = { 1.0f, 1.0f, 1.0f };
    bool        mSlotActive [kMaxSlots] = {};
    std::string mEnemyName  [kMaxSlots];      // display name drawn above each bar
    UIEffectState mEffectState [kMaxSlots];   // per-slot scale & shake animation

    // Non-owning pointer to the shared text renderer in BattleState.
    // Null-safe: Render() checks IsReady() before drawing names.
    BattleTextRenderer* mTextRenderer = nullptr;

    int mScreenW = 1280;
    int mScreenH = 720;

    // Fast drain for red bar
    static constexpr float kRedLerpSpeed    = 15.0f;
    // Slow catch-up drain for white background bar
    static constexpr float kWhiteLerpSpeed  = 3.0f;
    // Seconds to wait before white bar begins to catch up
    static constexpr float kDelayDuration   = 0.8f;

    // Desired rendered bar height in screen pixels.
    // scaleY = kTargetBarHeight / mTexH; height is always this many pixels tall.
    static constexpr float kTargetBarHeight = 32.0f;

    // Rendered bar width as a fraction of the screen width.
    // scaleX = screenW * kBarWidthFactor / mTexW; independent of scaleY so
    // the bar is stretched horizontally without changing its pixel-art height.
    static constexpr float kBarWidthFactor  = 0.6f;

    // Pixels from the top of the screen to the first bar's name label.
    // Must be >= kNameLineHeight + kNameGapY so slot 0's name is not clipped.
    static constexpr float kTopPadding      = 36.0f;

    // Rendered font line height in screen pixels (matches arial_16.spritefont).
    static constexpr float kNameLineHeight  = 16.0f;

    // Gap in pixels between the bottom of a name label and the top of its bar.
    static constexpr float kNameGapY        = 4.0f;

    // Pixels of vertical gap between the BOTTOM of one bar and the name label
    // of the NEXT slot below it.  Visible breathing room between slots.
    static constexpr float kBarSpacing      = 6.0f;

    // Bind the viewport so SpriteBatch does not throw when the RS stage
    // has no viewport set (can happen after WorldSpriteRenderer resets it).
    void BindViewport(ID3D11DeviceContext* context);
};
