// ============================================================
// File: BattleRenderer.h
// Responsibility: Render all combatant sprites during a battle session.
//
// Coordinate system — ONE space used everywhere: WORLD SPACE.
//   Camera2D at pos=(0,0) zoom=1 maps world origin (0,0) to the screen center.
//   +X right, +Y down.  World unit = 1 pixel at zoom=1.
//
//   Slot positions are stored as world-space floats (computed once from the
//   screen-pixel design values using the formula: worldX = px - W/2).
//   All internal methods — Render(), SetCameraPhase(), GetSlotPos() — speak
//   only world space.  No conversion code is duplicated anywhere.
//
// Screen-pixel design layout (1280x720) — staggered diagonal formation:
//
//   PLAYER SIDE (left, facing right)    ENEMY SIDE (right, facing left)
//   Slot 1  screen( 200, 280) — back    Slot 1  screen(1080, 280) — back
//   Slot 0  screen( 320, 400) — front   Slot 0  screen( 960, 400) — front
//   Slot 2  screen( 200, 520) — back    Slot 2  screen(1080, 520) — back
//
// World-space equivalents (W=1280, H=720 → halfW=640, halfH=360):
//
//   PLAYER SIDE                          ENEMY SIDE
//   Slot 1  world(-440, -80)             Slot 1  world( 440, -80)
//   Slot 0  world(-320,  40)             Slot 0  world( 320,  40)
//   Slot 2  world(-440, 160)             Slot 2  world( 440, 160)
//
// Slot occupancy:
//   Each slot holds one WorldSpriteRenderer (value member, always constructed).
//   Inactive slots are guarded by mPlayerActive[]/mEnemyActive[] — never drawn.
//
// Ownership:
//   BattleState owns BattleRenderer; BattleRenderer owns all renderers.
//
// Lifetime:
//   Initialize() called from BattleState::OnEnter()
//   Shutdown()   called from BattleState::OnExit()
//
// Common mistakes:
//   1. Adding a px-to-world conversion outside this file — there must be
//      exactly ONE conversion site: Initialize(), which seeds mPlayerWorldPos[].
//   2. Forgetting flipX for enemy slots — skeletons face the wrong direction.
//   3. Calling Update() before Initialize() — mActiveClip is nullptr → crash.
// ============================================================
#pragma once
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Renderer/Camera.h"
#include "../Renderer/SpriteSheet.h"
#include "../Battle/IBattler.h"
#include "../Battle/BattleCameraController.h"
#include <d3d11.h>
#include <array>
#include <vector>
#include <string>

class BattleRenderer
{
public:
    // Maximum combatants per team.
    static constexpr int kMaxSlots = 3;

    // ----------------------------------------------------------------
    // SlotInfo: describes one combatant slot — which asset to load and
    //   which animation clip to start on.
    // ----------------------------------------------------------------
    struct SlotInfo
    {
        std::wstring texturePath;  // e.g. L"assets/animations/verso.png"
        std::string  jsonPath;     // e.g. "assets/animations/verso.json"
        std::string  startClip;    // e.g. "idle"
        bool         occupied = false;

        // World-space draw offset applied to every Draw() call for this slot.
        // Positive Y is downward; negative Y shifts the sprite upward.
        // Use to correct pivot-alignment mismatches without moving the slot
        // anchor position or editing the sprite sheet JSON.
        float drawOffsetX = 0.0f;
        float drawOffsetY = 0.0f;
    };

    BattleRenderer() = default;

    // ----------------------------------------------------------------
    // Initialize
    // Purpose:
    //   Load sprites for all occupied slots.  Each occupied slot gets its
    //   own WorldSpriteRenderer with the given texture + sheet.
    //   Converts the hardcoded screen-pixel design positions to world space
    //   ONCE here; every other method uses world coords exclusively.
    // Parameters:
    //   device/context — D3D11 device and context
    //   playerSlots    — up to kMaxSlots entries for the player team
    //   enemySlots     — up to kMaxSlots entries for the enemy team
    //   screenW/H      — render-target dimensions (for camera + conversion)
    // ----------------------------------------------------------------
    bool Initialize(ID3D11Device*                    device,
                    ID3D11DeviceContext*              context,
                    const std::array<SlotInfo, kMaxSlots>& playerSlots,
                    const std::array<SlotInfo, kMaxSlots>& enemySlots,
                    int screenW, int screenH);

    // Advance all animation timers by dt and update the battle camera lerp.
    void Update(float dt);

    // Draw all occupied slots using the current battle camera.
    void Render(ID3D11DeviceContext* context);

    // Release all GPU resources.
    void Shutdown();

    // ------------------------------------------------------------
    // SetCameraPhase: drive the BattleCameraController from BattleState.
    //
    // Parameters:
    //   phase         — OVERVIEW / ACTOR_FOCUS / TARGET_FOCUS
    //   actorSlot     — player slot index of the acting combatant (-1 = none)
    //   targetSlot    — enemy slot index of the selected target  (-1 = none)
    //
    // World positions are read directly from mPlayerWorldPos[]/mEnemyWorldPos[]
    // — no conversion is done here.
    // ------------------------------------------------------------
    void SetCameraPhase(BattleCameraPhase phase,
                        int actorSlot  = -1,
                        int targetSlot = -1);

    // ------------------------------------------------------------
    // GetPlayerSlotPos / GetEnemySlotPos:
    //   Return the WORLD-SPACE center of a given slot.
    //   Bounds-checked: returns (0,0) for invalid slot indices.
    // ------------------------------------------------------------
    void GetPlayerSlotPos(int slot, float& outWorldX, float& outWorldY) const;
    void GetEnemySlotPos (int slot, float& outWorldX, float& outWorldY) const;

private:
    // ----------------------------------------------------------------
    // Screen-pixel design positions — the human-readable source of truth.
    // These are the values from the layout diagram in the file header.
    // They are used ONLY in Initialize() to seed mPlayerWorldPos[].
    // No other method touches these constants.
    //
    // Rule: if you want to move a slot, change the value here; Initialize()
    // will recompute mPlayerWorldPos[] automatically.
    // ----------------------------------------------------------------
    static constexpr float kPlayerScreenX[kMaxSlots] = { 320.0f,  200.0f,  200.0f };
    static constexpr float kPlayerScreenY[kMaxSlots] = { 400.0f,  280.0f,  520.0f };
    static constexpr float kEnemyScreenX [kMaxSlots] = { 960.0f, 1080.0f, 1080.0f };
    static constexpr float kEnemyScreenY [kMaxSlots] = { 400.0f,  280.0f,  520.0f };

    // ----------------------------------------------------------------
    // World-space slot positions — computed once in Initialize() from the
    // screen-pixel constants above:  worldX = screenX - screenW/2.
    // Every method after Initialize() reads from here exclusively.
    // ----------------------------------------------------------------
    float mPlayerWorldX[kMaxSlots] = {};
    float mPlayerWorldY[kMaxSlots] = {};
    float mEnemyWorldX [kMaxSlots] = {};
    float mEnemyWorldY [kMaxSlots] = {};

    // One renderer per slot per team.
    WorldSpriteRenderer mPlayerRenderers[kMaxSlots];
    WorldSpriteRenderer mEnemyRenderers [kMaxSlots];

    // Which slots are occupied (have a live WorldSpriteRenderer).
    bool mPlayerActive[kMaxSlots] = { false, false, false };
    bool mEnemyActive [kMaxSlots] = { false, false, false };

    // Battle camera controller — owns Camera2D and drives OVERVIEW /
    // ACTOR_FOCUS / TARGET_FOCUS transitions with smooth lerp.
    BattleCameraController mCameraCtrl;

    int mScreenW = 1280;
    int mScreenH = 720;
};
