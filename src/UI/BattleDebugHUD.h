// ============================================================
// File: BattleDebugHUD.h
// Responsibility: Format and emit a battle-state snapshot to both
//   OutputDebugStringA (VS Output / DebugView) AND the LOG() console.
//
// Design — pure data snapshot, zero coupling to game objects:
//   Callers build a BattleHUDSnapshot (plain structs, owned strings)
//   and pass it to BattleDebugHUD::Render().  The HUD class formats
//   it — nothing about battle rules or input state lives here.
//
// What each section shows:
//   simulationPhase  — BattleManager FSM state (PLAYER_TURN, RESOLVING…)
//   inputPhase       — player input sub-menu (COMMAND_SELECT, SKILL_SELECT…)
//   menuItems        — top-level command list with cursor (Fight/Flee/…)
//   skillRows        — available skills with availability flag
//   infoLines        — arbitrary key/value (target name, hint text, …)
//   combatants       — HP/rage/stats table for all combatants
//   logLines         — last N lines from the battle log
//
// Output goes to BOTH:
//   OutputDebugStringA  — VS Output window / DebugView (always)
//   LOG()               — timestamped game console (always)
// ============================================================
#pragma once
#include <string>
#include <vector>

// ------------------------------------------------------------
// BattleHUDSnapshot: everything the HUD needs — plain data, no ptrs.
// ------------------------------------------------------------
struct BattleHUDSnapshot
{
    std::string title = "BATTLE STATE";

    // ---- Simulation FSM phase (BattleManager) ----
    std::string simulationPhase;   // "PLAYER_TURN", "RESOLVING", "ENEMY_TURN"…

    // ---- Player input phase (empty when not player's turn) ----
    std::string inputPhase;        // "COMMAND_SELECT", "SKILL_SELECT", "TARGET_SELECT"

    // ---- Top-level command menu (Fight / Flee / …) ----
    struct MenuItem {
        std::string label;
        bool        selected = false;
    };
    std::vector<MenuItem> menuItems;

    // ---- Skill list (shown during SKILL_SELECT / TARGET_SELECT) ----
    struct SkillRow {
        int         slot      = 0;      // 1-based key hint shown to player
        std::string name;
        std::string description;
        bool        available = true;   // false = greyed out (no MP / rage)
        bool        selected  = false;  // cursor is on this row
    };
    std::vector<SkillRow> skillRows;

    // ---- Generic key/value info lines (target cursor, hints, …) ----
    struct InfoLine {
        std::string key;
        std::string value;
    };
    std::vector<InfoLine> infoLines;

    // ---- Combatant table ----
    struct CombatantRow {
        std::string tag;                   // "[PLAYER]" or "[ENEMY ]"
        std::string name;
        bool        isCurrentTurn = false; // ">>>" marker on the active combatant
        int hp = 0,    maxHp = 0;
        int rage = 0,  maxRage = 0;        // maxRage==0 → enemy, omit rage column
        int atk = 0,   def = 0,   spd = 0;
        bool alive = true;
    };
    std::vector<CombatantRow> combatants;

    // ---- Recent battle log (caller provides last N lines) ----
    std::vector<std::string> logLines;
};

// ------------------------------------------------------------
// BattleDebugHUD: static formatter — no member state, no ownership.
// ------------------------------------------------------------
class BattleDebugHUD
{
public:
    // Render the snapshot to OutputDebugStringA AND LOG() in one call.
    static void Render(const BattleHUDSnapshot& snap);

private:
    static void BuildLines(const BattleHUDSnapshot& snap,
                           std::vector<std::string>& out);

    static void PushBorder    (std::vector<std::string>& out, const std::string& title, int w);
    static void PushDivider   (std::vector<std::string>& out, const char* label, int w);
    static void PushKV        (std::vector<std::string>& out, const char* key, const std::string& val);
    // PushPhaseBanner: bold two-line block — engine state + what player must do + key hints.
    // This is the FIRST section rendered after the title border.
    static void PushPhaseBanner(std::vector<std::string>& out,
                                const std::string& simPhase,
                                const std::string& inputPhase,
                                int w);
    static void PushMenu      (std::vector<std::string>& out,
                                const std::vector<BattleHUDSnapshot::MenuItem>& items);
    static void PushSkills    (std::vector<std::string>& out,
                                const std::vector<BattleHUDSnapshot::SkillRow>& rows);
    static void PushInfoLines (std::vector<std::string>& out,
                                const std::vector<BattleHUDSnapshot::InfoLine>& lines);
    static void PushCombatants(std::vector<std::string>& out,
                                const std::vector<BattleHUDSnapshot::CombatantRow>& rows);
    static void PushLog       (std::vector<std::string>& out,
                                const std::vector<std::string>& lines);
};
