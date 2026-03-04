// ============================================================
// File: BattleRenderer.h
// Responsibility: Render all combatant sprites during a battle session.
//
// Screen layout (1280x720) — staggered diagonal formation:
//
//   PLAYER SIDE (left, facing right)    ENEMY SIDE (right, facing left)
//   Slot 1  screen( 200, 280) — back    Slot 1  screen(1080, 280) — back
//   Slot 0  screen( 320, 400) — front   Slot 0  screen( 960, 400) — front
//   Slot 2  screen( 200, 520) — back    Slot 2  screen(1080, 520) — back
//
//   Enemy sprites are drawn with flipX=true so they face the players.
//
// Camera coordinate conversion:
//   Camera2D at pos=(0,0) zoom=1 maps world origin (0,0) to the SCREEN CENTER.
//   To hit screen pixel (px, py) the world coordinate must be:
//     worldX = px - screenW/2
//     worldY = py - screenH/2
//   Render() performs this conversion automatically using mScreenW/H.
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
//   1. Passing raw screen pixels as world coords — sprite appears at wrong pos.
//      Always subtract (screenW/2, screenH/2) from pixel coords for world space.
//   2. Forgetting flipX for enemy slots — skeletons face the wrong direction.
//   3. Calling Update() before Initialize() — mActiveClip is nullptr → crash.
// ============================================================
#pragma once
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Renderer/Camera.h"
#include "../Renderer/SpriteSheet.h"
#include "../Battle/IBattler.h"
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
    };

    BattleRenderer() = default;

    // ----------------------------------------------------------------
    // Initialize
    // Purpose:
    //   Load sprites for all occupied slots.  Each occupied slot gets its
    //   own WorldSpriteRenderer with the given texture + sheet.
    // Parameters:
    //   device/context — D3D11 device and context
    //   playerSlots    — up to kMaxSlots entries for the player team
    //   enemySlots     — up to kMaxSlots entries for the enemy team
    //   screenW/H      — render-target dimensions (for camera + viewport)
    // ----------------------------------------------------------------
    bool Initialize(ID3D11Device*                    device,
                    ID3D11DeviceContext*              context,
                    const std::array<SlotInfo, kMaxSlots>& playerSlots,
                    const std::array<SlotInfo, kMaxSlots>& enemySlots,
                    int screenW, int screenH);

    // Advance all animation timers by dt.
    void Update(float dt);

    // Draw all occupied slots at their fixed screen positions.
    void Render(ID3D11DeviceContext* context);

    // Release all GPU resources.
    void Shutdown();

private:
    // ----------------------------------------------------------------
    // Fixed screen-pixel positions for each slot.
    // Staggered diagonal formation — front slot is closest to screen center,
    // back slots are further from center and vertically spread.
    //
    // These are SCREEN PIXELS, not world coords.
    // Render() converts them to world space: worldX = screenX - screenW/2
    //                                        worldY = screenY - screenH/2
    //
    // Player slots: face right (flipX=false), left half of screen.
    //   Slot 0 = front  (320, 400)
    //   Slot 1 = back-top (200, 280)
    //   Slot 2 = back-bot (200, 520)
    //
    // Enemy slots: face left (flipX=true), right half of screen.
    //   Slot 0 = front  (960, 400)
    //   Slot 1 = back-top (1080, 280)
    //   Slot 2 = back-bot (1080, 520)
    // ----------------------------------------------------------------
    static constexpr float kPlayerScreenX[kMaxSlots] = { 320.0f, 200.0f, 200.0f };
    static constexpr float kPlayerScreenY[kMaxSlots] = { 400.0f, 280.0f, 520.0f };
    static constexpr float kEnemyScreenX [kMaxSlots] = { 960.0f, 1080.0f, 1080.0f };
    static constexpr float kEnemyScreenY [kMaxSlots] = { 400.0f,  280.0f,  520.0f };

    // One renderer per slot per team.
    // Index matches slot index (0=center, 1=above, 2=below).
    WorldSpriteRenderer mPlayerRenderers[kMaxSlots];
    WorldSpriteRenderer mEnemyRenderers [kMaxSlots];

    // Which slots are occupied (have a live WorldSpriteRenderer).
    bool mPlayerActive[kMaxSlots] = { false, false, false };
    bool mEnemyActive [kMaxSlots] = { false, false, false };

    // Flat Camera2D — zoom=1, no pan, just maps pixels to NDC.
    // Required by WorldSpriteRenderer::Draw() for the view matrix.
    std::unique_ptr<Camera2D> mCamera;

    int mScreenW = 1280;
    int mScreenH = 720;
};
