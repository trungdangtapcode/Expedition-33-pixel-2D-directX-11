// ============================================================
// File: BattleState.cpp
// Responsibility: Turn-based battle IGameState — drives BattleManager,
//                handles input via a three-phase FSM, renders combatant
//                sprites and the HP bar UI.
// ============================================================
#include "BattleState.h"
#include "../Battle/BattleEvents.h"
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

    
    mPlayAnimListener = EventManager::Get().Subscribe("battler_play_anim", [this](const EventData& e){ OnPlayAnim(e); });
    mIsAnimDoneListener = EventManager::Get().Subscribe("battler_is_anim_done", [this](const EventData& e){ OnIsAnimDone(e); });
    mGetAnimProgressListener = EventManager::Get().Subscribe("battler_get_anim_progress", [this](const EventData& e){ OnGetAnimProgress(e); });
    mMoveOffsetListener = EventManager::Get().Subscribe("battler_set_offset", [this](const EventData& e){ OnMoveOffset(e); });
    mGetWorldPosListener = EventManager::Get().Subscribe("battler_get_world_pos", [this](const EventData& e){ OnGetWorldPos(e); });
    mGetOffsetListener = EventManager::Get().Subscribe("battler_get_offset", [this](const EventData& e){ OnGetOffset(e); });

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
    // Load UI data constants so we're not hardcoding layout numbers
    if (!JsonLoader::LoadBattleMenuLayout("data/battle_menu_layout.json", mMenuLayout))
    {
        LOG("[BattleState] WARNING — failed to load data/battle_menu_layout.json");
    }

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

    mTargetPointer.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/UI/enemy-pointer-ui.png",
        "assets/UI/enemy-pointer-ui.json",
        mD3D.GetWidth(),
        mD3D.GetHeight()
    );

    mDialogBox.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/UI/ui-dialog-box-hd.png",
        "assets/UI/ui-dialog-box-hd.json",
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

        EventManager::Get().Unsubscribe("battler_play_anim", mPlayAnimListener);
    EventManager::Get().Unsubscribe("battler_is_anim_done", mIsAnimDoneListener);
    EventManager::Get().Unsubscribe("battler_get_anim_progress", mGetAnimProgressListener);
    EventManager::Get().Unsubscribe("battler_set_offset", mMoveOffsetListener);
    EventManager::Get().Unsubscribe("battler_get_world_pos", mGetWorldPosListener);
    EventManager::Get().Unsubscribe("battler_get_offset", mGetOffsetListener);
    mBattleRenderer.Shutdown();
    mHealthBar.Shutdown();
    mEnemyHpBar.Shutdown();
    mTargetPointer.Shutdown();
    mDialogBox.Shutdown();
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
        mCmdMenuTimer = 0.0f;
    }
    else if (phaseBefore != phaseAfter)
    {
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::OVERVIEW, -1, -1);
        DumpStateToDebugOutput();
    }

    PlayerInputPhase currentInputPhase = mInputController.GetInputPhase();
    if (mLastInputPhase != currentInputPhase)
    {
        if (currentInputPhase == PlayerInputPhase::COMMAND_SELECT) mCmdMenuTimer = 0.0f;
        if (currentInputPhase == PlayerInputPhase::SKILL_SELECT) mSkillMenuTimer = 0.0f;
        mLastInputPhase = currentInputPhase;
    }

    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        mCmdMenuTimer += dt;
        mSkillMenuTimer += dt;
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

    // ------------------------------------------------------------
    // Update Combat Stance State
    // ------------------------------------------------------------
    const PlayerCombatant* activeP = mBattle.GetActivePlayer();
    const auto& players = mBattle.GetAllPlayers();
    for (int i = 0; i < static_cast<int>(players.size()) && i < BattleRenderer::kMaxSlots; ++i)
    {
        bool inStance = false;
        if (players[i] == activeP && activeP->IsAlive())
        {
            if (phaseAfter == BattlePhase::PLAYER_TURN)
            {
                auto inputPhase = mInputController.GetInputPhase();
                if (inputPhase == PlayerInputPhase::SKILL_SELECT || 
                    inputPhase == PlayerInputPhase::TARGET_SELECT)
                {
                    inStance = true;
                }
            }
            else if (phaseAfter == BattlePhase::RESOLVING)
            {
                // Action is playing out
                inStance = true;
            }
        }
        mBattleRenderer.SetPlayerFightStance(i, inStance);
    }

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
            mBattleRenderer.SetPlayerStanceEnabled(i, false);
            mBattleRenderer.PlayPlayerClip(i, CombatantAnim::Die);
            LOG("[BattleState] Player slot %d died — playing die animation.", i);
        }
        mPlayerWasAlive[i] = nowAlive;
    }
}

void BattleState::UpdateUIRenderers(float dt, IBattler* targetedEnemyPtr, bool playerSelected)
{
    mHealthBar.SetTargetScale(playerSelected ? 1.25f : 1.0f);
    mHealthBar.Update(dt);
    mTargetPointer.Update(dt);

    const auto& enemies = mBattle.GetAllEnemies();
    for (int i = 0; i < static_cast<int>(enemies.size()) && i < EnemyHpBarRenderer::kMaxSlots; ++i)
    {
        bool enemySelected = (targetedEnemyPtr == enemies[i]);
        mEnemyHpBar.SetTargetScale(i, enemySelected ? 1.05f : 1.0f);

        const auto& stats = enemies[i]->GetStats();
        
        bool active = enemies[i]->IsAlive() || !mBattleRenderer.IsEnemyClipDone(i);

        mEnemyHpBar.SetEnemy(
            i,
            static_cast<float>(stats.hp),
            static_cast<float>(stats.maxHp),
            active
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

    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN && 
        mInputController.GetInputPhase() == PlayerInputPhase::COMMAND_SELECT)
    {
        const auto& commands = mInputController.GetCommands();
        const int commandCount = static_cast<int>(commands.size());
        const int hoveredIndex = mInputController.GetCommandIndex();

        // Screen-space metrics for bottom-left anchoring
        const float screenW = static_cast<float>(mD3D.GetWidth());
        const float screenH = static_cast<float>(mD3D.GetHeight());

        const float baseDialogWidth = mMenuLayout.command.width;
        const float baseDialogHeight = mMenuLayout.command.height;
        
        // Offset from bottom-left corner
        const float paddingLeft = mMenuLayout.command.paddingLeft;
        const float paddingBottom = mMenuLayout.command.paddingBottom;
        const float itemSpacing = mMenuLayout.command.spacing;
        const float hoverScale = mMenuLayout.command.hoverScale;
        const float sliceScale = mMenuLayout.command.sliceScale;
        
        // Calculate Base Y so the bottom of the list aligns with paddingBottom
        const float startX = paddingLeft;
        const float totalHeight = commandCount * (baseDialogHeight + itemSpacing);
        const float startY = screenH - paddingBottom - totalHeight;

        // Render in Screen Space (Identity Matrix)
        auto identityMatrix = DirectX::XMMatrixIdentity();

        // Ease interpolation (cubic ease-out)
        float activeCmdTimer = (std::max)(0.0f, mCmdMenuTimer - mMenuLayout.command.entryDelay);
        float t = mMenuLayout.command.entryDuration > 0.f ? (std::min)(activeCmdTimer / mMenuLayout.command.entryDuration, 1.0f) : 1.0f;
        float easeT = 1.0f - std::pow(1.0f - t, 3.0f);
        float slideOffset = mMenuLayout.command.slideOffsetX * (1.0f - easeT);
        float currentAlpha = mMenuLayout.command.fadeStartAlpha + (1.0f - mMenuLayout.command.fadeStartAlpha) * easeT;
        DirectX::XMVECTOR dboxColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, currentAlpha);

        for (int i = 0; i < commandCount; ++i)
        {
            bool isHovered = (i == hoveredIndex);
            float scaleMultiplier = isHovered ? hoverScale : 1.0f;

            float dialogWidth = baseDialogWidth * scaleMultiplier;
            float dialogHeight = baseDialogHeight * scaleMultiplier;

            // Shift by half the difference to scale from center
            float offsetX = (dialogWidth - baseDialogWidth) / 2.0f;
            float offsetY = (dialogHeight - baseDialogHeight) / 2.0f;

            float dialogX = startX - offsetX - slideOffset;
            float dialogY = startY + (i * (baseDialogHeight + itemSpacing)) - offsetY;

            // Draw 9-slice in SCREEN SPACE
            mDialogBox.Draw(
                mD3D.GetContext(),
                dialogX, dialogY,
                dialogWidth, dialogHeight,
                sliceScale * scaleMultiplier,
                identityMatrix,
                dboxColor
            );

            float textX = dialogX + mMenuLayout.command.textOffsetX * scaleMultiplier;
            float textY = dialogY + mMenuLayout.command.textOffsetY * scaleMultiplier;

            DirectX::XMVECTOR textColor = isHovered ? DirectX::Colors::Yellow : DirectX::Colors::White;
            textColor = DirectX::XMVectorSetW(textColor, currentAlpha);

            // Draw Text (SCREEN SPACE)
            mTextRenderer.DrawString(
                mD3D.GetContext(),
                commands[i]->GetLabel(),
                textX, textY,
                textColor,
                identityMatrix
            );
        }
    }

    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN && 
        mInputController.GetInputPhase() == PlayerInputPhase::SKILL_SELECT)
    {
        const PlayerCombatant* activePlayer = mBattle.GetActivePlayer();
        const auto& players = mBattle.GetAllPlayers();
        int slotIndex = 0;
        for (int i = 0; i < static_cast<int>(players.size()); ++i)
        {
            if (players[i] == activePlayer) { slotIndex = i; break; }
        }

        float worldX, worldY;
        mBattleRenderer.GetPlayerSlotPos(slotIndex, worldX, worldY);

        auto cameraMatrix = mBattleRenderer.GetCamera().GetViewMatrix();

        if (activePlayer)
        {
            const int skillCount = activePlayer->GetSkillCount();
            const int hoveredIndex = mInputController.GetSkillIndex();

            const float baseDialogWidth = mMenuLayout.skill.width;
            const float baseDialogHeight = mMenuLayout.skill.height;
            const float itemSpacing = mMenuLayout.skill.spacing;
            const float hoverScale = mMenuLayout.skill.hoverScale;
            const float sliceScale = mMenuLayout.skill.sliceScale;

            // Position right from character, centered vertically based on skill count
            const float baseX = worldX + mMenuLayout.skill.offsetX;
            const float totalHeight = skillCount * (baseDialogHeight + itemSpacing);
            const float baseY = worldY + mMenuLayout.skill.offsetY - (totalHeight / 2.0f);

            // Ease interpolation (cubic ease-out)
            float activeSkillTimer = (std::max)(0.0f, mSkillMenuTimer - mMenuLayout.skill.entryDelay);
            float t = mMenuLayout.skill.entryDuration > 0.f ? (std::min)(activeSkillTimer / mMenuLayout.skill.entryDuration, 1.0f) : 1.0f;
            float easeT = 1.0f - std::pow(1.0f - t, 3.0f);
            float slideOffset = mMenuLayout.skill.slideOffsetX * (1.0f - easeT);
            float currentAlpha = mMenuLayout.skill.fadeStartAlpha + (1.0f - mMenuLayout.skill.fadeStartAlpha) * easeT;
            DirectX::XMVECTOR dboxColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, currentAlpha);

            for (int i = 0; i < skillCount; ++i)
            {
                const ISkill* skill = activePlayer->GetSkill(i);
                if (!skill) continue;

                bool isHovered = (i == hoveredIndex);
                float scaleMultiplier = isHovered ? hoverScale : 1.0f;

                float dialogWidth = baseDialogWidth * scaleMultiplier;
                float dialogHeight = baseDialogHeight * scaleMultiplier;

                // Shift by half the difference to scale from center
                float offsetX = (dialogWidth - baseDialogWidth) / 2.0f;
                float offsetY = (dialogHeight - baseDialogHeight) / 2.0f;

                float dialogX = baseX - offsetX - slideOffset;
                float dialogY = baseY + (i * (baseDialogHeight + itemSpacing)) - offsetY;

                // Draw 9-slice in WORLD SPACE
                mDialogBox.Draw(
                    mD3D.GetContext(),
                    dialogX, dialogY,
                    dialogWidth, dialogHeight,
                    sliceScale * scaleMultiplier,
                    cameraMatrix,
                    dboxColor
                );

                float textX = dialogX + mMenuLayout.skill.textOffsetX * scaleMultiplier;
                float textY = dialogY + mMenuLayout.skill.textOffsetY * scaleMultiplier;

                bool canUse = skill->CanUse(*static_cast<const IBattler*>(activePlayer));
                DirectX::XMVECTOR textColor = canUse ? DirectX::Colors::White : DirectX::Colors::Gray;
                if (isHovered) {
                    textColor = canUse ? DirectX::Colors::Yellow : DirectX::Colors::Orange;
                }
                textColor = DirectX::XMVectorSetW(textColor, currentAlpha);

                // Draw Text inside the dialog box (WORLD SPACE)
                mTextRenderer.DrawString(
                    mD3D.GetContext(),
                    skill->GetName(),
                    textX, textY,
                    textColor,
                    cameraMatrix
                );
            }
        }
    }

    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN && mInputController.GetInputPhase() == PlayerInputPhase::TARGET_SELECT)
    {
        const auto aliveEnemies = mBattle.GetAliveEnemies();
        int targetIdx = mInputController.GetTargetIndex();
        if (targetIdx >= 0 && targetIdx < static_cast<int>(aliveEnemies.size()))
        {
            IBattler* targetedEnemyPtr = aliveEnemies[targetIdx];
            const auto& allEnemies = mBattle.GetAllEnemies();
            int slotIndex = 0;
            for (int i = 0; i < static_cast<int>(allEnemies.size()); ++i)
            {
                if (allEnemies[i] == targetedEnemyPtr) { slotIndex = i; break; }
            }

            float worldX, worldY;
            mBattleRenderer.GetEnemySlotPos(slotIndex, worldX, worldY);
            
            auto cameraMatrix = mBattleRenderer.GetCamera().GetViewMatrix();
            mTargetPointer.Draw(mD3D.GetContext(), worldX, worldY, cameraMatrix);
        }
    }

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



bool BattleState::GetBattlerSlot(IBattler* target, int& outSlot, bool& outIsPlayer) const
{
    if (!target) return false;
    const auto& players = mBattle.GetAllPlayers();
    for (int i = 0; i < (int)players.size() && i < BattleRenderer::kMaxSlots; ++i) {
        if (players[i] == target) {
            outSlot = i; outIsPlayer = true; return true;
        }
    }
    const auto& enemies = mBattle.GetAllEnemies();
    for (int i = 0; i < (int)enemies.size() && i < BattleRenderer::kMaxSlots; ++i) {
        if (enemies[i] == target) {
            outSlot = i; outIsPlayer = false; return true;
        }
    }
    return false;
}

void BattleState::OnPlayAnim(const EventData& e)
{
    auto p = static_cast<PlayAnimPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) mBattleRenderer.PlayPlayerClip(slot, p->anim);
        else mBattleRenderer.PlayEnemyClip(slot, p->anim);
    }
}

void BattleState::OnIsAnimDone(const EventData& e)
{
    auto p = static_cast<IsAnimDonePayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) p->isDone = mBattleRenderer.IsPlayerClipDone(slot);
        else p->isDone = mBattleRenderer.IsEnemyClipDone(slot);
    }
}

void BattleState::OnGetAnimProgress(const EventData& e)
{
    auto p = static_cast<GetAnimProgressPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) p->progress = mBattleRenderer.GetPlayerClipProgress(slot);
        else p->progress = mBattleRenderer.GetEnemyClipProgress(slot);
    } else {
        p->progress = 1.0f;
    }
}

void BattleState::OnMoveOffset(const EventData& e)
{
    auto p = static_cast<MoveOffsetPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) mBattleRenderer.SetPlayerDrawOffset(slot, p->offsetX, p->offsetY);
        else mBattleRenderer.SetEnemyDrawOffset(slot, p->offsetX, p->offsetY);
    }
}

void BattleState::OnGetWorldPos(const EventData& e)
{
    auto p = static_cast<GetWorldPosPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) mBattleRenderer.GetPlayerSlotPos(slot, p->x, p->y);
        else mBattleRenderer.GetEnemySlotPos(slot, p->x, p->y);
    }
}

void BattleState::OnGetOffset(const EventData& e)
{
    auto p = static_cast<MoveOffsetPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) {
            mBattleRenderer.GetPlayerDrawOffset(slot, p->offsetX, p->offsetY);
        } else {
            mBattleRenderer.GetEnemyDrawOffset(slot, p->offsetX, p->offsetY);
        }
    }
}
