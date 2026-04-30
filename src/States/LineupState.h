// ============================================================
// File: LineupState.h
// Responsibility: Cinematic Party Lineup & Equipment screen.
//
// Visual design:
//   Characters ARE the scene — no UI panels, no dialog boxes.
//   Camera tilted 10° Dutch angle for dramatic composition.
//   SDF circle glow behind selected character.
//   All transitions smoothly interpolated via exponential decay.
//
// Camera: Owned by LineupCameraController (same pattern as
//   BattleCameraController). 5 phases: ENTERING → IDLE →
//   ZOOMING_IN/OUT → EXITING.
//
// Per-character animation: CharacterSlotAnim struct holds
//   interpolated scale, glow alpha, and Y offset. The renderer
//   reads current values — never computes instant ones.
//
// Rendering architecture (3 strict passes, no interleaving):
//   Pass 1 — CircleRenderer:       atmospheric glow
//   Pass 2 — WorldSpriteRenderer:  character sprites (tilted cam)
//   Pass 3 — BattleTextRenderer:   all text (screen space)
//
// Input FSM:
//   MEMBER_SELECT  — Left/Right cycles members, Enter zooms
//   SLOT_SELECT    — Up/Down cycles 4 equip slots
//   EQUIP_PICKER   — scrollable item list from Inventory
//
// All layout and animation constants from data/lineup_layout.json.
// ============================================================
#pragma once
#include "IGameState.h"
#include "LineupCameraController.h"
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Renderer/CircleRenderer.h"
#include "../UI/BattleTextRenderer.h"
#include "../Battle/ItemData.h"
#include <vector>
#include <string>
#include <memory>

// ============================================================
// CharacterSlotAnim: per-character interpolation state.
//
// When the cursor changes, the old slot's targets revert to
// normal values and the new slot's targets become selected values.
// Update() lerps current → target via exponential decay each frame.
//
// The renderer reads currentScale / currentGlowAlpha / currentOffsetY
// and never sees instant snaps.
// ============================================================
struct CharacterSlotAnim
{
    float currentScale     = 1.0f;
    float targetScale      = 1.0f;
    float currentGlowAlpha = 0.0f;
    float targetGlowAlpha  = 0.0f;
    float currentOffsetY   = 0.0f;
    float targetOffsetY    = 0.0f;

    // Exponential decay lerp — same formula as BattleCameraController.
    void Update(float dt, float lerpSpeed)
    {
        const float k = lerpSpeed * dt;
        currentScale     += (targetScale     - currentScale)     * k;
        currentGlowAlpha += (targetGlowAlpha - currentGlowAlpha) * k;
        currentOffsetY   += (targetOffsetY   - currentOffsetY)   * k;
    }

    // Set this slot as the selected character.
    void SetSelected(float selScale, float selOffsetY)
    {
        targetScale     = selScale;
        targetGlowAlpha = 1.0f;
        targetOffsetY   = selOffsetY;
    }

    // Set this slot as a non-selected character.
    void SetNormal(float normalScale)
    {
        targetScale     = normalScale;
        targetGlowAlpha = 0.0f;
        targetOffsetY   = 0.0f;
    }
};

class LineupState : public IGameState
{
public:
    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "LineupState"; }

private:
    // ============================================================
    //  View / Phase enums
    // ============================================================
    enum class View  { Lineup, Character };
    enum class Phase { MemberSelect, SlotSelect, EquipPicker };

    // ============================================================
    //  Input handlers (one per phase)
    // ============================================================
    void HandleMemberSelectInput();
    void HandleSlotSelectInput();
    void HandlePickerInput();

    // ============================================================
    //  Data refresh
    // ============================================================
    void RefreshPicker();

    // ============================================================
    //  Cursor change — updates slot anim targets
    // ============================================================
    void OnCursorChanged(int oldCursor, int newCursor);

    // ============================================================
    //  Render helpers — implemented in LineupStateRender.cpp
    // ============================================================
    void RenderLineupView();
    void RenderCharacterView();
    void RenderEquipPickerText();
    void RenderStatPanelText(float x, float y, bool showPreview);

    // ============================================================
    //  Renderers
    // ============================================================

    // One WorldSpriteRenderer per party member (animated idle clips).
    std::vector<std::unique_ptr<WorldSpriteRenderer>> mSprites;

    // SDF circle renderer for atmospheric glow effects.
    CircleRenderer mGlow;

    // Text renderer for all labels, names, and stat numbers.
    BattleTextRenderer mTextRenderer;

    // Cinematic camera controller with phase-based transitions.
    LineupCameraController mCameraCtrl;

    // Per-character interpolation states (parallel to mSprites).
    std::vector<CharacterSlotAnim> mSlotAnims;

    // ============================================================
    //  State
    // ============================================================
    View   mView  = View::Lineup;
    Phase  mPhase = Phase::MemberSelect;

    int mMemberCursor = 0;
    int mPartySize    = 0;
    int mSlotCursor   = 0;

    EquipSlot                mPickerSlot   = EquipSlot::None;
    std::vector<std::string> mPickerItems;
    int                      mPickerCursor = 0;
    int                      mPickerScroll = 0;

    bool PickerCursorIsUnequip() const
    {
        return mPickerCursor == static_cast<int>(mPickerItems.size());
    }

    // Flash message
    std::string mFlashMessage;
    float       mFlashTimer = 0.0f;
    static constexpr float kFlashDuration = 2.0f;

    // Elapsed timer (sprite animation, glow breathing)
    float mElapsed = 0.0f;

    // Pending safe exit flag (deferred PopState per iris transition pattern)
    bool mPendingSafeExit = false;

    // ============================================================
    //  Layout config (loaded from data/lineup_layout.json)
    // ============================================================
    struct LineupLayout
    {
        float characterSpacing = 260.0f;
        float normalScale      = 2.2f;
        float selectedScale    = 3.2f;
        float selectedOffsetY  = -15.0f;
        float floorRatio       = 0.68f;
        float glowRadius       = 130.0f;
        float slotLerpSpeed    = 6.0f;
        float glowOffsetY      = -40.0f;  // glow center relative to sprite feet (negative = above)
    } mLineupLayout;

    struct CharLayout
    {
        // --- Character sprite (CENTER column) ---
        float spriteXRatio     = 0.52f;   // ratio of screen width  (0.5 = center)
        float spriteYRatio     = 0.72f;   // ratio of screen height (feet position)
        float spriteScale      = 4.5f;

        // --- Glow (derived from sprite position via WorldToScreen) ---
        float glowOffsetY      = -100.0f; // negative = above feet (body center)
        float glowRadius       = 180.0f;

        // --- Name + level (above sprite) ---
        float nameYRatio       = 0.06f;   // ratio of screen height

        // --- Equipment panel (LEFT column) ---
        float slotListXRatio   = 0.04f;   // ratio of screen width
        float slotListYRatio   = 0.18f;   // ratio of screen height
        float slotSpacing      = 70.0f;

        // --- Stats panel (RIGHT column) ---
        float statXRatio       = 0.72f;   // ratio of screen width
        float statYRatio       = 0.18f;   // ratio of screen height

        // --- Equipment picker (overlays left column) ---
        float pickerXRatio     = 0.04f;   // ratio of screen width
        float pickerYRatio     = 0.18f;   // ratio of screen height
        float pickerItemHeight = 52.0f;
        int   pickerMaxVisible = 7;
    } mCharLayout;

    // Font scales — grouped by semantic role, loaded from JSON.
    // The render file uses ONLY these values — zero hardcoded font sizes.
    struct FontConfig
    {
        float titleScale   = 1.2f;  // "Team" title
        float sectionScale = 0.8f;  // section headers: "EQUIPMENT", "STATS", "SELECT ITEM"
        float nameScale    = 1.0f;  // character names
        float bodyScale    = 0.8f;  // equipment items, slot labels, stat labels, stat values
        float smallScale   = 0.65f; // Lv/HP info, hint footer, bonus text
    } mFontConfig;

    // Camera config — loaded from JSON, passed to LineupCameraController.
    LineupCameraConfig mCameraConfig;

    // Loads all layout values from data/lineup_layout.json.
    void LoadLayoutConfig();

    // ============================================================
    //  Input edge trackers
    // ============================================================
    bool mUpWasDown    = false;
    bool mDownWasDown  = false;
    bool mLeftWasDown  = false;
    bool mRightWasDown = false;
    bool mEnterWasDown = false;
    bool mEscWasDown   = false;
    bool mLWasDown     = false;
    bool mBackWasDown  = false;
};
