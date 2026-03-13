// ============================================================
// File: BattleState.cpp
// Responsibility: Turn-based battle IGameState — drives BattleManager,
//                handles input via a three-phase FSM, renders combatant
//                sprites and the HP bar UI.
// ============================================================
#include "BattleState.h"
#include "../States/StateManager.h"
#include "../Events/EventManager.h"
#include "../Systems/PartyManager.h"
#include "../UI/BattleDebugHUD.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"
#include <string>
#include <algorithm>
#include <array>

// Battle background — dark navy blue, distinct from the play-world color.
static constexpr float kBgR = 0.05f;
static constexpr float kBgG = 0.05f;
static constexpr float kBgB = 0.20f;

BattleState::BattleState(D3DContext& d3d, EnemyEncounterData encounter)
    : mD3D(d3d)
    , mEncounter(std::move(encounter))
    , mInputController(*this, mBattle, mBattleRenderer)
{}

void BattleState::OnEnter()
{
    LOG("%s", "[BattleState] OnEnter");

    InitAudio();

    mBattle.Initialize(mEncounter);
    mInputController.Initialize();

    InitBattleSlots();
    InitUIRenderers();

    DumpStateToDebugOutput();

    if (mIris.Initialize(mD3D.GetDevice(), mD3D.GetWidth(), mD3D.GetHeight()))
    {
        mIris.StartOpen(800.0f);
    }
    else
    {
        LOG("%s", "[BattleState] WARNING — IrisTransitionRenderer init failed.");
    }
}

void BattleState::InitAudio()
{
    EventManager::Get().Broadcast("bgm_play_battle", {});
}

void BattleState::InitBattleSlots()
{
    JsonLoader::FormationData formation{};
    if (!JsonLoader::LoadFormations("data/formations.json", formation))
    {
        LOG("%s", "[BattleState] WARNING: failed to load formations.json — slots will be at origin.");
    }

    const float battleCenterX = 0.0f;
    const float battleCenterY = 0.0f;

    std::array<BattleRenderer::SlotInfo, BattleRenderer::kMaxSlots> playerSlots{};
    playerSlots[0].occupied    = true;
    playerSlots[0].texturePath = L"assets/animations/verso.png";
    playerSlots[0].jsonPath    = "assets/animations/verso.json";
    playerSlots[0].startClip   = "idle";
    playerSlots[0].worldX             = battleCenterX + formation.player[0].offsetX;
    playerSlots[0].worldY             = battleCenterY + formation.player[0].offsetY;
    playerSlots[0].cameraFocusOffsetY = -128.0f;
    playerSlots[0].cameraFocusOffsetX = 100.0f;

    std::array<BattleRenderer::SlotInfo, BattleRenderer::kMaxSlots> enemySlots{};

    if (!mEncounter.battleParty.empty())
    {
        for (int i = 0;
             i < static_cast<int>(mEncounter.battleParty.size())
             && i < static_cast<int>(BattleRenderer::kMaxSlots);
             ++i)
        {
            const EnemySlotData& sd      = mEncounter.battleParty[i];
            enemySlots[i].occupied       = true;
            enemySlots[i].texturePath    = sd.texturePath;
            enemySlots[i].jsonPath       = sd.jsonPath;
            enemySlots[i].startClip      = sd.idleClip;
            enemySlots[i].cameraFocusOffsetY = sd.cameraFocusOffsetY;

            enemySlots[i].clipOverrides[static_cast<int>(CombatantAnim::Idle)]   = sd.idleClip;
            enemySlots[i].clipOverrides[static_cast<int>(CombatantAnim::Attack)] = sd.attackClip;
            enemySlots[i].clipOverrides[static_cast<int>(CombatantAnim::Walk)]   = sd.walkClip;
            enemySlots[i].clipOverrides[static_cast<int>(CombatantAnim::Die)]    = sd.dieClip;
            enemySlots[i].clipOverrides[static_cast<int>(CombatantAnim::Hurt)]   = sd.hurtClip;

            enemySlots[i].worldX         = battleCenterX + formation.enemy[i].offsetX;
            enemySlots[i].worldY         = battleCenterY + formation.enemy[i].offsetY;
        }
    }
    else
    {
        enemySlots[0].occupied           = true;
        enemySlots[0].texturePath        = L"assets/animations/skeleton.png";
        enemySlots[0].jsonPath           = "assets/animations/skeleton.json";
        enemySlots[0].startClip          = "idle";
        enemySlots[0].cameraFocusOffsetY = -128.0f;
        enemySlots[0].worldX             = battleCenterX + formation.enemy[0].offsetX;
        enemySlots[0].worldY             = battleCenterY + formation.enemy[0].offsetY;
    }

    mBattleRenderer.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        playerSlots,
        enemySlots,
        mD3D.GetWidth(),
        mD3D.GetHeight()
    );
}

void BattleState::InitUIRenderers()
{
    mHealthBar.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/UI/UI_hp_background.png",
        L"assets/UI/UI_verso_hp.png",
        "assets/UI/HP_description.json",
        mD3D.GetWidth(),
        mD3D.GetHeight()
    );

    const auto& players = mBattle.GetAllPlayers();
    if (!players.empty())
    {
        mHealthBar.SetMaxHP(static_cast<float>(players[0]->GetStats().maxHp));
        mHealthBar.SetHP   (static_cast<float>(players[0]->GetStats().hp));
    }

    mEnemyHpBar.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/UI/enemy-hp-ui-background.png",
        L"assets/UI/enemy-hp-ui.png",
        "assets/UI/enemy-hp-ui.json",
        mD3D.GetWidth(),
        mD3D.GetHeight()
    );

    mTextRenderer.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/fonts/arial_16.spritefont",
        mD3D.GetWidth(),
        mD3D.GetHeight()
    );
    mEnemyHpBar.SetTextRenderer(&mTextRenderer);

    {
        const auto& enemies = mBattle.GetAllEnemies();
        for (int i = 0; i < static_cast<int>(enemies.size()) && i < EnemyHpBarRenderer::kMaxSlots; ++i)
        {
            const auto& stats = enemies[i]->GetStats();
            mEnemyHpBar.SetEnemy(
                i,
                static_cast<float>(stats.hp),
                static_cast<float>(stats.maxHp),
                enemies[i]->IsAlive()
            );
            mEnemyHpBar.SetEnemyName(i, enemies[i]->GetName());
        }
    }
}

void BattleState::OnExit()
{
    LOG("%s", "[BattleState] OnExit");

    EventManager::Get().Broadcast("bgm_play_overworld", {});

    mBattleRenderer.Shutdown();
    mHealthBar.Shutdown();
    mEnemyHpBar.Shutdown();
    mTextRenderer.Shutdown();
    mIris.Shutdown();
}

void BattleState::Update(float dt)
{
    mIris.Update(dt);

    if (mPendingSafeExit)
    {
        if (!mExitEventName.empty())
        {
            EventManager::Get().Broadcast(mExitEventName, {});
        }

        StateManager::Get().PopState();
        return;
    }

    if (!mExitTransitionStarted)
    {
        UpdateLogic(dt);
    }
}

void BattleState::UpdateLogic(float dt)
{
    const BattlePhase phaseBefore = mBattle.GetPhase();

    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        mInputController.HandleInput();
    }

    mBattle.Update(dt);

    CheckDeathAnimations();

    const BattlePhase phaseAfter = mBattle.GetPhase();

    if (phaseBefore != BattlePhase::PLAYER_TURN &&
        phaseAfter  == BattlePhase::PLAYER_TURN)
    {
        SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
    }
    else if (phaseBefore != phaseAfter)
    {
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::OVERVIEW, -1, -1);
        DumpStateToDebugOutput();
    }

    bool playerSelected = (phaseAfter == BattlePhase::PLAYER_TURN && 
                            mInputController.GetInputPhase() == PlayerInputPhase::SKILL_SELECT);

    IBattler* targetedEnemyPtr = nullptr;
    if (phaseAfter == BattlePhase::PLAYER_TURN && mInputController.GetInputPhase() == PlayerInputPhase::TARGET_SELECT)
    {
        const auto aliveEnemies = mBattle.GetAliveEnemies();
        if (mInputController.GetTargetIndex() >= 0 && mInputController.GetTargetIndex() < static_cast<int>(aliveEnemies.size()))
        {
            targetedEnemyPtr = aliveEnemies[mInputController.GetTargetIndex()];
        }
    }

    UpdateUIRenderers(dt, targetedEnemyPtr, playerSelected);

    mBattleRenderer.Update(dt);

    CheckBattleEnd();
}

void BattleState::CheckDeathAnimations()
{
    const auto& enemies = mBattle.GetAllEnemies();
    for (int i = 0; i < static_cast<int>(enemies.size()) && i < BattleRenderer::kMaxSlots; ++i)
    {
        const bool nowAlive = enemies[i]->IsAlive();
        if (mEnemyWasAlive[i] && !nowAlive)
        {
            mBattleRenderer.PlayEnemyClip(i, CombatantAnim::Die);
            LOG("[BattleState] Enemy slot %d died — playing die animation.", i);
        }
        mEnemyWasAlive[i] = nowAlive;
    }

    const auto& players = mBattle.GetAllPlayers();
    for (int i = 0; i < static_cast<int>(players.size()) && i < BattleRenderer::kMaxSlots; ++i)
    {
        const bool nowAlive = players[i]->IsAlive();
        if (mPlayerWasAlive[i] && !nowAlive)
        {
            mBattleRenderer.PlayPlayerClip(i, CombatantAnim::Die);
            LOG("[BattleState] Player slot %d died — playing die animation.", i);
        }
        mPlayerWasAlive[i] = nowAlive;
    }
}

void BattleState::UpdateUIRenderers(float dt, IBattler* targetedEnemyPtr, bool playerSelected)
{
    mHealthBar.SetTargetScale(playerSelected ? 1.2f : 1.0f);
    mHealthBar.Update(dt);

    const auto& enemies = mBattle.GetAllEnemies();
    for (int i = 0; i < static_cast<int>(enemies.size()) && i < EnemyHpBarRenderer::kMaxSlots; ++i)
    {
        bool enemySelected = (targetedEnemyPtr == enemies[i]);
        mEnemyHpBar.SetTargetScale(i, enemySelected ? 1.05f : 1.0f);

        const auto& stats = enemies[i]->GetStats();
        mEnemyHpBar.SetEnemy(
            i,
            static_cast<float>(stats.hp),
            static_cast<float>(stats.maxHp),
            enemies[i]->IsAlive()
        );
    }
    mEnemyHpBar.Update(dt);
}

void BattleState::CheckBattleEnd()
{
    const BattleOutcome outcome = mBattle.GetOutcome();
    if (outcome != BattleOutcome::NONE && !mExitTransitionStarted && !mWaitingForDeathAnims)
    {
        const auto& players = mBattle.GetAllPlayers();
        if (!players.empty())
        {
            PartyManager::Get().SetVersoStats(players[0]->GetStats());
            LOG("[BattleState] Saved Verso HP: %d/%d",
                players[0]->GetStats().hp, players[0]->GetStats().maxHp);
        }

        mExitEventName = (outcome == BattleOutcome::VICTORY) ? "battle_end_victory" : "battle_end_defeat";
        mWaitingForDeathAnims = true;
        LOG("[BattleState] Outcome detected (%s) — waiting for death animations.", mExitEventName.c_str());
    }

    if (mWaitingForDeathAnims && mBattleRenderer.AreAllDeathAnimsDone())
    {
        mWaitingForDeathAnims  = false;
        mExitTransitionStarted = true;

        mIris.StartClose([this]() { mPendingSafeExit = true; }, 600.0f);
        LOG("[BattleState] Death animations done — starting iris close.");
    }

    if (mPendingFlee && !mExitTransitionStarted)
    {
        mPendingFlee           = false;
        mExitTransitionStarted = true;
        mExitEventName         = "battle_flee";

        mIris.StartClose([this]() { mPendingSafeExit = true; }, 600.0f);
        LOG("%s", "[BattleState] Flee requested — closing iris.");
    }
}

void BattleState::Render()
{
    mD3D.BeginFrame(kBgR, kBgG, kBgB);
    mBattleRenderer.Render(mD3D.GetContext());
    mHealthBar.Render(mD3D.GetContext());
    mEnemyHpBar.Render(mD3D.GetContext());
    mIris.Render(mD3D.GetContext());
}

void BattleState::DumpStateToDebugOutput() const
{
    BattleHUDSnapshot snap;
    snap.title = "BATTLE STATE";

    switch (mBattle.GetPhase())
    {
    case BattlePhase::INIT:        snap.simulationPhase = "INIT";        break;
    case BattlePhase::PLAYER_TURN: snap.simulationPhase = "PLAYER_TURN"; break;
    case BattlePhase::RESOLVING:   snap.simulationPhase = "RESOLVING";   break;
    case BattlePhase::ENEMY_TURN:  snap.simulationPhase = "ENEMY_TURN";  break;
    case BattlePhase::WIN:         snap.simulationPhase = "WIN";         break;
    case BattlePhase::LOSE:        snap.simulationPhase = "LOSE";        break;
    default:                       snap.simulationPhase = "UNKNOWN";     break;
    }

    auto inputPhase = mInputController.GetInputPhase();

    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        switch (inputPhase)
        {
        case PlayerInputPhase::COMMAND_SELECT: snap.inputPhase = "COMMAND_SELECT"; break;
        case PlayerInputPhase::SKILL_SELECT:   snap.inputPhase = "SKILL_SELECT";   break;
        case PlayerInputPhase::TARGET_SELECT:  snap.inputPhase = "TARGET_SELECT";  break;
        }
    }

    if (inputPhase == PlayerInputPhase::COMMAND_SELECT && mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        const auto& commands = mInputController.GetCommands();
        for (int i = 0; i < static_cast<int>(commands.size()); ++i)
        {
            BattleHUDSnapshot::MenuItem item;
            item.label    = commands[i]->GetLabel();
            item.selected = (i == mInputController.GetCommandIndex());
            snap.menuItems.push_back(item);
        }
    }

    if ((inputPhase == PlayerInputPhase::SKILL_SELECT || inputPhase == PlayerInputPhase::TARGET_SELECT) &&
        mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        const PlayerCombatant* player = mBattle.GetActivePlayer();
        if (player)
        {
            const int count = player->GetSkillCount();
            for (int i = 0; i < count; ++i)
            {
                const ISkill* skill = player->GetSkill(i);
                if (!skill) continue;

                BattleHUDSnapshot::SkillRow row;
                row.slot        = i + 1;
                row.name        = skill->GetName();
                row.description = skill->GetDescription();
                row.available   = skill->CanUse(*static_cast<const IBattler*>(player));
                row.selected    = (i == mInputController.GetSkillIndex());
                snap.skillRows.push_back(row);
            }
        }
    }

    if (inputPhase == PlayerInputPhase::TARGET_SELECT && mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        const auto enemies = mBattle.GetAliveEnemies();
        if (mInputController.GetTargetIndex() < static_cast<int>(enemies.size()))
        {
            snap.infoLines.push_back({ "Target", enemies[mInputController.GetTargetIndex()]->GetName() });
            snap.infoLines.push_back({ "Hint", "Tab / Down = next target  Up = prev target  Enter = confirm  Esc = back" });
        }

        const PlayerCombatant* player = mBattle.GetActivePlayer();
        if (player)
        {
            const ISkill* skill = player->GetSkill(mInputController.GetSkillIndex());
            snap.infoLines.push_back({ "Skill", skill ? skill->GetName() : "(none)" });
        }
    }

    const IBattler* activeCombatant = mBattle.GetActivePlayer();
    if (!activeCombatant)
        activeCombatant = mBattle.GetActiveEnemy();

    for (IBattler* p : mBattle.GetAllPlayers())
    {
        const auto& s = p->GetStats();
        BattleHUDSnapshot::CombatantRow row;
        row.tag          = "[PLAYER]";
        row.name         = p->GetName();
        row.isCurrentTurn = (p == activeCombatant);
        row.hp      = s.hp;      row.maxHp   = s.maxHp;
        row.rage    = s.rage;    row.maxRage = s.maxRage;
        row.atk     = s.atk;     row.def     = s.def;
        row.spd     = s.spd;     row.alive   = p->IsAlive();
        snap.combatants.push_back(row);
    }
    for (IBattler* e : mBattle.GetAllEnemies())
    {
        const auto& s = e->GetStats();
        BattleHUDSnapshot::CombatantRow row;
        row.tag           = "[ENEMY ]";
        row.name          = e->GetName();
        row.isCurrentTurn = (e == activeCombatant);
        row.hp      = s.hp;   row.maxHp = s.maxHp;
        row.atk     = s.atk;  row.def   = s.def;
        row.spd     = s.spd;  row.alive = e->IsAlive();
        snap.combatants.push_back(row);
    }

    snap.logLines = mBattle.GetBattleLog();

    BattleDebugHUD::Render(snap);
}

