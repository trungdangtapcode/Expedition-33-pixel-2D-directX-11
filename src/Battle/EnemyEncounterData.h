// ============================================================
// File: EnemyEncounterData.h
// Responsibility: Plain data package that bridges overworld enemies to
//                the turn-based battle system.
//
// How it flows:
//   OverworldEnemy (PlayState / SceneGraph)
//     └─ owns EnemyEncounterData mData  (loaded once from data/enemies/*.json)
//
//   When player touches the enemy and presses B:
//     PlayState reads enemy.GetEncounterData()
//     → passes it as BattleState constructor argument
//     → BattleState fills enemySlots[0] from this struct instead of hardcoded paths
//
// Why a plain struct (no methods)?
//   Encounter data is read-only value semantics: constructed once from JSON,
//   copied into BattleState.  No virtual dispatch or lifetime management needed.
//   The struct is copyable by value so the caller's copy is independent of the
//   OverworldEnemy lifetime.
//
// Fields:
//   texturePath     — wide-string path expected by WICTextureLoader / DDSTextureLoader
//   jsonPath        — sprite-sheet JSON path loaded by JsonLoader::LoadSpriteSheet
//   idleClip        — animation clip name to start with ("idle")
//   contactRadius   — world-space pixel radius used by OverworldEnemy::IsPlayerNearby
//   cameraFocusOffsetY — passed to BattleRenderer SlotInfo; shifts camera focal point
//                        from feet to chest. Typically -(frameHeight * scale / 2).
// ============================================================
#pragma once
#include <string>
#include <vector>

// ============================================================
// EnemySlotData: one enemy combatant inside a battle party.
//
// A battleParty can contain 1–3 of these (kMaxSlots = 3).
// Each slot maps 1:1 to a BattleRenderer::SlotInfo entry and
// one EnemyCombatant inside BattleManager.
//
// The overworld entity may have a completely different sprite
// than what appears in battle — e.g., a tiny scout icon in the
// overworld can trigger a fight with a full dragon party.
// ============================================================
struct EnemySlotData
{
    // ---- Sprite ----
    std::wstring texturePath;           // L"assets/animations/skeleton.png"
    std::string  jsonPath;              // "assets/animations/skeleton.json"
    std::string  idleClip;             // "idle"

    // ---- Per-role animation clip name overrides ----
    // These map to CombatantAnim roles.  Leave empty to use DefaultClipName().
    // Example: dieClip = "death" if the sprite sheet uses that name instead.
    // If the named clip is absent from the sheet, WorldSpriteRenderer logs a
    // warning and no-ops — the character freezes on its current frame.
    std::string dieClip;     // CombatantAnim::Die    (default: "die")
    std::string attackClip;  // CombatantAnim::Attack (default: "attack-1")
    std::string walkClip;    // CombatantAnim::Walk   (default: "walk")
    std::string hurtClip;    // CombatantAnim::Hurt   (default: "hurt")

    // ---- Battle stats ----
    // ---- Validation ----
    int hp  = 0;
    int atk = 0;
    int def = 0;
    int spd = 0;

    std::string attackJsonPath = "data/skills/skeleton_attack.json";

    // ---- Battle camera ----
    // Lifts ACTOR_FOCUS / TARGET_FOCUS from feet-anchor to chest.
    // Formula: -(frameHeight × renderScale) / 2.
    // Example: 128px sprite at scale 2 → -128.
    float cameraFocusOffsetY = -128.0f;
};

// ============================================================
// EnemyEncounterData: the data bridge that travels from an
// OverworldEnemy (PlayState) into BattleState when the player
// initiates combat.
//
// Top-level fields describe the OVERWORLD representation —
// these are used by OverworldEnemy's WorldSpriteRenderer.
//
// battleParty defines who actually appears IN BATTLE (1–3 slots).
// The overworld sprite and the battle party do NOT need to match:
// a single overworld icon can trigger a group encounter.
//
// If battleParty is empty (legacy / debug push), BattleState will
// fall back to a hardcoded skeleton so nothing crashes.
// ============================================================
struct EnemyEncounterData
{
    // ---- Overworld identity + sprite ----
    std::string  name;             // display name (e.g. "Skeleton Scout")
    std::wstring texturePath;      // overworld sprite  L"assets/animations/skeleton.png"
    std::string  jsonPath;         // overworld sprite sheet JSON
    std::string  idleClip;         // overworld idle clip name

    // ---- Overworld collision ----
    // Pixel radius around the enemy anchor that triggers the proximity prompt.
    // Typical value: 80–120 world pixels.
    float contactRadius = 80.0f;

    // ---- Battle party (1–3 slots) ----
    // Defines every enemy combatant that appears when this encounter starts.
    // battleParty[0] = front slot, [1] = back-top, [2] = back-bottom.
    // Slots beyond index 2 are ignored (BattleRenderer::kMaxSlots == 3).
    std::vector<EnemySlotData> battleParty;
};
