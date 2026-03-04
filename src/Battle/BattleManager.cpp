// ============================================================
// File: BattleManager.cpp
// Responsibility: Drive the turn-based combat FSM and action resolution.
// ============================================================
#include "BattleManager.h"
#include "LogAction.h"
#include "DelayedAction.h"
#define NOMINMAX
#include <algorithm>
#include "../Utils/Log.h"
#include "../Events/EventManager.h"

BattleManager::BattleManager() = default;

// ------------------------------------------------------------
// Initialize: create combatants and build the initial turn order.
// In the full game the team compositions come from data/enemies/*.json
// and the party roster loaded from save data.  For MVP they are inline.
//
// Verso's stats are seeded from PartyManager so HP carries over from
// previous battles.  On the very first battle PartyManager returns the
// default full-HP stats (100/100).
// ------------------------------------------------------------
void BattleManager::Initialize()
{
    // -- Spawn player party — Verso with persistent HP from PartyManager --
    mPlayers.push_back(std::make_unique<PlayerCombatant>(
        "Verso", PartyManager::Get().GetVersoStats()));

    // -- Spawn enemy team — two Skeleton combatants in slots 0 and 1.
    // Slot 2 is intentionally left empty; add a third enemy here when needed.
    mEnemies.push_back(std::make_unique<EnemyCombatant>("Skeleton A"));
    mEnemies.push_back(std::make_unique<EnemyCombatant>("Skeleton B"));

    BuildTurnOrder();

    Log("--- BATTLE START ---");
    for (auto& p : mPlayers) Log(p->GetName() + " HP:" + std::to_string(p->GetStats().hp));
    for (auto& e : mEnemies) Log(e->GetName() + " HP:" + std::to_string(e->GetStats().hp));

    mPhase = BattlePhase::PLAYER_TURN;
}

// ------------------------------------------------------------
// BuildTurnOrder: sort all living combatants by SPD descending.
// This snapshot is taken at battle start; combatants that die mid-battle
// are skipped by AdvanceTurn() via the IsAlive() check.
// ------------------------------------------------------------
void BattleManager::BuildTurnOrder()
{
    mTurnOrder.clear();
    for (auto& p : mPlayers) mTurnOrder.push_back(p.get());
    for (auto& e : mEnemies) mTurnOrder.push_back(e.get());

    std::sort(mTurnOrder.begin(), mTurnOrder.end(),
        [](const IBattler* a, const IBattler* b) {
            return a->GetStats().spd > b->GetStats().spd;  // higher SPD acts first
        });

    mCurrentTurnIndex = 0;
}

// ------------------------------------------------------------
// Update: main FSM tick.  Called by BattleState every frame.
// ------------------------------------------------------------
void BattleManager::Update(float dt)
{
    if (mPhase == BattlePhase::WIN || mPhase == BattlePhase::LOSE) return;
    if (mPhase == BattlePhase::INIT) { Initialize(); return; }

    if (mPhase == BattlePhase::PLAYER_TURN) HandlePlayerTurn(dt);
    else if (mPhase == BattlePhase::ENEMY_TURN)  HandleEnemyTurn(dt);
    else if (mPhase == BattlePhase::RESOLVING)   HandleResolving(dt);
}

// ------------------------------------------------------------
// HandlePlayerTurn: wait until the player has committed an action
//   via SetPlayerAction().  UI is fully in charge of input here.
// ------------------------------------------------------------
void BattleManager::HandlePlayerTurn(float /*dt*/)
{
    IBattler* current = CurrentCombatant();
    if (!current || !current->IsPlayerControlled()) {
        // Turn order moved to an enemy slot — switch phase.
        mPhase = BattlePhase::ENEMY_TURN;
        return;
    }

    auto* player = static_cast<PlayerCombatant*>(current);
    if (!player->HasPendingAction()) return;  // waiting for UI input

    // Collect the player's choice.
    ISkill* skill = player->GetSkill(player->GetPendingSkillIndex());
    IBattler* target = player->GetPendingTarget();
    player->ClearPendingAction();

    if (skill && target && skill->CanUse(*player))
    {
        std::vector<IBattler*> targets = { target };
        EnqueueSkillActions(*player, *skill, targets);
    }
    else
    {
        Log(player->GetName() + " cannot use that skill!");
    }

    player->OnTurnEnd();
    mPhase = BattlePhase::RESOLVING;
}

// ------------------------------------------------------------
// HandleEnemyTurn: AI picks a target and enqueues the attack.
// ------------------------------------------------------------
void BattleManager::HandleEnemyTurn(float /*dt*/)
{
    IBattler* current = CurrentCombatant();
    if (!current) { AdvanceTurn(); return; }
    if (current->IsPlayerControlled()) {
        mPhase = BattlePhase::PLAYER_TURN;
        return;
    }

    auto* enemy = static_cast<EnemyCombatant*>(current);
    std::vector<IBattler*> alivePlayers = GetAlivePlayers();
    IBattler* target = enemy->ChooseTarget(alivePlayers);

    if (target)
    {
        std::vector<IBattler*> targets = { target };
        EnqueueSkillActions(*enemy, *enemy->GetAttackSkill(), targets);
    }

    enemy->OnTurnEnd();
    mPhase = BattlePhase::RESOLVING;
}

// ------------------------------------------------------------
// HandleResolving: drain the queue; check win/lose after each action.
// ------------------------------------------------------------
void BattleManager::HandleResolving(float dt)
{
    mQueue.Update(dt);

    if (!mQueue.IsEmpty()) return;  // still executing actions

    // All actions in this turn's queue have resolved.
    // Broadcast the current HP of every player combatant so UI elements
    // (HealthBarRenderer) can react to damage without polling each frame.
    for (auto& p : mPlayers)
    {
        // Use the combatant's name as part of a namespaced event so
        // future multi-character parties can each have their own bar.
        // For now "verso_hp_changed" is the canonical event name.
        const std::string eventName =
            p->GetName() == "Verso" ? "verso_hp_changed"
                                     : p->GetName() + "_hp_changed";

        EventData data;
        data.name  = eventName;
        // EventData.value carries the raw HP value (float cast from int).
        // HealthBarRenderer divides by maxHp to get the ratio internally,
        // keeping the ratio calculation in one place and making the event
        // payload meaningful on its own.
        data.value = static_cast<float>(p->GetStats().hp);
        EventManager::Get().Broadcast(eventName, data);
    }

    // All actions done — check outcome before advancing turn.
    if (AllEnemiesDefeated())
    {
        Log("--- VICTORY! ---");
        mOutcome = BattleOutcome::VICTORY;
        mPhase   = BattlePhase::WIN;
        return;
    }
    if (AllPlayersDefeated())
    {
        Log("--- DEFEAT ---");
        mOutcome = BattleOutcome::DEFEAT;
        mPhase   = BattlePhase::LOSE;
        return;
    }

    AdvanceTurn();
}

// ------------------------------------------------------------
// AdvanceTurn: move to the next living combatant in turn order.
// ------------------------------------------------------------
void BattleManager::AdvanceTurn()
{
    if (mTurnOrder.empty()) return;

    // Cycle to next living combatant; guard against infinite loop if all dead.
    const int total = static_cast<int>(mTurnOrder.size());
    for (int attempts = 0; attempts < total; ++attempts)
    {
        mCurrentTurnIndex = (mCurrentTurnIndex + 1) % total;
        if (mTurnOrder[mCurrentTurnIndex]->IsAlive()) break;
    }

    IBattler* next = CurrentCombatant();
    if (!next) return;

    next->OnTurnStart();
    Log("--- " + next->GetName() + "'s turn ---");

    if (next->IsPlayerControlled())
        mPhase = BattlePhase::PLAYER_TURN;
    else
        mPhase = BattlePhase::ENEMY_TURN;
}

// ------------------------------------------------------------
// EnqueueSkillActions: execute the skill to get its IAction list,
//   inject the BattleLog pointer into every LogAction, then wrap each
//   action in a DelayedAction before enqueuing.
//
// Why wrap every action with DelayedAction?
//   Raw actions (DamageAction, LogAction) complete instantaneously — the
//   entire turn would resolve in a single frame with no visible feedback.
//   DelayedAction holds the queue for kDefaultDelay (1.0 s) after each
//   action, giving the player time to read log text and see hit reactions.
//
// Future extension:
//   When animation playback is implemented, replace DelayedAction with a
//   subclass (e.g. HitAnimationAction) that overrides OnAfter to wait for
//   the sprite clip to finish before releasing the queue.  No other code
//   in BattleManager needs to change.
// ------------------------------------------------------------
void BattleManager::EnqueueSkillActions(IBattler& caster, ISkill& skill,
                                         std::vector<IBattler*> targets)
{
    auto actions = skill.Execute(caster, targets);
    for (auto& action : actions)
    {
        std::unique_ptr<IAction> finalAction;

        // Downcast to LogAction to inject the live log pointer.
        // This is the ONLY justified downcast in the battle system:
        // skills cannot know the log address at construction time.
        if (auto* logAct = dynamic_cast<LogAction*>(action.get()))
        {
            // Re-create with the correct log pointer so messages appear
            // in the BattleState scrolling log panel.
            std::string msg = logAct->GetText();
            finalAction = std::make_unique<LogAction>(&mBattleLog, std::move(msg));
        }
        else
        {
            finalAction = std::move(action);
        }

        // Wrap the concrete action in a DelayedAction so the queue pauses
        // for kDefaultDelay seconds after each step.
        // Pass ownership into DelayedAction; it takes sole responsibility.
        mQueue.Enqueue(std::make_unique<DelayedAction>(std::move(finalAction)));
    }
}

// -- Queries --

std::vector<IBattler*> BattleManager::GetAlivePlayers() const
{
    std::vector<IBattler*> out;
    for (auto& p : mPlayers) if (p->IsAlive()) out.push_back(p.get());
    return out;
}

std::vector<IBattler*> BattleManager::GetAliveEnemies() const
{
    std::vector<IBattler*> out;
    for (auto& e : mEnemies) if (e->IsAlive()) out.push_back(e.get());
    return out;
}

std::vector<IBattler*> BattleManager::GetAllPlayers() const
{
    std::vector<IBattler*> out;
    for (auto& p : mPlayers) out.push_back(p.get());
    return out;
}

std::vector<IBattler*> BattleManager::GetAllEnemies() const
{
    std::vector<IBattler*> out;
    for (auto& e : mEnemies) out.push_back(e.get());
    return out;
}

void BattleManager::SetPlayerAction(int skillIndex, IBattler* target)
{
    IBattler* current = CurrentCombatant();
    if (!current || !current->IsPlayerControlled()) return;

    auto* player = static_cast<PlayerCombatant*>(current);
    player->SetPendingAction(skillIndex, target);
}

PlayerCombatant* BattleManager::GetActivePlayer() const
{
    IBattler* c = mTurnOrder.empty() ? nullptr : mTurnOrder[mCurrentTurnIndex];
    if (c && c->IsPlayerControlled()) return static_cast<PlayerCombatant*>(c);
    return nullptr;
}

IBattler* BattleManager::GetActiveEnemy() const
{
    IBattler* c = mTurnOrder.empty() ? nullptr : mTurnOrder[mCurrentTurnIndex];
    if (c && !c->IsPlayerControlled()) return c;
    return nullptr;
}

IBattler* BattleManager::CurrentCombatant()
{
    if (mTurnOrder.empty()) return nullptr;
    return mTurnOrder[mCurrentTurnIndex];
}

void BattleManager::Log(const std::string& msg)
{
    mBattleLog.push_back(msg);
    LOG("[Battle] %s", msg.c_str());

    constexpr std::size_t kMaxLines = 64;
    if (mBattleLog.size() > kMaxLines)
        mBattleLog.erase(mBattleLog.begin());
}

bool BattleManager::AllPlayersDefeated() const
{
    for (auto& p : mPlayers) if (p->IsAlive()) return false;
    return true;
}

bool BattleManager::AllEnemiesDefeated() const
{
    for (auto& e : mEnemies) if (e->IsAlive()) return false;
    return true;
}
