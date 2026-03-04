// ============================================================
// File: BattleDebugHUD.cpp
// Responsibility: ASCII-art battle HUD formatter.
//   Builds all lines into a vector<string>, then emits each line via
//   OutputDebugStringA and accumulates them into one block for LOG().
//
// Layout constants (change here, nowhere else):
//   kBoxWidth   — total width of the ASCII box
//   kKeyWidth   — left-column width in key/value rows
//   kNameWidth  — character name column width in combatant table
//   kLogLines   — how many recent battle log lines to show
// ============================================================
#include "BattleDebugHUD.h"
#define NOMINMAX
#include <Windows.h>    // OutputDebugStringA
#include "../Utils/Log.h"
#include <cstdio>
#include <cstring>      // strlen
#include <string>
#include <vector>
#include <algorithm>    // std::min

static constexpr int kBoxWidth  = 62;
static constexpr int kKeyWidth  = 12;
static constexpr int kNameWidth = 12;
static constexpr int kLogLines  = 5;   // recent battle-log tail shown in HUD

// ============================================================
// Public
// ============================================================

// ------------------------------------------------------------
// Render: build all lines, emit via OutputDebugStringA, then emit
//   the entire block as a single LOG() call so it appears in the
//   timestamped game console with one timestamp entry.
// ------------------------------------------------------------
void BattleDebugHUD::Render(const BattleHUDSnapshot& snap)
{
    std::vector<std::string> lines;
    BuildLines(snap, lines);

    // -- OutputDebugStringA path (VS Output / DebugView) --
    for (const auto& line : lines)
    {
        std::string out = line + "\r\n";
        OutputDebugStringA(out.c_str());
    }

    // -- LOG() path: concatenate into one block separated by \n --
    // LOG() adds its own timestamp, so the whole HUD appears together.
    std::string block;
    block.reserve(lines.size() * 64);
    block += "\n";
    for (const auto& line : lines)
    {
        block += line;
        block += "\n";
    }
    LOG("%s", block.c_str());
}

// ============================================================
// Private — section builders
// ============================================================

void BattleDebugHUD::BuildLines(const BattleHUDSnapshot& snap,
                                 std::vector<std::string>& out)
{
    PushBorder(out, snap.title, kBoxWidth);

    // ---- Phase banner — the FIRST thing the player reads ----
    // Show a bold, unambiguous line: what the engine is doing AND
    // what the player is expected to do right now.
    PushPhaseBanner(out, snap.simulationPhase, snap.inputPhase, kBoxWidth);

    // ---- Command menu ----
    if (!snap.menuItems.empty())
        PushMenu(out, snap.menuItems);

    // ---- Skill list ----
    if (!snap.skillRows.empty())
        PushSkills(out, snap.skillRows);

    // ---- Info lines (target, hints, etc.) ----
    if (!snap.infoLines.empty())
        PushInfoLines(out, snap.infoLines);

    // ---- Combatant table ----
    if (!snap.combatants.empty())
    {
        PushDivider(out, "Combatants", kBoxWidth);
        PushCombatants(out, snap.combatants);
    }

    // ---- Battle log tail ----
    if (!snap.logLines.empty())
    {
        PushDivider(out, "Battle Log", kBoxWidth);
        PushLog(out, snap.logLines);
    }

    // ---- Bottom border ----
    out.push_back(std::string(kBoxWidth, '='));
}

// ------------------------------------------------------------
// PushPhaseBanner: the most prominent element in the HUD.
//   Emits a clear block telling the player:
//     1. What the engine is doing       (simulation phase)
//     2. What the player should do NOW  (action label + key hint)
//
//   Examples:
//     [ ENGINE: PLAYER_TURN   ]   [ YOUR TURN: SELECT A SKILL ]
//       [1] [2] [3] select skill   [Esc] back to commands
//
//     [ ENGINE: RESOLVING     ]   [ WAIT — action in progress... ]
//
//     [ ENGINE: ENEMY_TURN    ]   [ WAIT — enemy is acting... ]
// ------------------------------------------------------------
void BattleDebugHUD::PushPhaseBanner(std::vector<std::string>& out,
                                      const std::string& simPhase,
                                      const std::string& inputPhase,
                                      int /*w*/)
{
    std::string actionLabel;
    std::string keyHint;

    if (simPhase == "PLAYER_TURN")
    {
        if (inputPhase == "COMMAND_SELECT")
        {
            actionLabel = "YOUR TURN: SELECT A COMMAND";
            keyHint     = "  [Up/Down] navigate   [Enter] confirm";
        }
        else if (inputPhase == "SKILL_SELECT")
        {
            actionLabel = "YOUR TURN: SELECT A SKILL";
            keyHint     = "  [1] [2] [3] select skill   [Esc] back to commands";
        }
        else if (inputPhase == "TARGET_SELECT")
        {
            actionLabel = "YOUR TURN: SELECT A TARGET";
            keyHint     = "  [Tab] next target   [Enter] confirm   [Esc] back to skills";
        }
        else
        {
            actionLabel = "YOUR TURN";
            keyHint     = "";
        }
    }
    else if (simPhase == "RESOLVING")
    {
        actionLabel = "WAIT — action in progress...";
        keyHint     = "";
    }
    else if (simPhase == "ENEMY_TURN")
    {
        actionLabel = "WAIT — enemy is acting...";
        keyHint     = "";
    }
    else if (simPhase == "WIN")
    {
        actionLabel = "*** VICTORY! ***";
        keyHint     = "";
    }
    else if (simPhase == "LOSE")
    {
        actionLabel = "*** DEFEATED. ***";
        keyHint     = "";
    }
    else
    {
        actionLabel = simPhase.empty() ? "INITIALIZING..." : simPhase;
        keyHint     = "";
    }

    // Banner line: fixed-width engine tag + action label
    char buf[256];
    snprintf(buf, sizeof(buf), "  [ ENGINE: %-13s]   [ %s ]",
        simPhase.c_str(), actionLabel.c_str());
    out.push_back(buf);

    // Key hint on the very next line — only when there is something to press.
    if (!keyHint.empty())
        out.push_back(keyHint);

    // Blank line separates banner from the content below.
    out.push_back("");
}

// ------------------------------------------------------------
// PushBorder: "===== BATTLE STATE ====="
// ------------------------------------------------------------
void BattleDebugHUD::PushBorder(std::vector<std::string>& out,
                                  const std::string& title, int w)
{
    const int titleLen  = static_cast<int>(title.size());
    const int fill      = w - titleLen - 2;
    const int left      = fill / 2;
    const int right     = fill - left;

    std::string line;
    line.append(left,  '=');
    line += ' ';
    line += title;
    line += ' ';
    line.append(right, '=');
    out.push_back(line);
}

// ------------------------------------------------------------
// PushDivider: "  ---- Combatants ----"
// ------------------------------------------------------------
void BattleDebugHUD::PushDivider(std::vector<std::string>& out,
                                   const char* label, int w)
{
    const int labelLen = static_cast<int>(strlen(label));
    const int fill     = w - labelLen - 2 - 2;  // 2 indent + 2 spaces
    const int left     = fill / 2;
    const int right    = fill - left;

    std::string line = "  ";
    line.append(left,  '-');
    line += ' ';
    line += label;
    line += ' ';
    line.append(right, '-');
    out.push_back(line);
}

// ------------------------------------------------------------
// PushKV: "  Key        : value"
// ------------------------------------------------------------
void BattleDebugHUD::PushKV(std::vector<std::string>& out,
                               const char* key, const std::string& val)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "  %-*s: %s", kKeyWidth, key, val.c_str());
    out.push_back(buf);
}

// ------------------------------------------------------------
// PushMenu: command list with ">" cursor marker.
//   > [Enter] Fight
//     [Enter] Flee
// ------------------------------------------------------------
void BattleDebugHUD::PushMenu(std::vector<std::string>& out,
                                const std::vector<BattleHUDSnapshot::MenuItem>& items)
{
    out.push_back("  Actions :");
    for (const auto& item : items)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "    %s %s",
            item.selected ? ">" : " ",
            item.label.c_str());
        out.push_back(buf);
    }
    // Key hint is printed by PushPhaseBanner — no duplicate here.
}

// ------------------------------------------------------------
// PushSkills: skill table with slot key, availability, cursor.
//   > [1] Attack      Strike the enemy.       (available)
//     [2] Rage Strike Heavy hit — rage full    UNAVAILABLE
// ------------------------------------------------------------
void BattleDebugHUD::PushSkills(std::vector<std::string>& out,
                                  const std::vector<BattleHUDSnapshot::SkillRow>& rows)
{
    out.push_back("  Skills  :");
    for (const auto& row : rows)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "    %s [%d] %-14s  %s  %s",
            row.selected   ? ">" : " ",
            row.slot,
            row.name.c_str(),
            row.description.c_str(),
            row.available  ? ""  : "(unavailable)");
        out.push_back(buf);
    }
    // Key hint is printed by PushPhaseBanner — no duplicate here.
}

// ------------------------------------------------------------
// PushInfoLines: "  Key        : value" — generic info rows.
// ------------------------------------------------------------
void BattleDebugHUD::PushInfoLines(std::vector<std::string>& out,
                                     const std::vector<BattleHUDSnapshot::InfoLine>& lines)
{
    for (const auto& kv : lines)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "  %-*s: %s",
            kKeyWidth, kv.key.c_str(), kv.value.c_str());
        out.push_back(buf);
    }
}

// ------------------------------------------------------------
// PushCombatants: HP/rage/stat table.
//   >>> [PLAYER] Verso         HP: 80/100  Rage:  0/100  ATK:15  DEF:10  SPD:10  ALIVE
//       [ENEMY ] Skeleton A    HP: 30/ 50  ATK:10  DEF: 5  SPD: 7  ALIVE
// ------------------------------------------------------------
void BattleDebugHUD::PushCombatants(std::vector<std::string>& out,
                                     const std::vector<BattleHUDSnapshot::CombatantRow>& rows)
{
    for (const auto& row : rows)
    {
        char buf[300];
        const char* turn = row.isCurrentTurn ? ">>>" : "   ";

        if (row.maxRage > 0)
        {
            // Player — show rage column.
            snprintf(buf, sizeof(buf),
                "  %s %s %-*s  HP:%3d/%-3d  Rage:%3d/%-3d  ATK:%2d  DEF:%2d  SPD:%2d  %s",
                turn, row.tag.c_str(), kNameWidth, row.name.c_str(),
                row.hp, row.maxHp,
                row.rage, row.maxRage,
                row.atk, row.def, row.spd,
                row.alive ? "ALIVE" : "DEAD");
        }
        else
        {
            // Enemy — omit rage column.
            snprintf(buf, sizeof(buf),
                "  %s %s %-*s  HP:%3d/%-3d  ATK:%2d  DEF:%2d  SPD:%2d  %s",
                turn, row.tag.c_str(), kNameWidth, row.name.c_str(),
                row.hp, row.maxHp,
                row.atk, row.def, row.spd,
                row.alive ? "ALIVE" : "DEAD");
        }

        out.push_back(buf);
    }
}

// ------------------------------------------------------------
// PushLog: last kLogLines lines of the battle log, newest last.
// ------------------------------------------------------------
void BattleDebugHUD::PushLog(std::vector<std::string>& out,
                               const std::vector<std::string>& lines)
{
    const int total = static_cast<int>(lines.size());
    const int start = std::max(0, total - kLogLines);
    for (int i = start; i < total; ++i)
    {
        out.push_back("  " + lines[i]);
    }
}
