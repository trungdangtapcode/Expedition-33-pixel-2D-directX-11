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
#include "BattleContext.h"
#include "EnemyEncounterData.h"
#include "../Systems/PartyManager.h"
#include "../Utils/JsonLoader.h"
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
    void Initialize(const EnemyEncounterData& encounter, const JsonLoader::BattleSystemConfig& config);

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

    // ------------------------------------------------------------
    // SetPlayerItem: called by BattleState when the player picks an
    //   item to consume this turn.  Only valid during PLAYER_TURN.
    //   itemId — id of the item in ItemRegistry / Inventory
    //   target — primary target (may be nullptr for SelfOnly / AoE)
    // ------------------------------------------------------------
    void SetPlayerItem(const std::string& itemId, IBattler* target);

    // Active player (the one whose turn it currently is) — may be nullptr.
    PlayerCombatant* GetActivePlayer() const;
    IBattler*        GetActiveEnemy()  const;

    // Turn timeline for tick-based agility system
    struct TurnNode {
        IBattler* battler;
        float currentAV;
        float baseAgility;
    };

    const std::vector<TurnNode>& GetTimeline() const { return mTimeline; }

    // Simulate the future actions in the queue based on the current AV timeline
    std::vector<IBattler*> GetFutureTurnQueue(int queueSize) const;

    // -- BattleContext access --
    // Exposed so UI/helpers (e.g. BattleInputController skill greyout,
    // BattleState skill list) can evaluate predicate-gated stat reads and
    // CanUse checks against the SAME context the simulation is using.
    // The reference is stable for the battle duration; contents refresh
    // each frame via RebuildContext at the top of Update.
    const BattleContext& GetContext() const { return mContext; }

private:
    // -- Team storage --
    std::vector<std::unique_ptr<PlayerCombatant>> mPlayers;
    std::vector<std::unique_ptr<EnemyCombatant>>  mEnemies;

    std::vector<TurnNode> mTimeline;
    static constexpr float kActionGauge = 10000.0f;

    ActionQueue            mQueue;
    BattlePhase            mPhase   = BattlePhase::INIT;
    BattleOutcome          mOutcome = BattleOutcome::NONE;

    std::vector<std::string> mBattleLog;
    int mTotalExpPool = 0;

    // Live battle context refreshed at the top of Update each frame.
    // Systems that need to read live state (skills, damage calculator,
    // UI predicates) hold pointers into this single member — the ADDRESS
    // is stable for the entire battle, the CONTENTS change per frame.
    BattleContext            mContext;

    // -- Internal helpers --
    void BuildTurnOrder();
    void AdvanceTurn();           // increment mCurrentTurnIndex, skip dead
    IBattler* CurrentCombatant(); // returns mTurnOrder[mCurrentTurnIndex]

    // Rebuild mContext in place from current team / timeline state.
    // Safe to call every frame — it only assigns plain-data members.
    void RebuildContext(float dt);

    void EnqueueSkillActions(IBattler& caster, ISkill& skill,
                             std::vector<IBattler*> targets);

    // Build the action sequence for one item use and enqueue it.
    // Mirrors EnqueueSkillActions but takes an item id + primary target
    // and delegates to BuildItemActions::Build internally.
    void EnqueueItemActions(IBattler& user,
                            const std::string& itemId,
                            IBattler* primaryTarget);

    void Log(const std::string& msg);

    bool AllPlayersDefeated() const;
    bool AllEnemiesDefeated()  const;

    void HandlePlayerTurn(float dt);
    void HandleEnemyTurn(float dt);
    void HandleResolving(float dt);
};
