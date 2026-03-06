// ============================================================
// File: BattleState.cpp
// Responsibility: Turn-based battle IGameState — drives BattleManager,
//                handles input via a three-phase FSM, renders combatant
//                sprites and the HP bar UI.
//
// Player Input FSM (PlayerInputPhase):
//   COMMAND_SELECT  Up/Down = move cursor, Enter = confirm IBattleCommand
//   SKILL_SELECT    1/2/3   = pick skill,  Esc   = back to COMMAND_SELECT
//   TARGET_SELECT   Tab     = cycle enemy, Enter = confirm, Esc = back
//
// Adding a new top-level menu option:
//   1. Create a class implementing IBattleCommand.
//   2. Append it in BuildCommandList() — no other code changes needed.
// ============================================================
#include "BattleState.h"
#include "../States/StateManager.h"
#include "../Events/EventManager.h"
#include "../Systems/PartyManager.h"
#include "../Battle/FightCommand.h"
#include "../Battle/FleeCommand.h"
#include "../UI/BattleDebugHUD.h"
#include "../Utils/Log.h"
#define NOMINMAX
#include <Windows.h>      // GetAsyncKeyState
#include <string>
#include <algorithm>
#include <array>

// Battle background — dark navy blue, distinct from the play-world color.
static constexpr float kBgR = 0.05f;
static constexpr float kBgG = 0.05f;
static constexpr float kBgB = 0.20f;

BattleState::BattleState(D3DContext& d3d)
    : mD3D(d3d)
{}

// ------------------------------------------------------------
// BuildCommandList: populate the top-level command menu.
//   To add a new option, append one line here and create the matching class.
//   Order of push_back == display order in the debug console.
// ------------------------------------------------------------
void BattleState::BuildCommandList()
{
    mCommands.clear();
    mCommands.push_back(std::make_unique<FightCommand>());
    mCommands.push_back(std::make_unique<FleeCommand>());
    // Future: mCommands.push_back(std::make_unique<ItemCommand>());
    // Future: mCommands.push_back(std::make_unique<DefendCommand>());
}

void BattleState::OnEnter()
{
    LOG("%s", "[BattleState] OnEnter");
    mBattle.Initialize();

    // Reset input FSM to the top-level command menu.
    mInputPhase   = PlayerInputPhase::COMMAND_SELECT;
    mCommandIndex = 0;
    mSkillIndex   = 0;
    mTargetIndex  = 0;

    // Build the data-driven command list (Fight, Flee, …).
    BuildCommandList();

    // ----------------------------------------------------------------
    // Build BattleRenderer slot descriptors.
    // Player slot 0 = Verso (always present).
    // Slots 1 and 2 = additional party members (empty for MVP).
    // Enemy slots 0 and 1 = Skeleton A/B; slot 2 empty.
    // ----------------------------------------------------------------
    std::array<BattleRenderer::SlotInfo, BattleRenderer::kMaxSlots> playerSlots{};
    playerSlots[0].occupied    = true;
    playerSlots[0].texturePath = L"assets/animations/verso.png";
    playerSlots[0].jsonPath    = "assets/animations/verso.json";
    playerSlots[0].startClip   = "idle";
    // Verso sprite has a bottom-center pivot (feet at worldY).
    // Shift up by half the rendered height (128px frame * scale 2.0 / 2 = 128)
    // so the visual center of the character aligns with the slot anchor.
    playerSlots[0].drawOffsetY = 128.0f/2;
    // slots 1 and 2 remain occupied=false (empty party slots)

    std::array<BattleRenderer::SlotInfo, BattleRenderer::kMaxSlots> enemySlots{};
    enemySlots[0].occupied    = true;
    enemySlots[0].texturePath = L"assets/animations/skeleton.png";
    enemySlots[0].jsonPath    = "assets/animations/skeleton.json";
    enemySlots[0].startClip   = "idle";
    enemySlots[0].drawOffsetY = 128.0f/2;  // same correction as player side
    enemySlots[1].occupied    = true;
    enemySlots[1].texturePath = L"assets/animations/skeleton.png";
    enemySlots[1].jsonPath    = "assets/animations/skeleton.json";
    enemySlots[1].startClip   = "idle";
    enemySlots[1].drawOffsetY = 128.0f/2;
    // slot 2 remains occupied=false

    mBattleRenderer.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        playerSlots,
        enemySlots,
        mD3D.GetWidth(),
        mD3D.GetHeight()
    );

    // Initialize the 3-layer HP bar renderer.
    // Initialize() loads both PNGs, creates SpriteBatch, and subscribes
    // to "verso_hp_changed" via EventManager — no polling needed.
    mHealthBar.Initialize(
        mD3D.GetDevice(),
        mD3D.GetContext(),
        L"assets/UI/UI_hp_background.png",
        L"assets/UI/UI_verso_hp.png",
        "assets/UI/HP_description.json",
        mD3D.GetWidth(),
        mD3D.GetHeight()
    );

    // Seed the bar with the live combatant's current HP so the bar is
    // correct before the first "verso_hp_changed" event fires.
    const auto& players = mBattle.GetAllPlayers();
    if (!players.empty())
    {
        mHealthBar.SetMaxHP(static_cast<float>(players[0]->GetStats().maxHp));
        mHealthBar.SetHP   (static_cast<float>(players[0]->GetStats().hp));
    }

    DumpStateToDebugOutput();
}

void BattleState::OnExit()
{
    LOG("%s", "[BattleState] OnExit");

    // Release combatant sprite GPU resources (textures, SpriteBatch, D3D states).
    mBattleRenderer.Shutdown();

    // Release HP bar GPU resources and unsubscribe "verso_hp_changed" listener
    // before the state is destroyed.  Must happen before D3DContext teardown.
    mHealthBar.Shutdown();
}

// ------------------------------------------------------------
// Update: process player input (only during PLAYER_TURN), then
//         tick the battle simulation and the HP bar lerp.
//         Pops the state when the battle concludes and broadcasts outcome.
// ------------------------------------------------------------
void BattleState::Update(float dt)
{
    const BattlePhase phaseBefore = mBattle.GetPhase();

    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        HandleInput();
    }

    mBattle.Update(dt);

    const BattlePhase phaseAfter = mBattle.GetPhase();

    // When a new PLAYER_TURN begins, reset the input FSM to COMMAND_SELECT.
    // SetInputPhase() calls DumpStateToDebugOutput() automatically.
    if (phaseBefore != BattlePhase::PLAYER_TURN &&
        phaseAfter  == BattlePhase::PLAYER_TURN)
    {
        SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
    }
    // For every OTHER phase transition (RESOLVING, ENEMY_TURN, WIN, LOSE):
    //   - Reset the camera to OVERVIEW — the action queue plays out in a wide shot.
    //   - Dump the HUD so the player sees what is happening.
    else if (phaseBefore != phaseAfter)
    {
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::OVERVIEW, -1, -1);
        DumpStateToDebugOutput();
    }

    // Advance HP bar lerp toward the target value set by the last event.
    mHealthBar.Update(dt);

    // Advance all combatant sprite animations.
    mBattleRenderer.Update(dt);

    // Check for battle conclusion and exit the state.
    const BattleOutcome outcome = mBattle.GetOutcome();
    if (outcome != BattleOutcome::NONE)
    {
        // Persist Verso's current HP (and MP) back to PartyManager so the
        // next battle starts with whatever HP remained at the end of this one.
        // Rage is intentionally reset inside PartyManager::SetVersoStats().
        const auto& players = mBattle.GetAllPlayers();
        if (!players.empty())
        {
            PartyManager::Get().SetVersoStats(players[0]->GetStats());
            LOG("[BattleState] Saved Verso HP: %d/%d",
                players[0]->GetStats().hp, players[0]->GetStats().maxHp);
        }

        const char* eventName = (outcome == BattleOutcome::VICTORY)
            ? "battle_end_victory" : "battle_end_defeat";

        EventManager::Get().Broadcast(eventName, {});
        StateManager::Get().PopState();
        return;  // BattleState is now destroyed — do NOT touch any member below.
    }

    // ------------------------------------------------------------
    // Deferred flee: FleeCommand called RequestFlee() during HandleInput().
    // We cannot PopState() there because Execute() runs inside our own
    // call stack — destroying BattleState mid-frame is a use-after-free.
    // Checking here, after ALL handlers have returned, is safe.
    // ------------------------------------------------------------
    if (mPendingFlee)
    {
        mPendingFlee = false;   // clear before pop so OnExit sees clean state
        LOG("%s", "[BattleState] Processing deferred flee — popping state.");
        EventData data;
        data.name = "battle_flee";
        EventManager::Get().Broadcast("battle_flee", data);
        StateManager::Get().PopState();
        return;  // BattleState is now destroyed — do NOT touch any member below.
    }
}

// ------------------------------------------------------------
// Render: re-clear the back buffer to the battle navy color, then
//         draw the HP bar UI.
// GameApp::Render() owns BeginFrame (clear) and EndFrame (Present).
// BattleState calls BeginFrame again to overwrite GameApp's default gray
// with the battle-specific color — safe because BeginFrame only clears
// the RTV; it does NOT call Present.
// EndFrame (Present) is called exactly once per frame by GameApp — never here.
// ------------------------------------------------------------
void BattleState::Render()
{
    // Re-clear with battle color. No EndFrame — GameApp handles Present.
    mD3D.BeginFrame(kBgR, kBgG, kBgB);

    // Draw all combatant sprites (player side + enemy side).
    // BattleRenderer uses its own flat Camera2D, so world coords = screen pixels.
    mBattleRenderer.Render(mD3D.GetContext());

    // Draw the 3-layer HP bar: background → fill (clipped) → frame+portrait.
    mHealthBar.Render(mD3D.GetContext());
}

// ------------------------------------------------------------
// HandleInput: route to the correct sub-handler based on the current
//   input phase.  Called only when BattleManager is in PLAYER_TURN.
// ------------------------------------------------------------
void BattleState::HandleInput()
{
    switch (mInputPhase)
    {
    case PlayerInputPhase::COMMAND_SELECT: HandleCommandSelect(); break;
    case PlayerInputPhase::SKILL_SELECT:   HandleSkillSelect();   break;
    case PlayerInputPhase::TARGET_SELECT:  HandleTargetSelect();  break;
    }
}

// ------------------------------------------------------------
// HandleCommandSelect: Up/Down moves the cursor, Enter confirms.
//   The confirmed IBattleCommand drives the phase transition — BattleState
//   never switches on the command type.
// ------------------------------------------------------------
void BattleState::HandleCommandSelect()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    const int cmdCount = static_cast<int>(mCommands.size());

    if (pressed(VK_UP, mKeyUpWasDown))
    {
        mCommandIndex = (mCommandIndex - 1 + cmdCount) % cmdCount;
        LOG("[BattleState] Command cursor -> %s", mCommands[mCommandIndex]->GetLabel());
        DumpStateToDebugOutput();
    }
    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mCommandIndex = (mCommandIndex + 1) % cmdCount;
        LOG("[BattleState] Command cursor -> %s", mCommands[mCommandIndex]->GetLabel());
        DumpStateToDebugOutput();
    }
    if (pressed(VK_RETURN, mEnterWasDown))
    {
        LOG("[BattleState] Command confirmed: %s", mCommands[mCommandIndex]->GetLabel());
        // Execute drives the phase change — open/closed, no switch needed here.
        mCommands[mCommandIndex]->Execute(*this);
        DumpStateToDebugOutput();
    }
}

// ------------------------------------------------------------
// HandleSkillSelect: Up/Down moves the skill cursor, Enter confirms.
//   Cursor wraps around the available skill count.
//   Enter advances to TARGET_SELECT only if the highlighted skill is usable.
//   Backspace cancels back to COMMAND_SELECT.
// ------------------------------------------------------------
void BattleState::HandleSkillSelect()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    auto* player = mBattle.GetActivePlayer();
    if (!player) return;

    // Count how many skills the active player actually has.
    // GetSkill() returns nullptr for out-of-range indices — walk until null.
    int skillCount = 0;
    while (player->GetSkill(skillCount) != nullptr) ++skillCount;
    if (skillCount == 0) return;

    // Up/Down move the cursor, wrapping around the skill list.
    if (pressed(VK_UP, mKeyUpWasDown))
    {
        mSkillIndex = (mSkillIndex - 1 + skillCount) % skillCount;
        LOG("[BattleState] Skill cursor -> slot %d (%s)",
            mSkillIndex, player->GetSkill(mSkillIndex)->GetName());
        DumpStateToDebugOutput();
    }
    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mSkillIndex = (mSkillIndex + 1) % skillCount;
        LOG("[BattleState] Skill cursor -> slot %d (%s)",
            mSkillIndex, player->GetSkill(mSkillIndex)->GetName());
        DumpStateToDebugOutput();
    }

    // Enter confirms the highlighted skill and advances to target selection.
    if (pressed(VK_RETURN, mEnterWasDown))
    {
        ISkill* skill = player->GetSkill(mSkillIndex);
        if (!skill || !skill->CanUse(*player))
        {
            LOG("[BattleState] Skill %d unavailable.", mSkillIndex + 1);
            return;
        }
        LOG("[BattleState] Skill confirmed: %s — now pick a target", skill->GetName());
        SetInputPhase(PlayerInputPhase::TARGET_SELECT);
    }

    // Backspace goes back to the top-level command menu.
    if (pressed(VK_BACK, mBackWasDown))
    {
        LOG("%s", "[BattleState] Cancelled skill select — back to command menu.");
        SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
    }
}

// ------------------------------------------------------------
// HandleTargetSelect: Up/Down cycles the target cursor, Enter confirms.
//   Down advances to the next alive enemy (same as the old Tab behaviour).
//   Up retreats to the previous alive enemy.
//   Backspace cancels back to SKILL_SELECT.
// ------------------------------------------------------------
void BattleState::HandleTargetSelect()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    const auto enemies = mBattle.GetAliveEnemies();
    if (enemies.empty()) return;
    const int enemyCount = static_cast<int>(enemies.size());

    // Down advances to the next target (wraps around).
    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mTargetIndex = (mTargetIndex + 1) % enemyCount;
        LOG("[BattleState] Target -> %s", enemies[mTargetIndex]->GetName().c_str());

        constexpr int kActivePlayerSlot = 0;
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::TARGET_FOCUS,
                                        kActivePlayerSlot, mTargetIndex);
        DumpStateToDebugOutput();
    }

    // Up retreats to the previous target (wraps around).
    if (pressed(VK_UP, mKeyUpWasDown))
    {
        mTargetIndex = (mTargetIndex - 1 + enemyCount) % enemyCount;
        LOG("[BattleState] Target -> %s", enemies[mTargetIndex]->GetName().c_str());

        constexpr int kActivePlayerSlot = 0;
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::TARGET_FOCUS,
                                        kActivePlayerSlot, mTargetIndex);
        DumpStateToDebugOutput();
    }

    if (pressed(VK_RETURN, mEnterWasDown))  ConfirmSkillAndTarget();

    // Backspace cancels target selection and returns to the skill menu.
    if (pressed(VK_BACK, mBackWasDown))
    {
        LOG("%s", "[BattleState] Cancelled target select — back to skill menu.");
        SetInputPhase(PlayerInputPhase::SKILL_SELECT);
    }
}

// ------------------------------------------------------------
// CycleTarget: advance the target cursor among alive enemies.
// ------------------------------------------------------------
void BattleState::CycleTarget()
{
    const auto enemies = mBattle.GetAliveEnemies();
    if (enemies.empty()) return;

    mTargetIndex = (mTargetIndex + 1) % static_cast<int>(enemies.size());
    LOG("[BattleState] Target -> %s", enemies[mTargetIndex]->GetName().c_str());

    // Update the camera target position to track the newly selected enemy.
    // The actor slot stays 0; only the target slot changes.
    constexpr int kActivePlayerSlot = 0;
    mBattleRenderer.SetCameraPhase(BattleCameraPhase::TARGET_FOCUS,
                                    kActivePlayerSlot, mTargetIndex);

    DumpStateToDebugOutput();
}

// ------------------------------------------------------------
// ConfirmSkillAndTarget: validate and submit the chosen action to
//   BattleManager.  Resets the input FSM to COMMAND_SELECT so the
//   next player turn starts fresh at the top menu.
// ------------------------------------------------------------
void BattleState::ConfirmSkillAndTarget()
{
    auto* player = mBattle.GetActivePlayer();
    if (!player) return;

    ISkill* skill = player->GetSkill(mSkillIndex);
    if (!skill || !skill->CanUse(*player))
    {
        LOG("%s", "[BattleState] Skill unavailable — action cancelled.");
        SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
        return;
    }

    auto enemies = mBattle.GetAliveEnemies();
    if (enemies.empty()) return;

    // Clamp in case an enemy died since the last Tab press.
    if (mTargetIndex >= static_cast<int>(enemies.size()))
        mTargetIndex = 0;

    IBattler* target = enemies[mTargetIndex];
    mBattle.SetPlayerAction(mSkillIndex, target);

    LOG("[BattleState] Action confirmed: %s -> %s",
        skill->GetName(), target->GetName().c_str());

    // Reset to top-level menu for the next player turn.
    // Phase will effectively be re-entered when PLAYER_TURN becomes active
    // again — the reset here just ensures a clean state.
    SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
}

// ------------------------------------------------------------
// SetInputPhase: public entry point for IBattleCommand implementations.
//   Resets relevant cursors on transition so stale positions don't leak
//   between phases.
//   Also drives the BattleRenderer camera into the appropriate cinematic
//   phase so the player gets a visual cue about what they are selecting.
// ------------------------------------------------------------
void BattleState::SetInputPhase(PlayerInputPhase phase)
{
    mInputPhase = phase;

    // Reset the cursor for the phase being entered.
    if (phase == PlayerInputPhase::COMMAND_SELECT) mCommandIndex = 0;
    if (phase == PlayerInputPhase::SKILL_SELECT)   mSkillIndex   = 0;
    if (phase == PlayerInputPhase::TARGET_SELECT)  mTargetIndex  = 0;

    // ---- Drive the battle camera to match the new input phase ----
    // Actor slot: the active player is always slot 0 (single-player party for now).
    // When multi-character turns are added, replace 0 with the active slot index.
    constexpr int kActivePlayerSlot = 0;

    switch (phase)
    {
    case PlayerInputPhase::SKILL_SELECT:
        // Player is choosing a skill — zoom in on the acting character so the
        // player sees clearly who is about to attack.
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::ACTOR_FOCUS,
                                        kActivePlayerSlot, -1);
        break;

    case PlayerInputPhase::TARGET_SELECT:
        // Player is choosing a target — pan to 80% target / 20% actor blend
        // so the target is clearly highlighted while the attacker stays in frame.
        // mTargetIndex was just reset to 0 above, so the camera starts on the
        // first alive enemy (slot 0).
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::TARGET_FOCUS,
                                        kActivePlayerSlot, mTargetIndex);
        break;

    case PlayerInputPhase::COMMAND_SELECT:
    default:
        // Back to top-level menu — wide shot, full battlefield visible.
        mBattleRenderer.SetCameraPhase(BattleCameraPhase::OVERVIEW, -1, -1);
        break;
    }

    DumpStateToDebugOutput();
}

// ------------------------------------------------------------
// DumpStateToDebugOutput: build a BattleHUDSnapshot from live state and
//   delegate all formatting to BattleDebugHUD::Render().
//   BattleState assembles the data; BattleDebugHUD owns all layout logic.
//
//   Called:
//     - Every SetInputPhase() transition (player input sub-menu change)
//     - Every simulation phase change (RESOLVING, ENEMY_TURN, WIN, LOSE)
//   This ensures the player always has current context in the LOG console.
// ------------------------------------------------------------
void BattleState::DumpStateToDebugOutput() const
{
    BattleHUDSnapshot snap;
    snap.title = "BATTLE STATE";

    // ---- Simulation phase (BattleManager FSM) ----
    // This tells the player what the engine is currently doing.
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

    // ---- Player input phase (only meaningful during PLAYER_TURN) ----
    if (mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        switch (mInputPhase)
        {
        case PlayerInputPhase::COMMAND_SELECT: snap.inputPhase = "COMMAND_SELECT"; break;
        case PlayerInputPhase::SKILL_SELECT:   snap.inputPhase = "SKILL_SELECT";   break;
        case PlayerInputPhase::TARGET_SELECT:  snap.inputPhase = "TARGET_SELECT";  break;
        }
    }

    // ---- Command menu (Fight / Flee / …) — visible in COMMAND_SELECT ----
    if (mInputPhase == PlayerInputPhase::COMMAND_SELECT &&
        mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        for (int i = 0; i < static_cast<int>(mCommands.size()); ++i)
        {
            BattleHUDSnapshot::MenuItem item;
            item.label    = mCommands[i]->GetLabel();
            item.selected = (i == mCommandIndex);
            snap.menuItems.push_back(item);
        }
    }

    // ---- Skill list — visible in SKILL_SELECT and TARGET_SELECT ----
    // Show all skills so the player knows their options before confirming.
    if ((mInputPhase == PlayerInputPhase::SKILL_SELECT ||
         mInputPhase == PlayerInputPhase::TARGET_SELECT) &&
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
                row.slot        = i + 1;          // 1-based key shown to player
                row.name        = skill->GetName();
                row.description = skill->GetDescription();
                // CanUse takes a const IBattler& — cast is safe, no mutation.
                row.available   = skill->CanUse(*static_cast<const IBattler*>(player));
                row.selected    = (i == mSkillIndex);
                snap.skillRows.push_back(row);
            }
        }
    }

    // ---- Info lines — target cursor hint in TARGET_SELECT ----
    if (mInputPhase == PlayerInputPhase::TARGET_SELECT &&
        mBattle.GetPhase() == BattlePhase::PLAYER_TURN)
    {
        const auto enemies = mBattle.GetAliveEnemies();
        if (mTargetIndex < static_cast<int>(enemies.size()))
        {
            snap.infoLines.push_back({ "Target",
                enemies[mTargetIndex]->GetName() });
            snap.infoLines.push_back({ "Hint",
                "Tab = next target  Enter = confirm  Esc = back" });
        }

        // Also show which skill is about to fire.
        const PlayerCombatant* player = mBattle.GetActivePlayer();
        if (player)
        {
            const ISkill* skill = player->GetSkill(mSkillIndex);
            snap.infoLines.push_back({ "Skill",
                skill ? skill->GetName() : "(none)" });
        }
    }

    // ---- Combatant table ----
    // Determine which combatant is currently acting so we can mark them.
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
        // maxRage left 0 — PushCombatants() omits the Rage bar for enemies.
        snap.combatants.push_back(row);
    }

    // ---- Battle log tail — last N lines ----
    // Copy the full log; BattleDebugHUD::Render slices to kLogLines itself.
    snap.logLines = mBattle.GetBattleLog();

    BattleDebugHUD::Render(snap);
}

