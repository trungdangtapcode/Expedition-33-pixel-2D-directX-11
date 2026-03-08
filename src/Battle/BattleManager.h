// ============================================================
// File: BattleManager.h
// Responsibility: Own all combatants, drive the turn-order FSM, and
//                 enqueue IAction sequences into the ActionQueue.
//
// FSM Phases:
//   INIT         — one-frame setup (sort turn order, log opening message)
//   PLAYER_TURN  — wait for PlayerCombatant::HasPendingAction()
//   RESOLVING    — drain ActionQueue (no input accepted during animations)
//   ENEMY_TURN   — auto-select target, enqueue AI actions, enter RESOLVING
//   WIN          — all enemies dead; notify via mOutcome
//   LOSE         — all players dead; notify via mOutcome
//
// Ownership:
//   Teams stored as unique_ptr arrays — BattleManager is sole owner.
//   ActionQueue is a value member — no heap allocation needed.
//
// BattleState polls GetOutcome() each Update to detect WIN / LOSE.
//
// Persistent party HP:
//   Initialize() reads Verso's current stats from PartyManager so wounds
//   carry over from previous battles.
//   BattleState::Update() writes the stats back via PartyManager::SetVersoStats()
//   before popping the state.
// ============================================================
#pragma once
#include "PlayerCombatant.h"
#include "EnemyCombatant.h"
#include "ActionQueue.h"
#include "IBattler.h"
#include "EnemyEncounterData.h"
#include "../Systems/PartyManager.h"
#include <vector>
#include <string>
#include <memory>
#include <array>

enum class BattlePhase
{
    INIT,
    PLAYER_TURN,
    RESOLVING,
    ENEMY_TURN,
    WIN,
    LOSE
};

enum class BattleOutcome
{
    NONE,
    VICTORY,
    DEFEAT
};

class BattleManager
{
public:
    static constexpr int kMaxTeamSize = 4;

    BattleManager();

    // Called by BattleState::OnEnter — spawn combatants from encounter data,
    // sort turn order.  Enemy count and stats come from encounter.battleParty
    // (data-driven); no values are hardcoded inside Initialize.
    void Initialize(const EnemyEncounterData& encounter);

    // Main update — drives FSM + ActionQueue each frame.
    void Update(float dt);

    // -- Queries for BattleState UI --
    BattlePhase   GetPhase()   const { return mPhase; }
    BattleOutcome GetOutcome() const { return mOutcome; }

    const std::vector<std::string>& GetBattleLog() const { return mBattleLog; }

    // Alive combatant lists for UI rendering.
    std::vector<IBattler*> GetAlivePlayers() const;
    std::vector<IBattler*> GetAliveEnemies() const;

    // All players (alive + dead) for stat display.
    std::vector<IBattler*> GetAllPlayers() const;
    std::vector<IBattler*> GetAllEnemies() const;

    // ------------------------------------------------------------
    // SetPlayerAction: called by BattleState when the player selects
    //   a skill and target.  Only valid during PLAYER_TURN phase.
    //   skillIndex — index into PlayerCombatant's skill list.
    //   target     — chosen enemy.
    // ------------------------------------------------------------
    void SetPlayerAction(int skillIndex, IBattler* target);

    // Active player (the one whose turn it currently is) — may be nullptr.
    PlayerCombatant* GetActivePlayer() const;
    IBattler*        GetActiveEnemy()  const;

private:
    // -- Team storage --
    std::vector<std::unique_ptr<PlayerCombatant>> mPlayers;
    std::vector<std::unique_ptr<EnemyCombatant>>  mEnemies;

    // Turn order: raw non-owning pointers sorted by SPD descending.
    std::vector<IBattler*> mTurnOrder;
    int                    mCurrentTurnIndex = 0;

    ActionQueue            mQueue;
    BattlePhase            mPhase   = BattlePhase::INIT;
    BattleOutcome          mOutcome = BattleOutcome::NONE;

    std::vector<std::string> mBattleLog;

    // -- Internal helpers --
    void BuildTurnOrder();
    void AdvanceTurn();           // increment mCurrentTurnIndex, skip dead
    IBattler* CurrentCombatant(); // returns mTurnOrder[mCurrentTurnIndex]

    void EnqueueSkillActions(IBattler& caster, ISkill& skill,
                             std::vector<IBattler*> targets);
    void Log(const std::string& msg);

    bool AllPlayersDefeated() const;
    bool AllEnemiesDefeated()  const;

    void HandlePlayerTurn(float dt);
    void HandleEnemyTurn(float dt);
    void HandleResolving(float dt);
};
