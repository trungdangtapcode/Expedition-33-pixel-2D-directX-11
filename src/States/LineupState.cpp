// ============================================================
// File: LineupState.cpp
// Responsibility: Lifecycle, input FSM, camera transitions, and
//                 per-character animation state management for
//                 the cinematic Party Lineup screen.
//
// Render helpers live in LineupStateRender.cpp.
//
// Key architectural decisions:
//   - Camera transitions use LineupCameraController (exponential
//     decay lerp, same pattern as BattleCameraController).
//   - Character selection changes update CharacterSlotAnim targets
//     via OnCursorChanged() — the renderer reads interpolated
//     current values and never sees instant snaps.
//   - Exit uses the deferred PopState pattern (§13 of copilot
//     instructions) — mPendingSafeExit is set by the EXITING
//     camera phase completion check, and PopState() is called
//     at the END of Update(), never mid-frame.
// ============================================================
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "LineupState.h"
#include "StateManager.h"
#include "../Renderer/D3DContext.h"
#include "../Battle/ItemRegistry.h"
#include "../Battle/ItemData.h"
#include "../Systems/Inventory.h"
#include "../Systems/PartyManager.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"

#include <windows.h>

// ============================================================
//  Layout config loader
// ============================================================

void LineupState::LoadLayoutConfig()
{
    std::ifstream file("data/lineup_layout.json");
    if (!file.is_open()) {
        LOG("[LineupState] WARNING: lineup_layout.json not found, using defaults.");
        return;
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    using namespace JsonLoader::detail;
    constexpr float kDegToRad = 3.14159265f / 180.0f;

    // Camera config (converted from degrees to radians at load time)
    mCameraConfig.enterZoom          = ParseFloat(ValueOf(src, "camera_enterZoom"), 0.5f);
    mCameraConfig.enterRotation      = ParseFloat(ValueOf(src, "camera_enterRotationDeg"), 0.0f) * kDegToRad;
    mCameraConfig.idleZoom           = ParseFloat(ValueOf(src, "camera_idleZoom"), 1.0f);
    mCameraConfig.idleRotation       = ParseFloat(ValueOf(src, "camera_idleRotationDeg"), 10.0f) * kDegToRad;
    mCameraConfig.characterZoom      = ParseFloat(ValueOf(src, "camera_characterZoom"), 1.8f);
    mCameraConfig.characterRotation  = ParseFloat(ValueOf(src, "camera_characterRotationDeg"), 5.0f) * kDegToRad;
    mCameraConfig.focusedZoom        = ParseFloat(ValueOf(src, "camera_focusedZoom"), 1.0f);
    mCameraConfig.focusedRotation    = ParseFloat(ValueOf(src, "camera_focusedRotationDeg"), 0.0f) * kDegToRad;
    mCameraConfig.exitZoom           = ParseFloat(ValueOf(src, "camera_exitZoom"), 0.5f);
    mCameraConfig.exitRotation       = ParseFloat(ValueOf(src, "camera_exitRotationDeg"), 0.0f) * kDegToRad;
    mCameraConfig.smoothSpeed        = ParseFloat(ValueOf(src, "camera_smoothSpeed"), 4.0f);
    mCameraConfig.fadeSpeed          = ParseFloat(ValueOf(src, "camera_fadeSpeed"), 3.5f);

    // Lineup view
    mLineupLayout.characterSpacing = ParseFloat(ValueOf(src, "lineup_characterSpacing"), 260.0f);
    mLineupLayout.normalScale      = ParseFloat(ValueOf(src, "lineup_normalScale"), 2.2f);
    mLineupLayout.selectedScale    = ParseFloat(ValueOf(src, "lineup_selectedScale"), 3.2f);
    mLineupLayout.selectedOffsetY  = ParseFloat(ValueOf(src, "lineup_selectedOffsetY"), -15.0f);
    mLineupLayout.floorRatio       = ParseFloat(ValueOf(src, "lineup_floorRatio"), 0.68f);
    mLineupLayout.glowRadius       = ParseFloat(ValueOf(src, "lineup_glowRadius"), 130.0f);
    mLineupLayout.slotLerpSpeed    = ParseFloat(ValueOf(src, "lineup_slotLerpSpeed"), 6.0f);
    mLineupLayout.glowOffsetY      = ParseFloat(ValueOf(src, "lineup_glowOffsetY"), -40.0f);

    // Character view (ratio-based: positions scale with screen size)
    mCharLayout.spriteXRatio     = ParseFloat(ValueOf(src, "char_spriteXRatio"), 0.52f);
    mCharLayout.spriteYRatio     = ParseFloat(ValueOf(src, "char_spriteYRatio"), 0.72f);
    mCharLayout.spriteScale      = ParseFloat(ValueOf(src, "char_spriteScale"), 4.5f);
    mCharLayout.glowOffsetY      = ParseFloat(ValueOf(src, "char_glowOffsetY"), -100.0f);
    mCharLayout.glowRadius       = ParseFloat(ValueOf(src, "char_glowRadius"), 180.0f);
    mCharLayout.nameYRatio       = ParseFloat(ValueOf(src, "char_nameYRatio"), 0.06f);
    mCharLayout.slotListXRatio   = ParseFloat(ValueOf(src, "char_slotListXRatio"), 0.04f);
    mCharLayout.slotListYRatio   = ParseFloat(ValueOf(src, "char_slotListYRatio"), 0.18f);
    mCharLayout.slotSpacing      = ParseFloat(ValueOf(src, "char_slotSpacing"), 70.0f);
    mCharLayout.statXRatio       = ParseFloat(ValueOf(src, "char_statXRatio"), 0.72f);
    mCharLayout.statYRatio       = ParseFloat(ValueOf(src, "char_statYRatio"), 0.18f);
    mCharLayout.pickerXRatio     = ParseFloat(ValueOf(src, "char_pickerXRatio"), 0.04f);
    mCharLayout.pickerYRatio     = ParseFloat(ValueOf(src, "char_pickerYRatio"), 0.18f);
    mCharLayout.pickerItemHeight = ParseFloat(ValueOf(src, "char_pickerItemHeight"), 52.0f);
    mCharLayout.pickerMaxVisible = ParseInt(ValueOf(src, "char_pickerMaxVisible"), 7);

    // Font scales
    mFontConfig.titleScale   = ParseFloat(ValueOf(src, "font_titleScale"), 1.2f);
    mFontConfig.sectionScale = ParseFloat(ValueOf(src, "font_sectionScale"), 0.8f);
    mFontConfig.nameScale    = ParseFloat(ValueOf(src, "font_nameScale"), 1.0f);
    mFontConfig.bodyScale    = ParseFloat(ValueOf(src, "font_bodyScale"), 0.8f);
    mFontConfig.smallScale   = ParseFloat(ValueOf(src, "font_smallScale"), 0.65f);

    LOG("[LineupState] Layout config loaded. Camera: idle zoom=%.2f, rotation=%.1f deg, smooth=%.1f",
        mCameraConfig.idleZoom, mCameraConfig.idleRotation / kDegToRad, mCameraConfig.smoothSpeed);
}

// ============================================================
//  Lifecycle
// ============================================================

void LineupState::OnEnter()
{
    LOG("[LineupState] OnEnter");

    auto& d3d = D3DContext::Get();
    ID3D11Device*        device  = d3d.GetDevice();
    ID3D11DeviceContext* context = d3d.GetContext();
    const int W = d3d.GetWidth();
    const int H = d3d.GetHeight();

    // Force-load item catalog for equipment display.
    ItemRegistry::Get().EnsureLoaded();

    // Load layout from JSON (populates mCameraConfig + mLineupLayout + mCharLayout).
    LoadLayoutConfig();

    // Initialize renderers.
    mGlow.Initialize(device);
    mTextRenderer.Initialize(device, context,
        L"assets/fonts/arial_16.spritefont", W, H);

    // Camera controller — starts in ENTERING phase (zoomed out + black).
    mCameraCtrl.Initialize(W, H, mCameraConfig);

    // Load one WorldSpriteRenderer per party member, playing "idle".
    const auto& party = PartyManager::Get().GetActiveParty();
    mPartySize = static_cast<int>(party.size());

    mSprites.clear();
    mSprites.resize(mPartySize);

    for (int i = 0; i < mPartySize; ++i)
    {
        const PartyMember& member = party[i];

        SpriteSheet sheet;
        if (!JsonLoader::LoadSpriteSheet(member.animJsonPath, sheet))
        {
            LOG("[LineupState] WARNING: Failed to load sprite sheet for %s", member.name.c_str());
            continue;
        }

        auto renderer = std::make_unique<WorldSpriteRenderer>();
        if (!renderer->Initialize(device, context, member.animationPath, sheet))
        {
            LOG("[LineupState] WARNING: Failed to initialize sprite for %s", member.name.c_str());
            continue;
        }

        renderer->PlayClip("idle");
        mSprites[i] = std::move(renderer);
    }

    // Initialize per-character slot animation states.
    mSlotAnims.clear();
    mSlotAnims.resize(mPartySize);
    for (int i = 0; i < mPartySize; ++i)
    {
        if (i == 0)  // cursor starts at 0
        {
            mSlotAnims[i].SetSelected(mLineupLayout.selectedScale, mLineupLayout.selectedOffsetY);
            // Seed current to target so selected char starts at correct scale.
            mSlotAnims[i].currentScale     = mLineupLayout.selectedScale;
            mSlotAnims[i].currentGlowAlpha = 1.0f;
            mSlotAnims[i].currentOffsetY   = mLineupLayout.selectedOffsetY;
        }
        else
        {
            mSlotAnims[i].SetNormal(mLineupLayout.normalScale);
            mSlotAnims[i].currentScale   = mLineupLayout.normalScale;
        }
    }

    // Reset state.
    mView           = View::Lineup;
    mPhase          = Phase::MemberSelect;
    mMemberCursor   = 0;
    mSlotCursor     = 0;
    mPickerCursor   = 0;
    mPickerScroll   = 0;
    mPickerSlot     = EquipSlot::None;
    mFlashMessage.clear();
    mFlashTimer     = 0.0f;
    mElapsed        = 0.0f;
    mPendingSafeExit = false;

    // Absorb the key press that opened this state.
    mUpWasDown    = true;
    mDownWasDown  = true;
    mLeftWasDown  = true;
    mRightWasDown = true;
    mEnterWasDown = true;
    mEscWasDown   = true;
    mLWasDown     = true;
}

void LineupState::OnExit()
{
    LOG("[LineupState] OnExit");

    for (auto& sprite : mSprites)
    {
        if (sprite) sprite->Shutdown();
    }
    mSprites.clear();
    mSlotAnims.clear();

    mTextRenderer.Shutdown();
    mGlow.Shutdown();
}

// ============================================================
//  Cursor change — drives the interpolation system
// ============================================================

void LineupState::OnCursorChanged(int oldCursor, int newCursor)
{
    if (oldCursor >= 0 && oldCursor < static_cast<int>(mSlotAnims.size()))
        mSlotAnims[oldCursor].SetNormal(mLineupLayout.normalScale);

    if (newCursor >= 0 && newCursor < static_cast<int>(mSlotAnims.size()))
        mSlotAnims[newCursor].SetSelected(mLineupLayout.selectedScale, mLineupLayout.selectedOffsetY);
}

// ============================================================
//  Data Refresh
// ============================================================

void LineupState::RefreshPicker()
{
    mPickerItems.clear();
    for (const std::string& id : Inventory::Get().OwnedIds())
    {
        const ItemData* item = ItemRegistry::Get().Find(id);
        if (item && item->equipSlot == mPickerSlot)
            mPickerItems.push_back(id);
    }
    if (mPickerCursor < 0) mPickerCursor = 0;
    const int maxCursor = static_cast<int>(mPickerItems.size());
    if (mPickerCursor > maxCursor) mPickerCursor = maxCursor;
    mPickerScroll = 0;
}

// ============================================================
//  Update
// ============================================================

void LineupState::Update(float dt)
{
    mElapsed += dt;

    // Advance all sprite animations.
    for (auto& sprite : mSprites)
    {
        if (sprite) sprite->Update(dt);
    }

    // Advance per-character slot animations (exponential decay).
    for (auto& anim : mSlotAnims)
        anim.Update(dt, mLineupLayout.slotLerpSpeed);

    // Advance camera transition.
    mCameraCtrl.Update(dt);

    // Flash timer.
    if (mFlashTimer > 0.0f)
    {
        mFlashTimer -= dt;
        if (mFlashTimer < 0.0f) mFlashTimer = 0.0f;
    }

    // --- Phase gate: auto-transition when camera reaches target ---

    // ENTERING → IDLE (camera finished zooming in)
    if (mCameraCtrl.GetPhase() == LineupCameraPhase::ENTERING &&
        mCameraCtrl.IsEntryComplete())
    {
        mCameraCtrl.SetPhase(LineupCameraPhase::IDLE);
    }

    // ZOOMING_IN → FOCUSED when zoom settles (camera holds at characterZoom)
    if (mCameraCtrl.GetPhase() == LineupCameraPhase::ZOOMING_IN &&
        mCameraCtrl.IsFocusSettled())
    {
        mView  = View::Character;
        mPhase = Phase::SlotSelect;
        mCameraCtrl.SetPhase(LineupCameraPhase::FOCUSED);
    }

    // ZOOMING_OUT → view switch when zoom settles
    if (mCameraCtrl.GetPhase() == LineupCameraPhase::ZOOMING_OUT &&
        mCameraCtrl.IsZoomOutComplete())
    {
        mView  = View::Lineup;
        mPhase = Phase::MemberSelect;
        mCameraCtrl.SetPhase(LineupCameraPhase::IDLE);
    }

    // EXITING → deferred PopState (iris transition pattern §13)
    if (mCameraCtrl.GetPhase() == LineupCameraPhase::EXITING &&
        mCameraCtrl.IsExitComplete())
    {
        mPendingSafeExit = true;
    }

    // Block input during camera transitions.
    const auto camPhase = mCameraCtrl.GetPhase();
    const bool inputBlocked = (camPhase == LineupCameraPhase::ENTERING ||
                               camPhase == LineupCameraPhase::ZOOMING_IN ||
                               camPhase == LineupCameraPhase::ZOOMING_OUT ||
                               camPhase == LineupCameraPhase::EXITING);

    if (!inputBlocked)
    {
        // Edge-detect press lambda.
        auto pressed = [](int vk, bool& wasDown) -> bool {
            const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
            const bool fresh = down && !wasDown;
            wasDown = down;
            return fresh;
        };

        // Global close: Esc, L, or Backspace.
        const bool escPressed  = pressed(VK_ESCAPE, mEscWasDown);
        const bool lPressed    = pressed('L',       mLWasDown);
        const bool backPressed = pressed(VK_BACK,   mBackWasDown);

        if (escPressed || lPressed || backPressed)
        {
            if (mPhase == Phase::EquipPicker)
            {
                mPhase = Phase::SlotSelect;
            }
            else if (mView == View::Character)
            {
                // Zoom back out to lineup overview.
                mCameraCtrl.SetPhase(LineupCameraPhase::ZOOMING_OUT);
            }
            else
            {
                // Start cinematic exit.
                mCameraCtrl.SetPhase(LineupCameraPhase::EXITING);
            }
        }
        else
        {
            // Dispatch to phase-specific handler.
            switch (mPhase)
            {
            case Phase::MemberSelect: HandleMemberSelectInput(); break;
            case Phase::SlotSelect:   HandleSlotSelectInput();   break;
            case Phase::EquipPicker:  HandlePickerInput();       break;
            }
        }
    }
    else
    {
        // Still consume key states to prevent stale edge detection
        // after transitions complete.
        auto consume = [](int vk, bool& wasDown) {
            wasDown = (GetAsyncKeyState(vk) & 0x8000) != 0;
        };
        consume(VK_ESCAPE, mEscWasDown);
        consume('L',       mLWasDown);
        consume(VK_UP,     mUpWasDown);
        consume(VK_DOWN,   mDownWasDown);
        consume(VK_LEFT,   mLeftWasDown);
        consume(VK_RIGHT,  mRightWasDown);
        consume(VK_RETURN, mEnterWasDown);
        consume(VK_BACK,   mBackWasDown);
    }

    // --- Deferred safe exit (end of Update, never mid-frame) ---
    if (mPendingSafeExit)
    {
        StateManager::Get().PopState();
        return;
    }
}

// ============================================================
//  Phase-specific input handlers
// ============================================================

void LineupState::HandleMemberSelectInput()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    if (mPartySize == 0) return;

    if (pressed(VK_LEFT,  mLeftWasDown))
    {
        int oldCursor = mMemberCursor;
        mMemberCursor = (mMemberCursor - 1 + mPartySize) % mPartySize;
        OnCursorChanged(oldCursor, mMemberCursor);
    }
    if (pressed(VK_RIGHT, mRightWasDown))
    {
        int oldCursor = mMemberCursor;
        mMemberCursor = (mMemberCursor + 1) % mPartySize;
        OnCursorChanged(oldCursor, mMemberCursor);
    }

    if (pressed(VK_RETURN, mEnterWasDown))
    {
        // Compute world-space focus position for the camera zoom-in.
        auto& d3d = D3DContext::Get();
        const float fW = static_cast<float>(d3d.GetWidth());
        const float fH = static_cast<float>(d3d.GetHeight());
        const float spacing = mLineupLayout.characterSpacing;
        const float totalW  = (mPartySize - 1) * spacing;
        const float cx = (fW - totalW) * 0.5f + mMemberCursor * spacing;
        const float cy = fH * mLineupLayout.floorRatio;

        mCameraCtrl.SetFocusPosition(cx - fW * 0.5f, cy - fH * 0.5f);
        mCameraCtrl.SetPhase(LineupCameraPhase::ZOOMING_IN);
        mSlotCursor = 0;
    }
}

void LineupState::HandleSlotSelectInput()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    if (pressed(VK_UP,   mUpWasDown))
        mSlotCursor = (mSlotCursor - 1 + kEquipSlotCount) % kEquipSlotCount;
    if (pressed(VK_DOWN, mDownWasDown))
        mSlotCursor = (mSlotCursor + 1) % kEquipSlotCount;

    if (pressed(VK_LEFT,  mLeftWasDown))
    {
        int oldCursor = mMemberCursor;
        mMemberCursor = (mMemberCursor - 1 + mPartySize) % mPartySize;
        OnCursorChanged(oldCursor, mMemberCursor);
    }
    if (pressed(VK_RIGHT, mRightWasDown))
    {
        int oldCursor = mMemberCursor;
        mMemberCursor = (mMemberCursor + 1) % mPartySize;
        OnCursorChanged(oldCursor, mMemberCursor);
    }

    if (pressed(VK_RETURN, mEnterWasDown))
    {
        constexpr EquipSlot order[kEquipSlotCount] = {
            EquipSlot::Weapon, EquipSlot::Body, EquipSlot::Head, EquipSlot::Accessory
        };
        mPickerSlot   = order[mSlotCursor];
        mPickerCursor = 0;
        mPickerScroll = 0;
        RefreshPicker();
        mPhase = Phase::EquipPicker;
    }
}

void LineupState::HandlePickerInput()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down  = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    const int total = static_cast<int>(mPickerItems.size()) + 1;

    if (pressed(VK_UP,   mUpWasDown))
        mPickerCursor = (mPickerCursor - 1 + total) % total;
    if (pressed(VK_DOWN, mDownWasDown))
        mPickerCursor = (mPickerCursor + 1) % total;

    const int maxVisible = mCharLayout.pickerMaxVisible;
    if (mPickerCursor < mPickerScroll)
        mPickerScroll = mPickerCursor;
    if (mPickerCursor >= mPickerScroll + maxVisible)
        mPickerScroll = mPickerCursor - maxVisible + 1;

    if (pressed(VK_RETURN, mEnterWasDown))
    {
        const int memberIdx = mMemberCursor;

        if (PickerCursorIsUnequip())
        {
            std::string equippedId = PartyManager::Get().GetEquippedItem(memberIdx, mPickerSlot);
            if (!equippedId.empty())
            {
                PartyManager::Get().UnequipItem(memberIdx, mPickerSlot);
                const ItemData* item = ItemRegistry::Get().Find(equippedId);
                mFlashMessage = std::string("Unequipped ") + (item ? item->name : equippedId);
                mFlashTimer = kFlashDuration;
            }
        }
        else
        {
            const std::string& itemId = mPickerItems[mPickerCursor];
            if (PartyManager::Get().EquipItem(memberIdx, mPickerSlot, itemId))
            {
                const ItemData* item = ItemRegistry::Get().Find(itemId);
                mFlashMessage = std::string("Equipped ") + (item ? item->name : itemId);
                mFlashTimer = kFlashDuration;
            }
            else
            {
                mFlashMessage = "Could not equip that item.";
                mFlashTimer = kFlashDuration;
            }
        }

        RefreshPicker();
        const int newTotal = static_cast<int>(mPickerItems.size()) + 1;
        if (mPickerCursor >= newTotal) mPickerCursor = newTotal - 1;
    }
}

// ============================================================
//  Render dispatch (calls into LineupStateRender.cpp)
// ============================================================

void LineupState::Render()
{
    switch (mView)
    {
    case View::Lineup:    RenderLineupView();    break;
    case View::Character: RenderCharacterView(); break;
    }
}
