// ============================================================
// File: EnemyEncounterData.h
// Responsibility: Plain data package that bridges overworld enemies to
//                 the turn-based battle system.
//
// How it flows:
//   OverworldEnemy owns EnemyEncounterData mData loaded from data/enemies/*.json.
//   When the player starts combat, the active overworld state copies this data
//   into BattleState, and BattleState builds enemy slots from it.
//
// Why a plain struct:
//   Encounter data is read-only value semantics. It is loaded once from JSON,
//   copied into BattleState, and does not need virtual dispatch or ownership
//   logic. The caller's copy remains independent of the OverworldEnemy lifetime.
//
// Fields:
//   texturePath        - wide-string path expected by WICTextureLoader.
//   jsonPath           - sprite-sheet JSON path loaded by JsonLoader.
//   idleClip           - animation clip name to start with.
//   contactRadius      - world-space radius used by proximity prompts.
//   cameraFocusOffsetY - battle camera focus offset from feet to chest.
// ============================================================
#pragma once

#include <string>
#include <vector>

// ============================================================
// EnemySlotData: one enemy combatant inside a battle party.
//
// A battleParty can contain 1-3 of these. Each slot maps to one
// BattleRenderer::SlotInfo entry and one EnemyCombatant inside BattleManager.
// The overworld entity can use a different sprite than the battle party.
// ============================================================
struct EnemySlotData
{
    // ---- Sprite ----
    std::wstring texturePath;           // L"assets/animations/skeleton.png"
    std::string  jsonPath;              // "assets/animations/skeleton.json"
    std::string  idleClip;              // "idle"
    std::wstring turnViewPath;          // L"assets/UI/turn-view-skeleton.png"

    // ---- Per-role animation clip name overrides ----
    // These map to CombatantAnim roles. Leave empty to use DefaultClipName().
    // If the named clip is absent from the sheet, WorldSpriteRenderer logs a
    // warning and no-ops, so the character freezes on its current frame.
    std::string dieClip;     // CombatantAnim::Die    (default: "die")
    std::string attackClip;  // CombatantAnim::Attack (default: "attack-1")
    std::string walkClip;    // CombatantAnim::Walk   (default: "walk")
    std::string hurtClip;    // CombatantAnim::Hurt   (default: "hurt")

    // ---- Battle stats ----
    int hp  = 0;
    int atk = 0;
    int def = 0;
    int spd = 0;

    std::string attackJsonPath = "data/skills/skeleton_attack.json";

    // ---- Rewards ----
    int expReward = 0;
    int coinReward = 0;

    // ---- Battle camera ----
    // Formula: -(frameHeight * renderScale) / 2.
    // Example: 128px sprite at scale 2 -> -128.
    float cameraFocusOffsetY = -128.0f;
};

// ============================================================
// EnemyEncounterData: data bridge from an overworld enemy into BattleState.
//
// Top-level fields describe the overworld representation. battleParty defines
// who appears in combat. A single overworld icon can trigger a group encounter.
// ============================================================
struct EnemyEncounterData
{
    // ---- Overworld identity + sprite ----
    std::string  name;             // display name fallback
    std::string  nameKey;          // localization key; falls back to name
    std::wstring texturePath;      // overworld sprite
    std::string  jsonPath;         // overworld sprite sheet JSON
    std::string  idleClip;         // overworld idle clip name

    // ---- Overworld collision ----
    float contactRadius = 80.0f;

    // ---- Battle environment ----
    std::string environmentPath;

    // ---- Battle BGM ----
    // Track id from data/audio/bgm.json. If empty, BattleState uses the
    // default battle theme event.
    std::string bgmTrackId;

    // ---- Result BGM ----
    // Track ids from data/audio/bgm.json. If empty, BattleState uses the
    // defaults from data/battle_result_layout.json. These fields let an
    // encounter pair its fight theme with matching victory/defeat music
    // without hardcoding soundtrack ids in C++.
    std::string victoryBgmTrackId;
    std::string defeatBgmTrackId;

    // ---- Battle party ----
    // battleParty[0] = front slot, [1] = back-top, [2] = back-bottom.
    // Slots beyond index 2 are ignored by BattleRenderer.
    std::vector<EnemySlotData> battleParty;
};
