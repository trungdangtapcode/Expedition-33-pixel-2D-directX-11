// ============================================================
// File: BattleManager.cpp
// Responsibility: Drive the turn-based combat FSM and action resolution.
// ============================================================
#include "BattleManager.h"
#include "LogAction.h"

#include "EnemyEncounterData.h"
#define NOMINMAX
#include <algorithm>
#include "../Utils/Log.h"
#include "../Events/EventManager.h"

BattleManager::BattleManager() = default;

// ------------------------------------------------------------
// Initialize: create combatants from encounter data and build the
// initial turn order.
//
// Enemy team: built by iterating encounter.battleParty (1–3 entries).
//   Each EnemySlotData provides hp/atk/def/spd; name is generated as
//   "<encounter.name>" for single-enemy parties or
//   "<encounter.name> A/B/C" for multi-enemy parties.
//
// Player team: Verso with persistent HP from PartyManager so wounds
//   carry over from previous battles.
// ------------------------------------------------------------
void BattleManager::Initialize(const EnemyEncounterData& encounter)
{
    // -- Spawn player party — Verso with persistent HP from PartyManager --
    mPlayers.push_back(std::make_unique<PlayerCombatant>(
        "Verso", PartyManager::Get().GetVersoStats()));

    // -- Spawn enemy team from encounter.battleParty (data-driven) --
    // Name scheme: single enemy uses the encounter name; multiple enemies
    // append " A", " B", " C" so the HUD can distinguish them.
    const bool multipleEnemies = encounter.battleParty.size() > 1;
    for (int i = 0; i < static_cast<int>(encounter.battleParty.size()); ++i)
    {
        const EnemySlotData& sd = encounter.battleParty[i];

        // Build a name: "Skeleton" for single, "Skeleton A" for multi.
        std::string slotName = encounter.name;
        if (multipleEnemies) { slotName += ' '; slotName += static_cast<char>('A' + i); }

        // Build BattlerStats from JSON-sourced values.
        // mp=0 and maxMp=0: enemies do not use MP in the current design.
        // rage=0 and maxRage=0: rage resource is player-only.
        BattlerStats stats{};
        stats.hp     = sd.hp;
        stats.maxHp  = sd.hp;
        stats.mp     = 0;
        stats.maxMp  = 0;
        stats.atk    = sd.atk;
        stats.def    = sd.def;
        stats.spd    = sd.spd;
        stats.rage   = 0;
        stats.maxRage= 0;

        mEnemies.push_back(std::make_unique<EnemyCombatant>(slotName, stats, sd.attackJsonPath));
    }

    Log("--- BATTLE START ---");
    for (auto& p : mPlayers) Log(p->GetName() + " HP:" + std::to_string(p->GetStats().hp));
    for (auto& e : mEnemies) Log(e->GetName() + " HP:" + std::to_string(e->GetStats().hp));

    BuildTurnOrder();
}

// ------------------------------------------------------------
// BuildTurnOrder: Initialize tick-based timeline sorted by AV descending (so smallest AV is first).
// ------------------------------------------------------------
void BattleManager::BuildTurnOrder()
{
    mTimeline.clear();

    auto addBattler = [this](IBattler* b) {
        if (b && b->IsAlive()) {
            float spd = static_cast<float>(b->GetStats().spd);
            if (spd <= 0.0f) spd = 1.0f; // Prevent division by zero
            
            TurnNode node;
            node.battler = b;
            node.baseAgility = spd;
            node.currentAV = kActionGauge / spd;
            mTimeline.push_back(node);
        }
    };

    for (auto& p : mPlayers) addBattler(p.get());
    for (auto& e : mEnemies) addBattler(e.get());

    std::sort(mTimeline.begin(), mTimeline.end(),
        [](const TurnNode& a, const TurnNode& b) {
            return a.currentAV < b.currentAV;
        });

    if (mTimeline.empty()) return;

    // Advance time for the very first turn so the first combatant reaches 0 AV
    float elapsedAV = mTimeline[0].currentAV;
    for (auto& node : mTimeline) node.currentAV -= elapsedAV;

    IBattler* next = mTimeline[0].battler;
    next->OnTurnStart();
    Log("--- " + next->GetName() + "'s turn ---");

    if (next->IsPlayerControlled())
        mPhase = BattlePhase::PLAYER_TURN;
    else
        mPhase = BattlePhase::ENEMY_TURN;
}

// ------------------------------------------------------------
// Update: main FSM tick.  Called by BattleState every frame.
//
// Note: INIT phase is no longer handled here.
//   BattleState::OnEnter() calls Initialize(encounter) explicitly before
//   the first Update(), so mPhase is already PLAYER_TURN on entry.
//   The INIT guard is kept only as a safety net for unexpected resets.
// ------------------------------------------------------------
void BattleManager::Update(float dt)
{
    if (mPhase == BattlePhase::WIN || mPhase == BattlePhase::LOSE) return;
    if (mPhase == BattlePhase::INIT) return;  // should not happen; Initialize(encounter) called in OnEnter

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
// AdvanceTurn: push the current combatant back and advance time.
// ------------------------------------------------------------
void BattleManager::AdvanceTurn()
{
    // 1. Remove dead combatants from the timeline
    mTimeline.erase(std::remove_if(mTimeline.begin(), mTimeline.end(),
        [](const TurnNode& n) { return !n.battler->IsAlive(); }), mTimeline.end());

    if (mTimeline.empty()) return;

    // 2. Reset AV for the combatant who just acted (should be at timeline index 0 with AV near 0)
    if (mTimeline[0].currentAV <= 0.001f)
    {
        float spd = static_cast<float>(mTimeline[0].battler->GetStats().spd);
        if (spd <= 0.0f) spd = 1.0f;
        mTimeline[0].currentAV = kActionGauge / spd;
    }

    // 3. Sort timeline by AV (smallest first)
    std::sort(mTimeline.begin(), mTimeline.end(),
        [](const TurnNode& a, const TurnNode& b) {
            return a.currentAV < b.currentAV;
        });

    // 4. Advance time globally so the next combatant reaches 0 AV
    float elapsedAV = mTimeline[0].currentAV;
    for (auto& node : mTimeline) node.currentAV -= elapsedAV;

    // 5. Start their turn
    IBattler* next = mTimeline[0].battler;
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

        mQueue.Enqueue(std::move(finalAction));
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
    if (mTimeline.empty()) return nullptr;
    IBattler* c = mTimeline[0].battler;
    if (c && c->IsPlayerControlled()) return static_cast<PlayerCombatant*>(c);
    return nullptr;
}

IBattler* BattleManager::GetActiveEnemy() const
{
    if (mTimeline.empty()) return nullptr;
    IBattler* c = mTimeline[0].battler;
    if (c && !c->IsPlayerControlled()) return c;
    return nullptr;
}

IBattler* BattleManager::CurrentCombatant()
{
    if (mTimeline.empty()) return nullptr;
    return mTimeline[0].battler;
}

std::vector<IBattler*> BattleManager::GetFutureTurnQueue(int queueSize) const
{
    std::vector<IBattler*> queueOut;
    if (mTimeline.empty()) return queueOut;

    // We clone the timeline to safely advance and predict turns
    std::vector<TurnNode> simTimeline;
    for (const auto& node : mTimeline)
    {
        if (node.battler->IsAlive())
        {
            simTimeline.push_back(node);
        }
    }

    if (simTimeline.empty()) return queueOut;

    // Simulate to retrieve the next `queueSize` valid active turns.
    for (int i = 0; i < queueSize; ++i)
    {
        if (simTimeline.empty()) break;
        
        // Push the character presently holding AV 0 
        TurnNode& activeNode = simTimeline[0];
        queueOut.push_back(activeNode.battler);
        
        // Give them a new AV simulating they just took a turn
        float spd = static_cast<float>(activeNode.battler->GetStats().spd);
        if (spd <= 0.0f) spd = 1.0f;
        activeNode.currentAV += (kActionGauge / spd);
        
        // Re-sort the timeline
        std::sort(simTimeline.begin(), simTimeline.end(),
            [](const TurnNode& a, const TurnNode& b) {
                return a.currentAV < b.currentAV;
            });
            
        // Advance time forward so the new earliest node hits 0 AV
        float elapsedAV = simTimeline[0].currentAV;
        for (auto& n : simTimeline) {
            n.currentAV -= elapsedAV;
        }
    }
    
    return queueOut;
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
