// ============================================================
// File: BattleRenderer.h
// Responsibility: Render all combatant sprites during a battle session.
//
// Coordinate system — ONE space used everywhere: WORLD SPACE.
//   Camera2D at pos=(0,0) zoom=1 maps world origin (0,0) to the screen center.
//   +X right, +Y down.  World unit = 1 pixel at zoom=1.
//
// Slot positions are supplied by the CALLER (BattleState) via SlotInfo::worldX/Y.
//   BattleState loads data/formations.json and resolves:
//     slotWorldX = battleCenterX + formationOffsetX
//     slotWorldY = battleCenterY + formationOffsetY
//   BattleRenderer stores these values and uses them every frame as-is.
//   No pixel-to-world conversion happens inside this file.
//
// Slot layout (read from data/formations.json, default 1280x720 battle center=(0,0)):
//
//   PLAYER SIDE (left, facing right)     ENEMY SIDE (right, facing left)
//   Slot 1  world(-440, -80) — back      Slot 1  world( 440, -80) — back
//   Slot 0  world(-320,  40) — front     Slot 0  world( 320,  40) — front
//   Slot 2  world(-440, 160) — back      Slot 2  world( 440, 160) — back
//
//   Pivot is BOTTOM-CENTER: the slot Y is where the character's feet touch the
//   ground.  No draw offset correction is needed for correctly authored sprites.
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
//   1. Converting screen coords inside this file — world positions come in via
//      SlotInfo::worldX/Y; any inline conversion here breaks the single-source rule.
//   2. Forgetting flipX for enemy slots — skeletons face the wrong direction.
//   3. Calling Update() before Initialize() — mActiveClip is nullptr → crash.
//   4. Setting drawOffsetY to correct a bottom-center pivot — use the formation
//      offsets instead; the pivot already lands feet at worldY correctly.
// ============================================================
#pragma once
#include "../Scene/SceneGraph.h"
#include "BattleCombatantSprite.h"
#include "../Renderer/Camera.h"
#include "../Renderer/SpriteSheet.h"
#include "../Battle/IBattler.h"
#include "../Battle/BattleCameraController.h"
#include "../Battle/CombatantAnim.h"
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

        // World-space position of this slot's anchor (the character's feet).
        // Set by the caller (BattleState) from the loaded FormationData:
        //   worldX = battleCenterX + formation.offsetX
        //   worldY = battleCenterY + formation.offsetY
        // With a correct bottom-center pivot, the sprite's feet land exactly
        // at (worldX, worldY) — no draw offset correction is required.
        float worldX = 0.0f;
        float worldY = 0.0f;

        // Optional fine-tuning offset applied on top of worldX/worldY in Draw().
        // Should be (0, 0) for any sprite whose JSON pivot is already at the feet.
        // Non-zero only when a legacy sprite has its pivot at an incorrect location
        // and re-authoring the JSON is not immediately practical.
        float drawOffsetX = 0.0f;
        float drawOffsetY = 0.0f;

        // World-space offset added to worldX/worldY when this slot becomes the
        // camera focus target (ACTOR_FOCUS or TARGET_FOCUS).
        //
        // Because the slot anchor is the CHARACTER'S FEET (bottom-center pivot),
        // centering the camera on it places the feet at the screen center, which
        // looks wrong — the character appears to be sinking below mid-screen.
        //
        // Set cameraFocusOffsetY to a NEGATIVE value to shift the focal point
        // upward to the visual center of the sprite:
        //   offset = -(frameHeight * renderScale) / 2
        //   e.g. for a 128px frame at scale 2.0:  -(128 * 2) / 2 = -128
        //
        // cameraFocusOffsetX is available for sprites that are not horizontally
        // centered (e.g., a character holding a weapon to one side).
        float cameraFocusOffsetX = 0.0f;
        float cameraFocusOffsetY = 0.0f;

        // Per-role animation clip name overrides.  Index with
        //   static_cast<int>(CombatantAnim::X)
        // Leave a slot empty to use DefaultClipName(X) for that role.
        // BattleRenderer seeds its internal per-slot clip table from these
        // during Initialize() so all subsequent PlayEnemyClip / PlayPlayerClip
        // calls resolve instantly without touching SlotInfo again.
        std::string clipOverrides[kCombatantAnimCount];
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

    Camera2D& GetCamera() { return mCameraCtrl.GetCamera(); }
    const Camera2D& GetCamera() const { return const_cast<BattleCameraController&>(mCameraCtrl).GetCamera(); }

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
    // PlayEnemyClip / PlayPlayerClip:
    //   Request a standard animation role on a specific slot.
    //   The role is resolved to a clip name using the per-slot override
    //   table seeded in Initialize() (or DefaultClipName if no override).
    //
    //   If the character's sprite sheet does not contain that clip,
    //   WorldSpriteRenderer logs a warning and no-ops — no crash.
    //
    //   Out-of-range slots and inactive slots are silently ignored.
    // ------------------------------------------------------------
    void PlayEnemyClip (int slot, CombatantAnim anim);
    void PlayPlayerClip(int slot, CombatantAnim anim);

    // ------------------------------------------------------------
    // AreAllDeathAnimsDone:
    //   Returns true when every active combatant slot on BOTH sides has
    //   a clip that IsClipDone() == true.
    //
    //   Why check both sides?
    //     For VICTORY the enemy die clips are what we wait on; for DEFEAT
    //     the player die clips.  Checking all slots is safe because alive
    //     combatants play looping "idle" which IsClipDone() always reports
    //     as done — only the TARGET non-looping die clip blocks the wait.
    //
    //   Special case: if a character has no "die" clip, PlayClip returns false
    //     and PlayEnemyClip/PlayPlayerClip calls FreezeCurrentFrame() so the
    //     combatant holds its last visible pose.  IsClipDone() returns true
    //     immediately (frozen = done), so the iris does NOT stall.
    // ------------------------------------------------------------
    bool AreAllDeathAnimsDone() const;
    bool IsEnemyClipDone(int slot) const;

    // ------------------------------------------------------------
    // GetPlayerSlotPos / GetEnemySlotPos:
    //   Return the WORLD-SPACE center of a given slot.
    //   Bounds-checked: returns (0,0) for invalid slot indices.
    // ------------------------------------------------------------
    void GetPlayerSlotPos(int slot, float& outWorldX, float& outWorldY) const;
    void GetEnemySlotPos (int slot, float& outWorldX, float& outWorldY) const;

private:
    // ----------------------------------------------------------------
    // World-space slot positions — copied directly from SlotInfo::worldX/Y
    // in Initialize().  Set by the caller from formation offset data so
    // that no pixel-to-world conversion exists inside this file.
    // Every method after Initialize() reads from here exclusively.
    // ----------------------------------------------------------------
    float mPlayerWorldX[kMaxSlots] = {};
    float mPlayerWorldY[kMaxSlots] = {};
    float mEnemyWorldX [kMaxSlots] = {};
    float mEnemyWorldY [kMaxSlots] = {};

    // Per-slot camera focus offsets — added to the slot world position when
    // the slot is passed to BattleCameraController::SetActorPos/SetTargetPos.
    // Seeded from SlotInfo::cameraFocusOffsetX/Y in Initialize().
    // Zero by default: no adjustment for slots whose anchor is already at the
    // visual center.  Set to -(frameH * scale)/2 for bottom-center pivots.
    float mPlayerCamOffX[kMaxSlots] = {};
    float mPlayerCamOffY[kMaxSlots] = {};
    float mEnemyCamOffX [kMaxSlots] = {};
    float mEnemyCamOffY [kMaxSlots] = {};

    SceneGraph mScene;

    // Non-owning pointers. SceneGraph owns the instances.
    BattleCombatantSprite* mPlayerSprites[kMaxSlots] = {nullptr};
    BattleCombatantSprite* mEnemySprites [kMaxSlots] = {nullptr};

    // Which slots are occupied.
    bool mPlayerActive[kMaxSlots] = { false, false, false };
    bool mEnemyActive [kMaxSlots] = { false, false, false };

    // ----------------------------------------------------------------
    // Per-slot clip name table — resolved during Initialize().
    //
    // mEnemyClipNames[slot][animIdx] holds the actual clip name string
    // to pass to WorldSpriteRenderer::PlayClip when PlayEnemyClip(slot, anim)
    // is called.  Seeded from SlotInfo::clipOverrides; falls back to
    // DefaultClipName(anim) if the override is empty.
    //
    // Storing resolved strings avoids repeating the override/default logic
    // every time PlayEnemyClip is called at runtime.
    // ----------------------------------------------------------------
    std::string mPlayerClipNames[kMaxSlots][kCombatantAnimCount];
    std::string mEnemyClipNames [kMaxSlots][kCombatantAnimCount];

    // Battle camera controller — owns Camera2D and drives OVERVIEW /
    // ACTOR_FOCUS / TARGET_FOCUS transitions with smooth lerp.
    BattleCameraController mCameraCtrl;

    int mScreenW = 1280;
    int mScreenH = 720;
};
