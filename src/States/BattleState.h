// ============================================================
// File: BattleState.h
// Responsibility: IGameState wrapper for a turn-based battle session.
//
// Owns:
//   BattleManager     — the full battle simulation
//   HealthBarRenderer — three-layer HP bar UI widget (background, fill, frame)
//   BattleRenderer    — renders all combatant sprites at fixed screen slots
//   IBattleCommand[]  — data-driven list of top-level menu options
//
// Player Input FSM (PlayerInputPhase):
//   COMMAND_SELECT  — player picks a top-level action (Fight / Flee / …)
//   SKILL_SELECT    — player picks which skill to use  (1 / 2 / 3)
//   TARGET_SELECT   — player picks which enemy to target (Tab / Enter)
//
//   Navigating COMMAND_SELECT: Up/Down arrow to move cursor, Enter to confirm.
//   Adding a new command: create a class implementing IBattleCommand, append
//   it to mCommands in BuildCommandList() — zero other changes needed.
//
// Debug console output:
//   DumpStateToDebugOutput() prints the current input phase, all combatant
//   stats, and the active command/skill cursor on every phase change.
//
// Rendering:
//   - Screen cleared to navy-blue (signals active battle).
//   - BattleRenderer draws player + enemy sprites at 3 slots each.
//   - HealthBarRenderer  draws the player HP bar at bottom-right.
//   - EnemyHpBarRenderer draws up to 3 enemy HP bars at top-center.
//
// Lifetime:
//   Pushed onto StateManager by PlayState when B is pressed.
//   Pops itself on WIN or LOSE and broadcasts "battle_end_victory" or
//   "battle_end_defeat" for the caller to react to.
// ============================================================
#pragma once
#include "../States/IGameState.h"
#include "../Renderer/D3DContext.h"
#include "../Battle/BattleManager.h"
#include "../Battle/IBattler.h"
#include "../Battle/IBattleCommand.h"
#include "../Battle/BattleRenderer.h"
#include "../UI/HealthBarRenderer.h"
#include "../UI/EnemyHpBarRenderer.h"
#include "../UI/BattleTextRenderer.h"
#include <vector>
#include <memory>

// ------------------------------------------------------------
// PlayerInputPhase: which sub-menu is currently active.
//   Owned and mutated by BattleState; read by IBattleCommand
//   implementations via SetInputPhase().
// ------------------------------------------------------------
enum class PlayerInputPhase
{
    COMMAND_SELECT,   // top-level: Fight / Flee / Item …
    SKILL_SELECT,     // which skill (1/2/3)
    TARGET_SELECT     // which enemy (Tab to cycle, Enter to confirm)
};

class BattleState : public IGameState
{
public:
    explicit BattleState(D3DContext& d3d);

    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "BattleState"; }

    // ------------------------------------------------------------
    // SetInputPhase: public API used by IBattleCommand implementations
    //   to drive the input FSM.  IBattleCommand must not touch any other
    //   BattleState internals — all state changes go through this method.
    // ------------------------------------------------------------
    void SetInputPhase(PlayerInputPhase phase);

    // ------------------------------------------------------------
    // RequestFlee: called by FleeCommand to signal that the player wants
    //   to exit the battle.  Does NOT pop the state immediately —
    //   popping from inside IBattleCommand::Execute() would destroy
    //   BattleState while its own call stack is still live (use-after-free).
    //   The flag is checked at the END of Update() after all handlers return.
    // ------------------------------------------------------------
    void RequestFlee() { mPendingFlee = true; }

private:
    D3DContext&        mD3D;
    BattleManager      mBattle;
    BattleRenderer     mBattleRenderer;
    HealthBarRenderer   mHealthBar;       // player HP bar (bottom-right)
    EnemyHpBarRenderer  mEnemyHpBar;     // enemy HP bars (top-center, up to 3)
    BattleTextRenderer  mTextRenderer;   // shared font renderer for battle HUD text

    // ---- Player input FSM ----
    PlayerInputPhase  mInputPhase     = PlayerInputPhase::COMMAND_SELECT;
    int               mCommandIndex   = 0;   // cursor in COMMAND_SELECT
    int               mSkillIndex     = 0;   // cursor in SKILL_SELECT
    int               mTargetIndex    = 0;   // cursor in TARGET_SELECT

    // Set to true by RequestFlee(); acted on at the end of Update() so
    // BattleState is never destroyed while its own call stack is still live.
    bool              mPendingFlee    = false;

    // Data-driven command list — built once in BuildCommandList().
    // Order here is the order shown in the debug console menu.
    std::vector<std::unique_ptr<IBattleCommand>> mCommands;

    // One-press key tracking — avoids repeated firing while a key is held.
    // All three input phases share the same Up/Down/Enter/Backspace keys;
    // the active phase determines what the keys do (unified navigation).
    bool mKeyUpWasDown    = false;
    bool mKeyDownWasDown  = false;
    bool mEnterWasDown    = false;
    bool mBackWasDown     = false;  // Backspace — navigate back one input phase

    // Populate mCommands with all available top-level options.
    void BuildCommandList();

    // Per-phase input handlers — called from HandleInput().
    void HandleInput();
    void HandleCommandSelect();
    void HandleSkillSelect();
    void HandleTargetSelect();

    // Shared helpers.
    void CycleTarget();
    void ConfirmSkillAndTarget();

    // Write a full debug snapshot to OutputDebugStringA.
    // Called on every input phase transition and turn change.
    void DumpStateToDebugOutput() const;
};
