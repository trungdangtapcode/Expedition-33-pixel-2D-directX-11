// ============================================================
// File: BattleState.cpp
// Responsibility: Turn-based battle IGameState — drives BattleManager,
//                handles input via a three-phase FSM, renders combatant
//                sprites and the HP bar UI.
// ============================================================
#include "BattleState.h"
#include "../Battle/BattleEvents.h"
#include "../Battle/ItemRegistry.h"
#include "../Battle/ItemData.h"
#include "../States/StateManager.h"
#include "../Events/EventManager.h"
#include "../Systems/PartyManager.h"
#include "../Systems/Inventory.h"
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

    if (!JsonLoader::LoadBattleSystemConfig("data/battle_system_config.json", mSystemConfig)) {
        LOG("%s", "[BattleState] WARNING — failed to load data/battle_system_config.json");
    }

    mBattle.Initialize(mEncounter, mSystemConfig);
    mInputController.Initialize();

    InitBattleSlots();
    InitUIRenderers();
      
      mEnvRenderer.Initialize(mD3D.GetDevice(), mD3D.GetContext());
      std::string envPath = mEncounter.environmentPath;
      if (envPath.empty()) {
          envPath = "assets/environments/battle-paris-view.json"; // Default
      }
      mEnvRenderer.LoadEnvironment(envPath);

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
    
    const auto& activeParty = PartyManager::Get().GetActiveParty();
    for (size_t i = 0; i < activeParty.size(); ++i) {
        playerSlots[i].occupied    = true;
        playerSlots[i].texturePath = activeParty[i].animationPath;
        playerSlots[i].jsonPath    = activeParty[i].animJsonPath;
        playerSlots[i].startClip   = "idle";
        playerSlots[i].worldX      = battleCenterX + formation.player[i].offsetX;
        playerSlots[i].worldY      = battleCenterY + formation.player[i].offsetY;
        playerSlots[i].cameraFocusOffsetY = -128.0f;
        playerSlots[i].cameraFocusOffsetX = 100.0f;
    }

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
    mCameraPhaseListener = EventManager::Get().Subscribe("battler_set_camera_phase", [this](const EventData& e){ OnCameraSetPhase(e); });
    
    mDamageTakenListener = EventManager::Get().Subscribe("battler_damage_taken", [this](const EventData& e){ OnDamageTaken(e); });
    mQteUpdateListener = EventManager::Get().Subscribe("battler_qte_update", [this](const EventData& e){ OnQteFeedback(e); });
    mBulletHellStateListener = EventManager::Get().Subscribe("verso_bullet_hell_state", [this](const EventData& e){ OnBulletHellState(e); });

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

    const auto& party = PartyManager::Get().GetActiveParty();
    const auto& players = mBattle.GetAllPlayers();
    
    // Stack health bars visually mapping descending configurations vertically
    for (size_t i = 0; i < players.size(); ++i) {
        auto bar = std::make_unique<HealthBarRenderer>();
        // Using "membername_hp_changed" explicitly!
        std::string hpTopic = (party[i].name == "Verso") ? "verso_hp_changed" : party[i].name + "_hp_changed";
        
        if (bar->Initialize(
            mD3D.GetDevice(),
            mD3D.GetContext(),
            L"assets/UI/UI_hp_background.png",
            party[i].hpFramePath,
            "assets/UI/HP_description.json",
            mD3D.GetWidth(), mD3D.GetHeight(),
            hpTopic,
            mMenuLayout.partyHud.align == "bottom-right" ? 
                (mD3D.GetWidth() + mMenuLayout.partyHud.originX + ((players.size() - 1 - i) * mMenuLayout.partyHud.spacingX)) :
                (mMenuLayout.partyHud.originX + ((players.size() - 1 - i) * mMenuLayout.partyHud.spacingX)),
            mMenuLayout.partyHud.align == "bottom-right" ?
                (mD3D.GetHeight() + mMenuLayout.partyHud.originY + ((players.size() - 1 - i) * mMenuLayout.partyHud.spacingY)) :
                (mMenuLayout.partyHud.originY + ((players.size() - 1 - i) * mMenuLayout.partyHud.spacingY))
        )) {
            const BattlerStats& s = players[i]->GetStats();
            bar->SetMaxHP(static_cast<float>(s.maxHp));
            bar->SetHP(static_cast<float>(s.hp));
            mHealthBars.push_back(std::move(bar));
        } else {
            LOG("[BattleState] WARNING — HealthBar initialization failed for %s.", party[i].name.c_str());
        }
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

    mTurnQueueUI.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        "assets/UI/turn-view.json",
        L"assets/UI/turn-view-background.png",
        L"assets/UI/turn-view-frame.png",
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

    // Scroll chevrons for the item menu.  Both load the same texture;
    // the up chevron is rendered with a 180-degree rotation + inverted
    // bob direction inside ScrollArrowRenderer::Draw, so one PNG covers
    // both directions until a dedicated chevron asset is authored.
    //
    // Bob speed/amplitude are tuned smaller than the target pointer's
    // because the chevron sits next to small menu rows — a 10px bob
    // would dwarf the row height.
    mChevronDown.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/UI/enemy-pointer-ui.png",
        mD3D.GetWidth(),
        mD3D.GetHeight(),
        4.0f,   // bobSpeed (rad/s)
        4.0f    // bobAmplitude (px)
    );
    mChevronUp.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/UI/enemy-pointer-ui.png",
        mD3D.GetWidth(),
        mD3D.GetHeight(),
        4.0f,
        4.0f
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

    mQTERenderer.Initialize(mD3D.GetDevice(), mD3D.GetContext(), mD3D.GetWidth(), mD3D.GetHeight());
    mBulletHellRenderer = std::make_unique<BattleBulletHellRenderer>(mD3D.GetDevice(), mD3D.GetContext());
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
    EventManager::Get().Unsubscribe("battler_set_camera_phase", mCameraPhaseListener);
    EventManager::Get().Unsubscribe("battler_damage_taken", mDamageTakenListener);
    EventManager::Get().Unsubscribe("battler_qte_update", mQteUpdateListener);
    mBattleRenderer.Shutdown();
    for (auto& bar : mHealthBars) bar->Shutdown();
    mEnemyHpBar.Shutdown();
    mTurnQueueUI.Shutdown();
    mTargetPointer.Shutdown();
    mChevronUp.Shutdown();
    mChevronDown.Shutdown();
    mDialogBox.Shutdown();
    mTextRenderer.Shutdown();
    mQTERenderer.Shutdown();
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
    // Update floating damage texts
    for (auto it = mFloatingTexts.begin(); it != mFloatingTexts.end(); ) {
        it->lifeTimer -= dt;
        
        // Physics logic for 'thrown' effect (Gravity pulls down along +Y, so we start negative vy)
        it->vy += 800.0f * dt; 
        it->worldX += it->vx * dt;
        it->worldY += it->vy * dt;
        
        if (it->lifeTimer <= 0.0f) {
            it = mFloatingTexts.erase(it);
        } else {
            ++it;
        }
    }

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
        // Item menu reuses mSkillMenuTimer so the slide-in animation
        // restarts each time the player opens the inventory list.
        if (currentInputPhase == PlayerInputPhase::ITEM_SELECT)  mSkillMenuTimer = 0.0f;
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
    for (auto& bar : mHealthBars) {
        if (bar->IsInitialized()) bar->Update(dt);
    }
    mTurnQueueUI.Update(dt);
    mTurnQueueUI.UpdateQueue(mBattle.GetFutureTurnQueue(6));
    mTargetPointer.Update(dt);
    // Bob the scroll chevrons every frame so the loop animation runs
    // even when the cursor is idle.  Both instances share the same
    // delta-time clock; the flip flag inside Draw inverts the bob
    // direction so the up and down arrows lean in opposite directions.
    mChevronUp.Update(dt);
    mChevronDown.Update(dt);

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
    
    mQTERenderer.Update(dt);
}

void BattleState::OnCameraSetPhase(const EventData& d)
{
    const CameraPhasePayload* payload = static_cast<const CameraPhasePayload*>(d.payload);
    if (!payload) return;

    int actorSlot = -1;
    if (payload->targetToFollow)
    {
        bool isPlayer = false;
        if (GetBattlerSlot(payload->targetToFollow, actorSlot, isPlayer)) {
            // We successfully resolved the slot! 
            // Note: BattleRenderer::SetCameraPhase natively assumes that 
            // if we pass actorSlot = X, it checks mPlayerActive[X] FIRST.
            // If the target is an enemy, but the player is active at the SAME slot index,
            // BattleRenderer assigns the player. This is a known limitation of the 
            // current SetCameraPhase API format.
        } else {
            actorSlot = -1;
        }
    }

    mBattleRenderer.SetDynamicFollowZoom(payload->dynamicZoom);
    mBattleRenderer.SetCameraPhase(payload->phase, actorSlot, -1);
}

void BattleState::CheckBattleEnd()
{
    const BattleOutcome outcome = mBattle.GetOutcome();
    if (outcome != BattleOutcome::NONE && !mExitTransitionStarted && !mWaitingForDeathAnims)
    {
        const auto& players = mBattle.GetAllPlayers();
        for (size_t i = 0; i < players.size(); ++i) {
            PartyManager::Get().SetMemberStats(i, players[i]->GetStats());
            LOG("[BattleState] Saved %s HP: %d/%d",
                players[i]->GetName().c_str(), players[i]->GetStats().hp, players[i]->GetStats().maxHp);
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

    mEnvRenderer.RenderBackground(mBattleRenderer.GetCamera());
    mBattleRenderer.Render(mD3D.GetContext());
    mEnvRenderer.RenderForeground(mBattleRenderer.GetCamera());

    // UI Render
    for (auto& bar : mHealthBars) {
        if (bar->IsInitialized()) bar->Render(mD3D.GetContext());
    }
    mEnemyHpBar.Render(mD3D.GetContext());
    mTurnQueueUI.Render(mD3D.GetContext());
    
    // Draw Floating Damage Texts
    if (!mFloatingTexts.empty()) {
        mTextRenderer.BeginBatch(mD3D.GetContext(), mBattleRenderer.GetCamera().GetViewMatrix());
        for (const auto& ft : mFloatingTexts) {
            float alpha = 1.0f;
            if (ft.lifeTimer < 0.3f) {
                alpha = ft.lifeTimer / 0.3f; // Fade out near the end
            }
            DirectX::XMVECTOR fadedColor = ft.color;
            fadedColor.m128_f32[3] = alpha;

            mTextRenderer.DrawStringCenteredRaw(ft.text.c_str(), ft.worldX, ft.worldY, fadedColor, ft.scale, true);
        }
        mTextRenderer.EndBatch();
    }

    mQTERenderer.Render(mD3D.GetContext());
    if (mBulletHellRenderer) mBulletHellRenderer->Render(mD3D.GetContext(), mD3D.GetWidth(), mD3D.GetHeight());

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

                bool canUse = skill->CanUse(*static_cast<const IBattler*>(activePlayer), mBattle.GetContext());
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

    // ---- Item menu (ITEM_SELECT) ----
    // Mirrors the skill block above: world-space dialog boxes anchored
    // to the active player's slot, listing one row per OWNED item id.
    // Rendered ONLY when the inventory snapshot is non-empty so an
    // empty bag never produces stray quads on screen.
    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN &&
        (mInputController.GetInputPhase() == PlayerInputPhase::ITEM_SELECT ||
         mInputController.GetInputPhase() == PlayerInputPhase::ITEM_TARGET_SELECT))
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

        const auto& itemIds = mInputController.GetItemIds();
        if (activePlayer && !itemIds.empty())
        {
            const int itemCount    = static_cast<int>(itemIds.size());
            const int hoveredIndex = mInputController.GetItemIndex();

            // Scrolling window — show only kVisibleItems rows at once,
            // centered on the cursor when possible.  Keeps menu height
            // bounded regardless of inventory size.
            constexpr int kVisibleItems = 3;

            int first = hoveredIndex - (kVisibleItems / 2);
            const int maxFirst = (std::max)(0, itemCount - kVisibleItems);
            if (first < 0) first = 0;
            if (first > maxFirst) first = maxFirst;
            const int last = (std::min)(itemCount, first + kVisibleItems);
            const int visibleCount = last - first;

            // Reuse the skill layout block — items and skills look the same.
            const float baseDialogWidth  = mMenuLayout.skill.width;
            const float baseDialogHeight = mMenuLayout.skill.height;
            const float itemSpacing      = mMenuLayout.skill.spacing;
            const float hoverScale       = mMenuLayout.skill.hoverScale;
            const float sliceScale       = mMenuLayout.skill.sliceScale;

            // Position right of the character, vertically centered around
            // the VISIBLE window so the menu anchor stays put while scrolling.
            const float baseX = worldX + mMenuLayout.skill.offsetX;
            const float rowStride = baseDialogHeight + itemSpacing;
            const float totalHeight = visibleCount * rowStride;
            const float baseY = worldY + mMenuLayout.skill.offsetY - (totalHeight / 2.0f);

            // Slide / fade animation matches the skill menu so the
            // Fight->skills and Item->items transitions feel identical.
            float activeItemTimer = (std::max)(0.0f, mSkillMenuTimer - mMenuLayout.skill.entryDelay);
            float t = mMenuLayout.skill.entryDuration > 0.f ? (std::min)(activeItemTimer / mMenuLayout.skill.entryDuration, 1.0f) : 1.0f;
            float easeT = 1.0f - std::pow(1.0f - t, 3.0f);
            float slideOffset = mMenuLayout.skill.slideOffsetX * (1.0f - easeT);
            float currentAlpha = mMenuLayout.skill.fadeStartAlpha + (1.0f - mMenuLayout.skill.fadeStartAlpha) * easeT;
            DirectX::XMVECTOR dboxColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, currentAlpha);

            // ------------------------------------------------------------
            // Per-effect-kind icon tint.  Until per-item PNGs are authored
            // (see idea/asset-todo.md), each row's icon is a tiny tinted
            // dialog-box quad whose color encodes the effect category.
            // The tint is applied to the standard mDialogBox 9-slice so
            // we don't need a new texture.
            //
            // When real icons land, replace this color helper with a
            // texture lookup keyed by ItemData::iconPath.
            // ------------------------------------------------------------
            auto IconTint = [currentAlpha](const ItemData* item) -> DirectX::XMVECTOR
            {
                // Default fallback for missing items: neutral grey.
                if (!item) return DirectX::XMVectorSet(0.6f, 0.6f, 0.6f, currentAlpha);
                switch (item->effect)
                {
                case ItemEffectKind::HealHp:
                case ItemEffectKind::FullHeal:
                    // Green — restorative
                    return DirectX::XMVectorSet(0.40f, 0.85f, 0.40f, currentAlpha);
                case ItemEffectKind::HealMp:
                    // Blue — mana
                    return DirectX::XMVectorSet(0.40f, 0.55f, 0.95f, currentAlpha);
                case ItemEffectKind::Revive:
                    // Pink — revive
                    return DirectX::XMVectorSet(0.95f, 0.55f, 0.75f, currentAlpha);
                case ItemEffectKind::RestoreRage:
                    // Orange — rage / power
                    return DirectX::XMVectorSet(1.00f, 0.55f, 0.20f, currentAlpha);
                case ItemEffectKind::DealDamage:
                    // Red — offensive
                    return DirectX::XMVectorSet(0.95f, 0.30f, 0.30f, currentAlpha);
                case ItemEffectKind::StatBuff:
                    // Yellow — buff
                    return DirectX::XMVectorSet(0.95f, 0.90f, 0.35f, currentAlpha);
                case ItemEffectKind::Cleanse:
                    // Cyan — cleanse
                    return DirectX::XMVectorSet(0.45f, 0.90f, 0.95f, currentAlpha);
                }
                return DirectX::XMVectorSet(0.6f, 0.6f, 0.6f, currentAlpha);
            };

            // ------------------------------------------------------------
            // Item rows (icon + label).
            //
            // Layout per row:
            //   [icon] [text label                                       ]
            //
            // Icon: square inside the row, left-padded by iconPad.  Drawn
            // BEFORE the row background so the row's dialog box paints
            // OVER the icon's edge — wait, we want the icon ON TOP, so
            // draw the row background first, then the icon, then the text.
            // ------------------------------------------------------------
            const float iconPad  = baseDialogHeight * 0.18f;
            const float iconSize = baseDialogHeight - (iconPad * 2.0f);

            for (int i = first; i < last; ++i)
            {
                const ItemData* item = ItemRegistry::Get().Find(itemIds[i]);

                std::string label;
                if (item)
                {
                    label = item->name + " x" + std::to_string(Inventory::Get().GetCount(itemIds[i]));
                }
                else
                {
                    // Missing registry entry: show the raw id with a marker
                    // so a typo is visible instead of an invisible row.
                    label = itemIds[i] + " (?)";
                }

                bool isHovered = (i == hoveredIndex);
                float scaleMultiplier = isHovered ? hoverScale : 1.0f;

                float dialogWidth  = baseDialogWidth  * scaleMultiplier;
                float dialogHeight = baseDialogHeight * scaleMultiplier;

                float offsetX = (dialogWidth  - baseDialogWidth)  / 2.0f;
                float offsetY = (dialogHeight - baseDialogHeight) / 2.0f;

                const int rowInWindow = i - first;
                float dialogX = baseX - offsetX - slideOffset;
                float dialogY = baseY + (rowInWindow * rowStride) - offsetY;

                // 1. Row background.
                mDialogBox.Draw(
                    mD3D.GetContext(),
                    dialogX, dialogY,
                    dialogWidth, dialogHeight,
                    sliceScale * scaleMultiplier,
                    cameraMatrix,
                    dboxColor
                );

                // 2. Icon placeholder.  Square positioned at the left of
                //    the row.  Tinted color encodes the effect kind so the
                //    player can scan the menu by color even before real art.
                float iconScaledSize = iconSize * scaleMultiplier;
                float iconScaledPad  = iconPad  * scaleMultiplier;
                float iconX = dialogX + iconScaledPad;
                float iconY = dialogY + iconScaledPad;
                mDialogBox.Draw(
                    mD3D.GetContext(),
                    iconX, iconY,
                    iconScaledSize, iconScaledSize,
                    sliceScale * scaleMultiplier * 0.6f,   // tighter slice for the small quad
                    cameraMatrix,
                    IconTint(item)
                );

                // 3. Label text — shifted right to clear the icon column.
                float textX = dialogX + mMenuLayout.skill.textOffsetX * scaleMultiplier
                            + iconScaledSize + iconScaledPad;
                float textY = dialogY + mMenuLayout.skill.textOffsetY * scaleMultiplier;

                DirectX::XMVECTOR textColor = isHovered ? DirectX::Colors::Yellow : DirectX::Colors::White;
                textColor = DirectX::XMVectorSetW(textColor, currentAlpha);

                mTextRenderer.DrawString(
                    mD3D.GetContext(),
                    label.c_str(),
                    textX, textY,
                    textColor,
                    cameraMatrix
                );
            }

            // ------------------------------------------------------------
            // Up / down chevron sprites — drawn above and below the menu
            // when there are items off-screen in that direction.
            //
            // Both chevrons share one PNG (the existing pointer asset is
            // a placeholder; see idea/asset-todo.md for the planned
            // dedicated chevron art).  ScrollArrowRenderer::Draw rotates
            // the sprite 180 degrees AND inverts the bob direction when
            // flipVertical=true so the up arrow leans up and the down
            // arrow leans down — the loop animation feels purposeful
            // rather than random oscillation.
            //
            // Sprite scale is computed from the row height so the
            // chevron stays proportional to the dialog rows regardless
            // of menu layout tweaks.
            // ------------------------------------------------------------
            const bool hasMoreAbove = (first > 0);
            const bool hasMoreBelow = (last < itemCount);

            // ------------------------------------------------------------
            // Chevron sizing.
            //
            // The TARGET on-screen size is computed from the menu row
            // height, NOT from the source texture.  This means the
            // chevron always looks proportional to the menu — and an
            // artist can drop in a 32x32, 64x64, or 256x256 PNG without
            // touching code.  The renderer reports its actual texture
            // width via GetWidth(); we divide to derive the per-draw
            // scale.
            //
            // Why query GetWidth() instead of hardcoding 64:
            //   ScrollArrowRenderer auto-detects texture dimensions in
            //   Initialize().  Hardcoding a constant here would re-bind
            //   the call site to a specific PNG size and reintroduce
            //   the cropping bug we just fixed in the renderer.
            // ------------------------------------------------------------
            const float chevronSize  = baseDialogHeight * 0.55f;
            const int   chevronSrcW  = mChevronDown.GetWidth();
            const float chevronScale = (chevronSrcW > 0)
                ? chevronSize / static_cast<float>(chevronSrcW)
                : 1.0f;

            // Pivot is texture-center, so chevronX/chevronY name the
            // CENTER of the rendered sprite (not its top-left corner).
            const float chevronCenterX = baseX - slideOffset + (baseDialogWidth * 0.5f);

            DirectX::XMVECTOR chevronColor = DirectX::XMVectorSet(0.95f, 0.95f, 0.95f, currentAlpha);

            if (hasMoreAbove)
            {
                // Sit just above the menu's first row.  Half a row height
                // of clearance keeps the bob from overlapping the row.
                const float chevronCenterY = baseY - (baseDialogHeight * 0.45f);
                mChevronUp.Draw(
                    mD3D.GetContext(),
                    chevronCenterX, chevronCenterY,
                    true,                  // flipVertical: rotate 180 + invert bob
                    chevronScale,
                    cameraMatrix,
                    chevronColor
                );
            }

            if (hasMoreBelow)
            {
                // Sit just below the menu's last row.  totalHeight already
                // includes the trailing spacing; subtract a fraction of it
                // back so the chevron hugs the row instead of floating.
                const float chevronCenterY = baseY + totalHeight - (itemSpacing * 0.5f) + (baseDialogHeight * 0.10f);
                mChevronDown.Draw(
                    mD3D.GetContext(),
                    chevronCenterX, chevronCenterY,
                    false,                 // no flip: bob downward
                    chevronScale,
                    cameraMatrix,
                    chevronColor
                );
            }

            // ------------------------------------------------------------
            // Scrollbar — track + thumb to the RIGHT of the menu.
            //
            // Track: full-height thin rectangle representing the entire
            //        inventory list.
            // Thumb: shorter rectangle whose vertical position and height
            //        encode (first / itemCount) and (visibleCount / itemCount)
            //        respectively, just like a browser scrollbar.
            //
            // Drawn only when there are more items than visible rows —
            // a 1-or-2 item bag has nothing to scroll, so the bar would
            // just be visual noise.
            // ------------------------------------------------------------
            if (itemCount > visibleCount)
            {
                const float trackWidth   = baseDialogWidth * 0.06f;
                const float trackPadding = baseDialogWidth * 0.04f;
                const float trackX = baseX - slideOffset + baseDialogWidth + trackPadding;
                const float trackY = baseY;
                const float trackHeight = totalHeight - itemSpacing;   // align to last row's bottom

                // Track — dim grey.  The track lets the player see the
                // FULL extent of their inventory at a glance.
                DirectX::XMVECTOR trackColor =
                    DirectX::XMVectorSet(0.30f, 0.30f, 0.30f, currentAlpha * 0.85f);
                mDialogBox.Draw(
                    mD3D.GetContext(),
                    trackX, trackY,
                    trackWidth, trackHeight,
                    sliceScale * 0.5f,
                    cameraMatrix,
                    trackColor
                );

                // Thumb — bright accent.  Height proportional to the
                // visible window's share of the full list; vertical
                // position proportional to scroll progress.
                const float ratio = static_cast<float>(visibleCount) / static_cast<float>(itemCount);
                const float thumbHeight = (std::max)(trackHeight * ratio, baseDialogHeight * 0.20f);
                // first/(itemCount - visibleCount) clamps progress into [0,1]
                // even when the window is full of trailing items.
                const int   denom = (std::max)(1, itemCount - visibleCount);
                const float progress = static_cast<float>(first) / static_cast<float>(denom);
                const float thumbY = trackY + (trackHeight - thumbHeight) * progress;

                DirectX::XMVECTOR thumbColor =
                    DirectX::XMVectorSet(0.95f, 0.85f, 0.30f, currentAlpha);
                mDialogBox.Draw(
                    mD3D.GetContext(),
                    trackX, thumbY,
                    trackWidth, thumbHeight,
                    sliceScale * 0.5f,
                    cameraMatrix,
                    thumbColor
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

    // ---- Item target pointer (ITEM_TARGET_SELECT) ----
    // Items can target allies or enemies depending on their targeting
    // rule.  We re-resolve the candidate list once per frame using the
    // same lookup the input controller uses, then point at whichever
    // slot the cursor is on.
    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN &&
        mInputController.GetInputPhase() == PlayerInputPhase::ITEM_TARGET_SELECT)
    {
        const auto& itemIds = mInputController.GetItemIds();
        const int itemIdx   = mInputController.GetItemIndex();
        if (itemIdx >= 0 && itemIdx < static_cast<int>(itemIds.size()))
        {
            const ItemData* item = ItemRegistry::Get().Find(itemIds[itemIdx]);
            if (item)
            {
                std::vector<IBattler*> candidates;
                bool aimAtAllies = false;
                if (item->targeting == ItemTargeting::SingleAlly)
                {
                    candidates  = mBattle.GetAlivePlayers();
                    aimAtAllies = true;
                }
                else if (item->targeting == ItemTargeting::SingleAllyAny)
                {
                    candidates  = mBattle.GetAllPlayers();
                    aimAtAllies = true;
                }
                else if (item->targeting == ItemTargeting::SingleEnemy)
                {
                    candidates = mBattle.GetAliveEnemies();
                }

                int targetIdx = mInputController.GetTargetIndex();
                if (targetIdx < 0 || targetIdx >= static_cast<int>(candidates.size()))
                    targetIdx = 0;

                if (!candidates.empty())
                {
                    IBattler* picked = candidates[targetIdx];

                    // Locate the picked battler's slot inside its team list
                    // (slot positions live in BattleRenderer keyed by team).
                    int slotIndex = 0;
                    if (aimAtAllies)
                    {
                        const auto& players = mBattle.GetAllPlayers();
                        for (int i = 0; i < static_cast<int>(players.size()); ++i)
                            if (players[i] == picked) { slotIndex = i; break; }
                    }
                    else
                    {
                        const auto& enemies = mBattle.GetAllEnemies();
                        for (int i = 0; i < static_cast<int>(enemies.size()); ++i)
                            if (enemies[i] == picked) { slotIndex = i; break; }
                    }

                    float worldX, worldY;
                    if (aimAtAllies)
                        mBattleRenderer.GetPlayerSlotPos(slotIndex, worldX, worldY);
                    else
                        mBattleRenderer.GetEnemySlotPos(slotIndex, worldX, worldY);

                    auto cameraMatrix = mBattleRenderer.GetCamera().GetViewMatrix();
                    mTargetPointer.Draw(mD3D.GetContext(), worldX, worldY, cameraMatrix);
                }
            }
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
        case PlayerInputPhase::COMMAND_SELECT:     snap.inputPhase = "COMMAND_SELECT";     break;
        case PlayerInputPhase::SKILL_SELECT:       snap.inputPhase = "SKILL_SELECT";       break;
        case PlayerInputPhase::TARGET_SELECT:      snap.inputPhase = "TARGET_SELECT";      break;
        case PlayerInputPhase::ITEM_SELECT:        snap.inputPhase = "ITEM_SELECT";        break;
        case PlayerInputPhase::ITEM_TARGET_SELECT: snap.inputPhase = "ITEM_TARGET_SELECT"; break;
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
                row.available   = skill->CanUse(*static_cast<const IBattler*>(player), mBattle.GetContext());
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

    // ---- Inventory rows: shown during ITEM_SELECT or ITEM_TARGET_SELECT.
    // Driven from BattleInputController::GetItemIds(), which is the same
    // snapshot the controller uses to drive cursor movement — no risk of
    // index drift between the menu rendering and the input handler.
    //
    // Window matches the on-screen menu (kVisibleItems = 3) so the debug
    // HUD shows the same scroll position the player sees.
    if ((inputPhase == PlayerInputPhase::ITEM_SELECT ||
         inputPhase == PlayerInputPhase::ITEM_TARGET_SELECT) &&
        mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        const auto& ids = mInputController.GetItemIds();
        const int n = static_cast<int>(ids.size());
        constexpr int kVisibleItems = 3;

        const int hovered = mInputController.GetItemIndex();
        int first = hovered - (kVisibleItems / 2);
        const int maxFirst = (n > kVisibleItems) ? (n - kVisibleItems) : 0;
        if (first < 0) first = 0;
        if (first > maxFirst) first = maxFirst;
        const int last = (n < first + kVisibleItems) ? n : first + kVisibleItems;

        for (int i = first; i < last; ++i)
        {
            const ItemData* item = ItemRegistry::Get().Find(ids[i]);
            BattleHUDSnapshot::ItemRow row;
            row.slot        = i + 1;
            row.name        = item ? item->name        : ids[i];
            row.description = item ? item->description : "(missing registry entry)";
            row.count       = Inventory::Get().GetCount(ids[i]);
            row.selected    = (i == hovered);
            snap.itemRows.push_back(row);
        }

        if (inputPhase == PlayerInputPhase::ITEM_TARGET_SELECT)
        {
            snap.infoLines.push_back({ "Hint",
                "Up/Down = pick target   Enter = confirm   Esc = back" });
        }
        else
        {
            snap.infoLines.push_back({ "Hint",
                "Up/Down = browse items  Enter = use   Esc = back to commands" });
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

    int queueSize = 6; // Queue size can be configured
    const auto& predictedQueue = mBattle.GetFutureTurnQueue(queueSize);
    for (IBattler* b : predictedQueue)
    {
        BattleHUDSnapshot::TimelineRow row;
        row.name = b->GetName();
        row.currentAV = 0.0f; // No longer needed for visual queue
        snap.timeline.push_back(row);
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

void BattleState::OnDamageTaken(const EventData& e)
{
    auto* payload = static_cast<DamageTakenPayload*>(e.payload);
    if (!payload || !payload->target) return;

    int slot;
    bool isPlayer;
    if (GetBattlerSlot(payload->target, slot, isPlayer))
    {
        float worldX, worldY;
        if (isPlayer) {
            mBattleRenderer.GetPlayerSlotPos(slot, worldX, worldY);
        } else {
            mBattleRenderer.GetEnemySlotPos(slot, worldX, worldY);
        }
        // Initial spawn positions
        worldX += ((rand() % 20) - 10.0f); 
        worldY -= 40.0f;

        FloatingDamageText ft;
        ft.text = std::to_string(payload->damage);
        ft.worldX = worldX;
        ft.worldY = worldY;
        
        // Toss numbers out and up
        ft.vx = ((rand() % 160) - 80.0f); 
        ft.vy = -((rand() % 200) + 300.0f); 
        
        ft.scale = 1.3f;
        ft.lifeTimer = 1.0f;
        ft.maxLife = 1.0f;
        if (payload->isCrit) {
            ft.color = DirectX::Colors::Orange;
            ft.text += "!";
            ft.scale = 1.7f;
            // Minor bump for critical damage
            mBattleRenderer.TriggerCameraShake(20.0f, 0.2f);
        } else {
            ft.color = DirectX::Colors::White;
        }

        mFloatingTexts.push_back(ft);
    }
}

void BattleState::OnQteFeedback(const EventData& e)
{
    auto* payload = static_cast<QTEStatePayload*>(e.payload);
    if (!payload) return;
    
    if (payload->result == QTEResult::Perfect) {
        // High intensity shake for perfect 
        mBattleRenderer.TriggerCameraShake(40.0f, 0.25f);
    }
    else if (payload->result == QTEResult::Good) {
        mBattleRenderer.TriggerCameraShake(15.0f, 0.15f);
    }
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

void BattleState::OnBulletHellState(const EventData& e)
{
    if (e.payload && mBulletHellRenderer) {
        const BulletHellPayload* payload = static_cast<const BulletHellPayload*>(e.payload);
        mBulletHellRenderer->UpdateState(*payload);
    }
}
