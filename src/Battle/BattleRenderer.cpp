// ============================================================
// File: BattleRenderer.cpp
// Responsibility: Initialize, animate, and render all combatant sprites
//   during a battle — 3 player slots (left side) and 3 enemy slots (right side).
//   Drives the BattleCameraController for cinematic phase transitions.
//
// Coordinate system — ONE space used everywhere: WORLD SPACE.
//   Camera2D at pos=(0,0) zoom=1 maps world origin (0,0) to the screen center.
//   +X right, +Y down.  World unit = 1 pixel at zoom=1.
//
//   Slot positions arrive as-is from SlotInfo::worldX/Y, pre-computed by
//   BattleState from data/formations.json:
//     slotWorldX = battleCenterX + formationOffsetX
//     slotWorldY = battleCenterY + formationOffsetY
//   BattleRenderer stores them directly — no arithmetic is performed here.
//
// World-space slot layout (default: battle center=(0,0), 1280x720 screen):
//
//   PLAYER SIDE (left, face right)     ENEMY SIDE (right, face left)
//   Slot 1  world(-440, -80)  back     Slot 1  world( 440, -80)  back
//   Slot 0  world(-320,  40)  front    Slot 0  world( 320,  40)  front
//   Slot 2  world(-440, 160)  back     Slot 2  world( 440, 160)  back
//
// Camera phase transitions (driven by BattleState):
//   OVERVIEW      — default wide shot, pos=(0,0), zoom=1.0
//   ACTOR_FOCUS   — zoom to acting player slot,   zoom=1.6
//   TARGET_FOCUS  — pan to 80% target + 20% actor blend, zoom=1.0
//
// Common mistakes:
//   1. Computing screen-to-world inside this file — positions come in via
//      SlotInfo::worldX/Y; no conversion belongs here.
//   2. Forgetting flipX for enemy slots — skeletons face the wrong direction.
//   3. Calling Update() before Initialize() — mActiveClip is nullptr -> crash.
//   4. Not calling SetCameraPhase() on input phase change — camera stays stuck.
// ============================================================
#include "BattleRenderer.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   For each occupied slot, load the SpriteSheet JSON, then call
//   WorldSpriteRenderer::Initialize() to upload the texture to the GPU.
//   Immediately play the start clip so the sprite has a valid frame on the
//   first Render() call.
//
//   ALSO computes world-space slot positions from the screen-pixel design
//   constants defined in BattleRenderer.h.  This conversion happens EXACTLY
//   ONCE here — worldX = screenX - screenW/2.  All other methods in this
//   file read mPlayerWorldX/Y[] and mEnemyWorldX/Y[] directly, with no
//   further arithmetic needed.
//
// Parameters:
//   device/context — D3D11 objects passed to each WorldSpriteRenderer
//   playerSlots    — array of kMaxSlots slot descriptors for the player team
//   enemySlots     — array of kMaxSlots slot descriptors for the enemy team
//   screenW/H      — render-target size; used to seed world coords + camera
// ------------------------------------------------------------
bool BattleRenderer::Initialize(ID3D11Device*                          device,
                                 ID3D11DeviceContext*                   context,
                                 const std::array<SlotInfo, kMaxSlots>& playerSlots,
                                 const std::array<SlotInfo, kMaxSlots>& enemySlots,
                                 int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;

    // ----------------------------------------------------------------
    // Copy world-space slot positions and camera focus offsets from SlotInfo.
    //
    // World positions are pre-computed by the caller (BattleState):
    //   slotWorldX = battleCenterX + formationOffsetX   (from formations.json)
    //   slotWorldY = battleCenterY + formationOffsetY
    //
    // Camera focus offsets shift the focal point from the ground anchor (feet)
    // to the visual center of the sprite during ACTOR/TARGET_FOCUS phases.
    // They are applied only in SetCameraPhase() — Draw() is unaffected.
    // ----------------------------------------------------------------
    for (int i = 0; i < kMaxSlots; ++i)
    {
        mPlayerWorldX[i]  = playerSlots[i].worldX;
        mPlayerWorldY[i]  = playerSlots[i].worldY;
        mPlayerCamOffX[i] = playerSlots[i].cameraFocusOffsetX;
        mPlayerCamOffY[i] = playerSlots[i].cameraFocusOffsetY;

        mEnemyWorldX [i]  = enemySlots [i].worldX;
        mEnemyWorldY [i]  = enemySlots [i].worldY;
        mEnemyCamOffX[i]  = enemySlots [i].cameraFocusOffsetX;
        mEnemyCamOffY[i]  = enemySlots [i].cameraFocusOffsetY;
    }

    // ----------------------------------------------------------------
    // Seed per-slot animation clip name tables.
    //
    // For each slot × each CombatantAnim role: use SlotInfo::clipOverrides[role]
    // if non-empty, otherwise fall back to DefaultClipName(role).
    //
    // This is done unconditionally for all slots (occupied or not); inactive
    // slots will never have PlayEnemyClip/PlayPlayerClip called on them, so
    // storing the strings costs negligible RAM and avoids a special-case check.
    // ----------------------------------------------------------------
    for (int i = 0; i < kMaxSlots; ++i)
    {
        for (int a = 0; a < kCombatantAnimCount; ++a)
        {
            const CombatantAnim role = static_cast<CombatantAnim>(a);

            const std::string& pOvr = playerSlots[i].clipOverrides[a];
            mPlayerClipNames[i][a] = pOvr.empty() ? DefaultClipName(role) : pOvr;

            const std::string& eOvr = enemySlots[i].clipOverrides[a];
            mEnemyClipNames[i][a]  = eOvr.empty() ? DefaultClipName(role) : eOvr;
        }
    }

    // ----------------------------------------------------------------
    // Initialize the battle camera controller.
    // It starts in OVERVIEW phase: pos=(0,0), zoom=1.0 — all combatants visible.
    // BattleState will call SetCameraPhase() to drive transitions later.
    // ----------------------------------------------------------------
    mCameraCtrl.Initialize(screenW, screenH);

    // ----------------------------------------------------------------
    // Initialize player-side renderers.
    // ----------------------------------------------------------------
    for (int i = 0; i < kMaxSlots; ++i)
    {
        const SlotInfo& info = playerSlots[i];
        if (!info.occupied) { mPlayerActive[i] = false; continue; }

        // Load the SpriteSheet descriptor (frame size, clip list, pivots).
        SpriteSheet sheet;
        if (!JsonLoader::LoadSpriteSheet(info.jsonPath, sheet))
        {
            LOG("[BattleRenderer] Failed to load player sheet: %s", info.jsonPath.c_str());
            return false;
        }

        // Upload PNG to GPU and create SpriteBatch + D3D states.
        if (!mPlayerRenderers[i].Initialize(device, context, info.texturePath, sheet))
        {
            LOG("[BattleRenderer] Failed to init player renderer slot %d", i);
            return false;
        }

        // Start the idle (or configured start) clip immediately so the first
        // Render() call has a valid mActiveClip and does not assert.
        mPlayerRenderers[i].PlayClip(info.startClip);

        // Apply the per-slot draw offset (e.g. to correct bottom-center pivot).
        // Zero by default — set explicitly in SlotInfo when alignment needs tuning.
        mPlayerRenderers[i].SetDrawOffset(info.drawOffsetX, info.drawOffsetY);
        mPlayerActive[i] = true;
    }

    // ----------------------------------------------------------------
    // Initialize enemy-side renderers.
    // Exactly the same pattern — only the active flag and renderer array differ.
    // ----------------------------------------------------------------
    for (int i = 0; i < kMaxSlots; ++i)
    {
        const SlotInfo& info = enemySlots[i];
        if (!info.occupied) { mEnemyActive[i] = false; continue; }

        SpriteSheet sheet;
        if (!JsonLoader::LoadSpriteSheet(info.jsonPath, sheet))
        {
            LOG("[BattleRenderer] Failed to load enemy sheet: %s", info.jsonPath.c_str());
            return false;
        }

        if (!mEnemyRenderers[i].Initialize(device, context, info.texturePath, sheet))
        {
            LOG("[BattleRenderer] Failed to init enemy renderer slot %d", i);
            return false;
        }

        mEnemyRenderers[i].PlayClip(info.startClip);
        mEnemyRenderers[i].SetDrawOffset(info.drawOffsetX, info.drawOffsetY);
        mEnemyActive[i] = true;
    }

    LOG("[BattleRenderer] Initialized — screen %dx%d", screenW, screenH);
    return true;
}

// ------------------------------------------------------------
// Function: PlayEnemyClip / PlayPlayerClip
// Purpose:
//   Drive a standard animation role on a specific combatant slot.
//   Looks up the resolved clip name in the per-slot table seeded in
//   Initialize(), then delegates to WorldSpriteRenderer::PlayClip.
//
// Die-clip fallback:
//   If PlayClip() returns false (clip absent from sprite sheet) AND the
//   requested role is CombatantAnim::Die, FreezeCurrentFrame() is called.
//   This holds the combatant on its last visible pose while the iris closes,
//   instead of leaving it looping in idle — which would look alive.
//   Without this, a missing die clip causes IsClipDone() to return true
//   immediately (idle is looping), so the iris starts the same frame the
//   enemy dies, and the enemy appears to vanish instantly.
//
// Graceful no-op paths:
//   • slot out-of-range or slot not active  → silently ignored.
//   • clip name absent in the sprite sheet for non-Die roles
//       → WorldSpriteRenderer logs a warning; active clip unchanged.
//
// Why not pass a string directly?
//   All callers (BattleState death detection, AttackAction, etc.) use the
//   CombatantAnim enum to stay decoupled from per-character naming.
//   The per-slot override table handles any name differences transparently.
// ------------------------------------------------------------
void BattleRenderer::PlayEnemyClip(int slot, CombatantAnim anim)
{
    // Guard: ignore out-of-range or uninitialized slots.
    if (slot < 0 || slot >= kMaxSlots || !mEnemyActive[slot]) return;

    const std::string& clipName = mEnemyClipNames[slot][static_cast<int>(anim)];
    const bool found = mEnemyRenderers[slot].PlayClip(clipName);

    // If the die clip is missing from the sprite sheet, freeze the sprite on
    // its current frame.  This stops idle looping so the combatant looks dead
    // and IsClipDone() returns true, allowing the iris to proceed.
    if (!found && anim == CombatantAnim::Die)
    {
        mEnemyRenderers[slot].FreezeCurrentFrame();
        LOG("[BattleRenderer] Enemy slot %d: die clip '%s' not found — frame frozen.",
            slot, clipName.c_str());
    }
}

void BattleRenderer::PlayPlayerClip(int slot, CombatantAnim anim)
{
    // Guard: ignore out-of-range or uninitialized slots.
    if (slot < 0 || slot >= kMaxSlots || !mPlayerActive[slot]) return;

    const std::string& clipName = mPlayerClipNames[slot][static_cast<int>(anim)];
    const bool found = mPlayerRenderers[slot].PlayClip(clipName);

    // Same freeze-on-missing-die logic as PlayEnemyClip.
    if (!found && anim == CombatantAnim::Die)
    {
        mPlayerRenderers[slot].FreezeCurrentFrame();
        LOG("[BattleRenderer] Player slot %d: die clip '%s' not found — frame frozen.",
            slot, clipName.c_str());
    }
}

// ------------------------------------------------------------
// Function: AreAllDeathAnimsDone
// Purpose:
//   Iterate every active slot on both sides.  Return false if ANY active
//   slot has a non-looping clip still in progress (IsClipDone() == false).
//
// Looping clips ("idle", "walk") always return IsClipDone() == true, so
// only a death/attack non-looping clip can block this check.
//
// Called by BattleState each frame while mWaitingForDeathAnims == true.
// Once this returns true, BattleState starts the iris close.
// ------------------------------------------------------------
bool BattleRenderer::AreAllDeathAnimsDone() const
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (mEnemyActive[i]  && !mEnemyRenderers[i].IsClipDone())  return false;
        if (mPlayerActive[i] && !mPlayerRenderers[i].IsClipDone()) return false;
    }
    return true;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance the animation frame timer for every active slot.
//   Must be called once per frame, before Render().
//
// Why update inactive slots is skipped:
//   WorldSpriteRenderer::Update() dereferences mActiveClip; calling it
//   on an uninitialized renderer that never had PlayClip() called is UB.
// ------------------------------------------------------------
void BattleRenderer::Update(float dt)
{
    // Advance the battle camera lerp toward the desired phase target.
    // This also calls Camera2D::Update() internally to rebuild the matrix.
    mCameraCtrl.Update(dt);

    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (mPlayerActive[i]) mPlayerRenderers[i].Update(dt);
        if (mEnemyActive[i])  mEnemyRenderers[i].Update(dt);
    }
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw all active combatant sprites at their slot positions.
//   Player sprites face right (flipX=false); enemy sprites face left (flipX=true).
//
//   Slot positions are read directly from mPlayerWorldX/Y[] and mEnemyWorldX/Y[]
//   — both arrays are already in world space (computed once in Initialize()).
//   No pixel-to-world conversion is needed here.
// ------------------------------------------------------------
void BattleRenderer::Render(ID3D11DeviceContext* context)
{
    // Retrieve the interpolated camera from the controller.
    // The matrix was already rebuilt in Update() — no redundant rebuild here.
    Camera2D& cam = mCameraCtrl.GetCamera();

    // -- Player side ------------------------------------------------
    // Players face right (default sprite orientation) — flipX=false.
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mPlayerActive[i]) continue;
        mPlayerRenderers[i].Draw(
            context,
            cam,
            mPlayerWorldX[i],   // world x — no conversion needed
            mPlayerWorldY[i],   // world y — no conversion needed
            2.0f,               // scale up for visibility
            false               // face right
        );
    }

    // -- Enemy side -------------------------------------------------
    // Enemies face left (flipX=true) so they face the player side.
    for (int i = 0; i < kMaxSlots; ++i)
    {
        if (!mEnemyActive[i]) continue;
        mEnemyRenderers[i].Draw(
            context,
            cam,
            mEnemyWorldX[i],    // world x — no conversion needed
            mEnemyWorldY[i],    // world y — no conversion needed
            2.0f,               // scale up for visibility
            true                // face left — mirror the sprite
        );
    }
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Release all GPU resources owned by each active WorldSpriteRenderer.
//   Renderer slots that were never initialized (inactive) are safe to call
//   Shutdown() on — WorldSpriteRenderer::Shutdown() is a no-op in that case.
// ------------------------------------------------------------
void BattleRenderer::Shutdown()
{
    for (int i = 0; i < kMaxSlots; ++i)
    {
        mPlayerRenderers[i].Shutdown();
        mEnemyRenderers[i].Shutdown();
    }

    // BattleCameraController owns its Camera2D via unique_ptr;
    // it will be destroyed automatically when mCameraCtrl goes out of scope.
    // No explicit reset needed here.

    LOG("[BattleRenderer] Shutdown complete.");
}

// ------------------------------------------------------------
// Function: SetCameraPhase
// Purpose:
//   Drive the BattleCameraController to a new cinematic state.
//   World-space slot positions are read directly from mPlayerWorldX/Y[] and
//   mEnemyWorldX/Y[] — no conversion is performed here.
//
// Parameters:
//   phase      — OVERVIEW / ACTOR_FOCUS / TARGET_FOCUS
//   actorSlot  — player slot index for the acting combatant (-1 = unknown)
//   targetSlot — enemy  slot index for the selected target  (-1 = unknown)
// ------------------------------------------------------------
void BattleRenderer::SetCameraPhase(BattleCameraPhase phase,
                                     int actorSlot, int targetSlot)
{
    // Update actor world position if a valid player slot was provided.
    // Apply cameraFocusOffsetX/Y so the camera centers on the sprite's visual
    // midpoint rather than its feet (the raw slot anchor).
    if (actorSlot >= 0 && actorSlot < kMaxSlots && mPlayerActive[actorSlot])
    {
        mCameraCtrl.SetActorPos(
            mPlayerWorldX[actorSlot] + mPlayerCamOffX[actorSlot],
            mPlayerWorldY[actorSlot] + mPlayerCamOffY[actorSlot]
        );
    }

    // Update target world position if a valid enemy slot was provided.
    // Same offset logic: enemies are also anchored at their feet.
    if (targetSlot >= 0 && targetSlot < kMaxSlots && mEnemyActive[targetSlot])
    {
        mCameraCtrl.SetTargetPos(
            mEnemyWorldX[targetSlot] + mEnemyCamOffX[targetSlot],
            mEnemyWorldY[targetSlot] + mEnemyCamOffY[targetSlot]
        );
    }

    mCameraCtrl.SetPhase(phase);
}

// ------------------------------------------------------------
// Function: GetPlayerSlotPos / GetEnemySlotPos
// Purpose:
//   Return the WORLD-SPACE center of a given slot so external callers
//   (e.g., BattleState building debug overlay labels) never have to compute
//   world coords themselves.
//   Returns (0.0f, 0.0f) for invalid slot indices — safe default.
// ------------------------------------------------------------
void BattleRenderer::GetPlayerSlotPos(int slot, float& outWorldX, float& outWorldY) const
{
    if (slot < 0 || slot >= kMaxSlots) { outWorldX = 0.0f; outWorldY = 0.0f; return; }
    outWorldX = mPlayerWorldX[slot];
    outWorldY = mPlayerWorldY[slot];
}

void BattleRenderer::GetEnemySlotPos(int slot, float& outWorldX, float& outWorldY) const
{
    if (slot < 0 || slot >= kMaxSlots) { outWorldX = 0.0f; outWorldY = 0.0f; return; }
    outWorldX = mEnemyWorldX[slot];
    outWorldY = mEnemyWorldY[slot];
}
