// ============================================================
// File: BattleState.h
// Responsibility: IGameState wrapper for a turn-based battle session.
//
// Owns:
//   BattleManager     — the full battle simulation
//   HealthBarRenderer — three-layer HP bar UI widget (background, fill, frame)
//   BattleRenderer    — renders all combatant sprites at fixed screen slots
//   IBattleCommand[]  — data-driven list of top-level menu options
//   IrisTransitionRenderer — iris circle overlay for enter/exit transitions
//
// Construction:
//   BattleState(d3d, encounter) — encounter carries the overworld enemy's
//   sprite paths and stats.  enemySlots[0] is populated from this data
//   instead of hardcoded paths.  If encounter.name is empty (default),
//   the fallback skeleton data is used (preserves backwards compatibility
//   when BattleState is pushed without an OverworldEnemy context).
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
// Iris transition:
//   OnEnter  → iris.Initialize() + iris.StartOpen() (reveals the battle from black).
//   WIN/LOSE → iris.StartClose(callback) — when black, sets mPendingSafeExit flag.
//   Flee     → same pattern as WIN/LOSE.
//   End of Update(): if mPendingSafeExit → broadcast event + PopState().
//
//   The deferred-exit pattern (mPendingSafeExit flag) prevents use-after-free:
//   StateManager::PopState() destroys BattleState immediately.  Any member
//   access after PopState() is undefined behaviour.  We therefore check the
//   flag at the VERY END of Update(), after all other code has returned.
//
// Rendering:
//   - Screen cleared to navy-blue (signals active battle).
//   - BattleRenderer draws player + enemy sprites at 3 slots each.
//   - HealthBarRenderer  draws the player HP bar at bottom-right.
//   - EnemyHpBarRenderer draws up to 3 enemy HP bars at top-center.
//   - IrisTransitionRenderer drawn LAST as the topmost overlay.
//
// Lifetime:
//   Pushed onto StateManager by PlayState when B is pressed near an enemy.
//   Pops itself on WIN or LOSE and broadcasts "battle_end_victory" or
//   "battle_end_defeat" for the caller to react to.
// ============================================================
#pragma once
#include "../States/IGameState.h"
#include "../Renderer/D3DContext.h"
#include "../Renderer/IrisTransitionRenderer.h"
#include "../Battle/BattleManager.h"
#include "../Battle/IBattler.h"
#include "../Battle/IBattleCommand.h"
#include "../Battle/BattleRenderer.h"
#include "../Battle/EnemyEncounterData.h"
#include "../Battle/BattleInputController.h"
#include "../UI/HealthBarRenderer.h"
#include "../UI/EnemyHpBarRenderer.h"
#include "../UI/BattleTextRenderer.h"
#include "../UI/PointerRenderer.h"
#include "../Renderer/NineSliceRenderer.h"
#include "../Utils/JsonLoader.h"
#include <vector>
#include <memory>
#include <string>

class BattleState : public IGameState
{
public:
    // ------------------------------------------------------------
    // Constructor
    // Purpose:
    //   Store D3DContext reference and encounter data.
    //   Actual GPU initialization is deferred to OnEnter() — the D3D
    //   device must be valid before any resource creation.
    //
    // Parameters:
    //   d3d       — D3D11 device/context/RTV facade (owned by GameApp)
    //   encounter — overworld enemy data package.  If encounter.name is
    //               empty, OnEnter() falls back to the hardcoded skeleton.
    //               This preserves backwards compatibility for legacy callers.
    // ------------------------------------------------------------
    BattleState(D3DContext& d3d, EnemyEncounterData encounter = {});

    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "BattleState"; }

    // Public API used by IBattleCommand implementations or BattleInputController.
    void SetInputPhase(PlayerInputPhase phase) { mInputController.SetInputPhase(phase); }

    // RequestFlee: called by FleeCommand — deferred exit pattern.
    // See mPendingFlee comment and BattleState.h header for full rationale.
    void RequestFlee() { mPendingFlee = true; }

    void DumpStateToDebugOutput() const;

private:
    D3DContext&            mD3D;
    EnemyEncounterData     mEncounter;       // overworld encounter data (may be default/empty)
    BattleManager          mBattle;
    BattleRenderer         mBattleRenderer;
    HealthBarRenderer      mHealthBar;
    EnemyHpBarRenderer     mEnemyHpBar;
    PointerRenderer        mTargetPointer;
    NineSliceRenderer      mDialogBox;
    BattleTextRenderer     mTextRenderer;
    IrisTransitionRenderer mIris;            // iris overlay (opens on enter, closes on exit)
    BattleInputController  mInputController;

    JsonLoader::BattleMenuLayout mMenuLayout;

    // ---- Deferred exit state ----
    // mWaitingForDeathAnims: set true when a battle outcome is first detected.
    //   While true, BattleRenderer.Update(dt) keeps running (inside the
    //   !mExitTransitionStarted block) so die clips advance.  Once
    //   BattleRenderer::AreAllDeathAnimsDone() returns true, this flag is
    //   cleared and mExitTransitionStarted is set + iris close begins.
    //
    // Why delay the iris?
    //   mBattle.GetOutcome() fires on the same frame HP hits 0.  Without the
    //   delay, StartClose() fires before the die clip plays even one frame.
    //   Characters appear to vanish instantly rather than collapsing.
    bool mWaitingForDeathAnims = false;

    // mExitTransitionStarted: prevents double-triggering when outcome is detected.
    //   Set to true the moment the iris starts closing; never cleared.
    bool mExitTransitionStarted = false;

    // mPendingSafeExit: set to true by the iris-close callback.
    //   Checked at the END of Update() — after all handlers have returned —
    //   so PopState() is called when no BattleState code is on the call stack.
    bool mPendingSafeExit = false;

    // Event name to broadcast when the safe exit fires (victory, defeat, or flee).
    std::string mExitEventName;

    // mPendingFlee: set by FleeCommand::Execute() (legacy one-frame-deferred pop).
    //   StartClose replaces the direct PopState() — checked each frame.
    bool mPendingFlee = false;

    void InitAudio();
    void InitBattleSlots();
    void InitUIRenderers();
    void UpdateLogic(float dt);
    void UpdateUIRenderers(float dt, IBattler* targetedEnemyPtr, bool playerSelected);
    void CheckDeathAnimations();
    void CheckBattleEnd();

    // ----------------------------------------------------------------
    // Per-slot alive state from the previous frame.
    // Compared against the current frame after mBattle.Update(dt) to
    // detect the exact frame when a combatant's HP reaches 0, so the
    // die animation fires exactly once on the death frame.
    //
    // Initialized to true in OnEnter (all combatants alive at battle start).
    // Updated every frame inside the !mExitTransitionStarted block.
    // ----------------------------------------------------------------
    bool mEnemyWasAlive [BattleRenderer::kMaxSlots] = { true, true, true };
    bool mPlayerWasAlive[BattleRenderer::kMaxSlots] = { true, true, true };

    // ----------------------------------------------------------------
    // UI Animations
    // ----------------------------------------------------------------
    float mCmdMenuTimer = 0.0f;
    float mSkillMenuTimer = 0.0f;
    PlayerInputPhase mLastInputPhase = PlayerInputPhase::COMMAND_SELECT;
};
