// ============================================================
// File: BattleInputController.cpp
// Responsibility: Implements the battle input FSM.
// ============================================================
#include "BattleInputController.h"
#include "../States/BattleState.h"
#include "BattleManager.h"
#include "BattleRenderer.h"
#include "IBattleCommand.h"
#include "FightCommand.h"
#include "FleeCommand.h"
#include "ItemCommand.h"
#include "ItemRegistry.h"
#include "ItemData.h"
#include "../Systems/Inventory.h"
#include "../Utils/Log.h"

#define NOMINMAX
#include <Windows.h>

BattleInputController::BattleInputController(BattleState& state, BattleManager& battle, BattleRenderer& renderer)
    : mState(state), mBattle(battle), mRenderer(renderer)
{
}

BattleInputController::~BattleInputController() = default;

void BattleInputController::Initialize()
{
    mInputPhase   = PlayerInputPhase::COMMAND_SELECT;
    mCommandIndex = 0;
    mSkillIndex   = 0;
    mTargetIndex  = 0;
    mItemIndex    = 0;

    // ItemRegistry is the read-only catalog of every item the game knows
    // about.  Loaded once on first access; safe to call here.  Inventory
    // is a separate concept (per-player counts) and seeds itself in its
    // own constructor.
    ItemRegistry::Get().EnsureLoaded();

    BuildCommandList();
}

void BattleInputController::BuildCommandList()
{
    mCommands.clear();
    mCommands.push_back(std::make_unique<FightCommand>());
    mCommands.push_back(std::make_unique<ItemCommand>());
    mCommands.push_back(std::make_unique<FleeCommand>());
    // Future: mCommands.push_back(std::make_unique<DefendCommand>());
}

void BattleInputController::HandleInput()
{
    switch (mInputPhase)
    {
    case PlayerInputPhase::COMMAND_SELECT:     HandleCommandSelect();    break;
    case PlayerInputPhase::SKILL_SELECT:       HandleSkillSelect();      break;
    case PlayerInputPhase::TARGET_SELECT:      HandleTargetSelect();     break;
    case PlayerInputPhase::ITEM_SELECT:        HandleItemSelect();       break;
    case PlayerInputPhase::ITEM_TARGET_SELECT: HandleItemTargetSelect(); break;
    }
}

// ------------------------------------------------------------
// RefreshItemList: cache the current OwnedIds() snapshot.
// Called whenever the player enters ITEM_SELECT so the menu reflects
// the latest stack counts (an item used last turn vanishes if the
// stack hit 0).
// ------------------------------------------------------------
void BattleInputController::RefreshItemList()
{
    mItemIds = Inventory::Get().OwnedIds();
    if (mItemIndex >= static_cast<int>(mItemIds.size()))
        mItemIndex = 0;
}

void BattleInputController::HandleCommandSelect()
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
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mCommandIndex = (mCommandIndex + 1) % cmdCount;
        LOG("[BattleState] Command cursor -> %s", mCommands[mCommandIndex]->GetLabel());
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_RETURN, mEnterWasDown))
    {
        LOG("[BattleState] Command confirmed: %s", mCommands[mCommandIndex]->GetLabel());
        mCommands[mCommandIndex]->Execute(mState);
        mState.DumpStateToDebugOutput();
    }
}

void BattleInputController::HandleSkillSelect()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    auto* player = mBattle.GetActivePlayer();
    if (!player) return;

    int skillCount = 0;
    while (player->GetSkill(skillCount) != nullptr) ++skillCount;
    if (skillCount == 0) return;

    if (pressed(VK_UP, mKeyUpWasDown))
    {
        mSkillIndex = (mSkillIndex - 1 + skillCount) % skillCount;
        LOG("[BattleState] Skill cursor -> slot %d (%s)",
            mSkillIndex, player->GetSkill(mSkillIndex)->GetName());
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mSkillIndex = (mSkillIndex + 1) % skillCount;
        LOG("[BattleState] Skill cursor -> slot %d (%s)",
            mSkillIndex, player->GetSkill(mSkillIndex)->GetName());
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_RETURN, mEnterWasDown))
    {
        ISkill* skill = player->GetSkill(mSkillIndex);
        // CanUse now takes the live BattleContext so predicate-driven
        // availability (e.g. "HP below 50%") evaluates under current state.
        if (!skill || !skill->CanUse(*player, mBattle.GetContext()))
        {
            LOG("[BattleState] Skill %d unavailable.", mSkillIndex + 1);
            return;
        }
        LOG("[BattleState] Skill confirmed: %s — now pick a target", skill->GetName());
        SetInputPhase(PlayerInputPhase::TARGET_SELECT);
    }
    if (pressed(VK_BACK, mBackWasDown))
    {
        LOG("%s", "[BattleState] Cancelled skill select — back to command menu.");
        SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
    }
}

void BattleInputController::HandleTargetSelect()
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

    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mTargetIndex = (mTargetIndex + 1) % enemyCount;
        LOG("[BattleState] Target -> %s", enemies[mTargetIndex]->GetName().c_str());

        constexpr int kActivePlayerSlot = 0;
        mRenderer.SetCameraPhase(BattleCameraPhase::TARGET_FOCUS,
                                 kActivePlayerSlot, mTargetIndex);
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_UP, mKeyUpWasDown))
    {
        mTargetIndex = (mTargetIndex - 1 + enemyCount) % enemyCount;
        LOG("[BattleState] Target -> %s", enemies[mTargetIndex]->GetName().c_str());

        constexpr int kActivePlayerSlot = 0;
        mRenderer.SetCameraPhase(BattleCameraPhase::TARGET_FOCUS,
                                 kActivePlayerSlot, mTargetIndex);
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_RETURN, mEnterWasDown))  ConfirmSkillAndTarget();

    if (pressed(VK_BACK, mBackWasDown))
    {
        LOG("%s", "[BattleState] Cancelled target select — back to skill menu.");
        SetInputPhase(PlayerInputPhase::SKILL_SELECT);
    }
}

void BattleInputController::ConfirmSkillAndTarget()
{
    auto* player = mBattle.GetActivePlayer();
    if (!player) return;

    ISkill* skill = player->GetSkill(mSkillIndex);
    if (!skill || !skill->CanUse(*player, mBattle.GetContext()))
    {
        LOG("%s", "[BattleState] Skill unavailable — action cancelled.");
        SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
        return;
    }

    auto enemies = mBattle.GetAliveEnemies();
    if (enemies.empty()) return;

    if (mTargetIndex >= static_cast<int>(enemies.size()))
        mTargetIndex = 0;

    IBattler* target = enemies[mTargetIndex];
    mBattle.SetPlayerAction(mSkillIndex, target);

    LOG("[BattleState] Action confirmed: %s -> %s",
        skill->GetName(), target->GetName().c_str());

    SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
}

// ------------------------------------------------------------
// HandleItemSelect: cursor over the inventory list.
//
// Targeting shortcut:
//   Items whose targeting rule does NOT need a battler (SelfOnly /
//   AllAllies / AllEnemies) commit immediately on Enter — no second
//   menu.  Single-target items advance to ITEM_TARGET_SELECT.
//
// Empty inventory:
//   When the player owns nothing, Enter is a no-op and Esc returns
//   to the command menu.  RefreshItemList already cleared mItemIds.
// ------------------------------------------------------------
void BattleInputController::HandleItemSelect()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    const int count = static_cast<int>(mItemIds.size());

    if (count == 0)
    {
        // Empty bag — only Esc does anything.
        if (pressed(VK_BACK, mBackWasDown))
        {
            LOG("%s", "[BattleState] No items to use — back to command menu.");
            SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
        }
        return;
    }

    if (pressed(VK_UP, mKeyUpWasDown))
    {
        mItemIndex = (mItemIndex - 1 + count) % count;
        LOG("[BattleState] Item cursor -> %s", mItemIds[mItemIndex].c_str());
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mItemIndex = (mItemIndex + 1) % count;
        LOG("[BattleState] Item cursor -> %s", mItemIds[mItemIndex].c_str());
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_RETURN, mEnterWasDown))
    {
        const ItemData* item = ItemRegistry::Get().Find(mItemIds[mItemIndex]);
        if (!item)
        {
            // Inventory referenced an id with no registry entry — most
            // likely a typo in a JSON file.  Refuse the action so the
            // player loses neither the item nor the turn.
            LOG("[BattleState] Item id '%s' has no registry entry.",
                mItemIds[mItemIndex].c_str());
            return;
        }

        // Skip target select for items with implicit targeting.
        const bool implicit =
            item->targeting == ItemTargeting::SelfOnly ||
            item->targeting == ItemTargeting::AllAllies ||
            item->targeting == ItemTargeting::AllEnemies;

        if (implicit)
        {
            // Commit immediately with a null primary target — BuildItemActions
            // resolves the actual target list from the targeting rule.
            mBattle.SetPlayerItem(item->id, nullptr);
            LOG("[BattleState] Item confirmed: %s (implicit target)",
                item->name.c_str());
            SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
        }
        else
        {
            // Need a target.  Default mTargetIndex to 0 so the cursor
            // starts on the first valid candidate.
            mTargetIndex = 0;
            SetInputPhase(PlayerInputPhase::ITEM_TARGET_SELECT);
        }
    }
    if (pressed(VK_BACK, mBackWasDown))
    {
        LOG("%s", "[BattleState] Cancelled item select — back to command menu.");
        SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
    }
}

// ------------------------------------------------------------
// HandleItemTargetSelect: pick a single battler for an item.
//
// Target list source depends on the item's targeting rule:
//   SingleAlly / SingleAllyAny -> alive (or all) players
//   SingleEnemy                -> alive enemies
// Other rules never reach this handler — they auto-commit in HandleItemSelect.
//
// SingleAllyAny includes KO'd allies (revive items).  All other ally
// lookups exclude the dead so the cursor never lands on a corpse.
// ------------------------------------------------------------
void BattleInputController::HandleItemTargetSelect()
{
    auto pressed = [](int vk, bool& wasDown) -> bool {
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool fresh = down && !wasDown;
        wasDown = down;
        return fresh;
    };

    const ItemData* item = ItemRegistry::Get().Find(mItemIds[mItemIndex]);
    if (!item)
    {
        SetInputPhase(PlayerInputPhase::ITEM_SELECT);
        return;
    }

    // Resolve the candidate list once per frame so the cursor cycles
    // over the right pool.
    std::vector<IBattler*> candidates;
    if (item->targeting == ItemTargeting::SingleAlly)
    {
        candidates = mBattle.GetAlivePlayers();
    }
    else if (item->targeting == ItemTargeting::SingleAllyAny)
    {
        candidates = mBattle.GetAllPlayers();   // alive + dead
    }
    else if (item->targeting == ItemTargeting::SingleEnemy)
    {
        candidates = mBattle.GetAliveEnemies();
    }

    if (candidates.empty())
    {
        // No valid target (rare — e.g. revive with no dead allies).
        // Bounce back to ITEM_SELECT so the player can pick something else.
        LOG("%s", "[BattleState] No valid target for this item.");
        SetInputPhase(PlayerInputPhase::ITEM_SELECT);
        return;
    }

    const int n = static_cast<int>(candidates.size());
    if (mTargetIndex >= n) mTargetIndex = 0;

    if (pressed(VK_UP, mKeyUpWasDown))
    {
        mTargetIndex = (mTargetIndex - 1 + n) % n;
        LOG("[BattleState] Item target -> %s",
            candidates[mTargetIndex]->GetName().c_str());
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_DOWN, mKeyDownWasDown))
    {
        mTargetIndex = (mTargetIndex + 1) % n;
        LOG("[BattleState] Item target -> %s",
            candidates[mTargetIndex]->GetName().c_str());
        mState.DumpStateToDebugOutput();
    }
    if (pressed(VK_RETURN, mEnterWasDown))
    {
        ConfirmItemAndTarget();
    }
    if (pressed(VK_BACK, mBackWasDown))
    {
        LOG("%s", "[BattleState] Cancelled target — back to item select.");
        SetInputPhase(PlayerInputPhase::ITEM_SELECT);
    }
}

// ------------------------------------------------------------
// ConfirmItemAndTarget: commit the item + chosen battler to BattleManager.
// Symmetric with ConfirmSkillAndTarget — both call SetPlayer*() then
// reset the input phase to COMMAND_SELECT so the simulation can take over.
// ------------------------------------------------------------
void BattleInputController::ConfirmItemAndTarget()
{
    auto* player = mBattle.GetActivePlayer();
    if (!player) return;

    if (mItemIndex < 0 || mItemIndex >= static_cast<int>(mItemIds.size()))
        return;

    const ItemData* item = ItemRegistry::Get().Find(mItemIds[mItemIndex]);
    if (!item) return;

    // Resolve the target reference one more time using the same rules
    // as HandleItemTargetSelect — re-querying instead of caching keeps
    // this method tolerant of frame-to-frame roster changes.
    std::vector<IBattler*> candidates;
    if (item->targeting == ItemTargeting::SingleAlly)
        candidates = mBattle.GetAlivePlayers();
    else if (item->targeting == ItemTargeting::SingleAllyAny)
        candidates = mBattle.GetAllPlayers();
    else if (item->targeting == ItemTargeting::SingleEnemy)
        candidates = mBattle.GetAliveEnemies();

    if (candidates.empty()) return;
    if (mTargetIndex >= static_cast<int>(candidates.size()))
        mTargetIndex = 0;

    IBattler* target = candidates[mTargetIndex];
    mBattle.SetPlayerItem(item->id, target);

    LOG("[BattleState] Item confirmed: %s -> %s",
        item->name.c_str(), target->GetName().c_str());

    SetInputPhase(PlayerInputPhase::COMMAND_SELECT);
}

void BattleInputController::SetInputPhase(PlayerInputPhase phase)
{
    mInputPhase = phase;

    if (phase == PlayerInputPhase::COMMAND_SELECT) mCommandIndex = 0;
    if (phase == PlayerInputPhase::SKILL_SELECT)   mSkillIndex   = 0;
    if (phase == PlayerInputPhase::TARGET_SELECT)  mTargetIndex  = 0;

    // Refresh the inventory snapshot the moment the player enters the
    // item menu so a stack that hit 0 last turn disappears immediately.
    if (phase == PlayerInputPhase::ITEM_SELECT)
    {
        RefreshItemList();
    }

    constexpr int kActivePlayerSlot = 0;

    switch (phase)
    {
    case PlayerInputPhase::SKILL_SELECT:
    case PlayerInputPhase::ITEM_SELECT:
        // Both skill and item menus focus the camera on the active hero.
        mRenderer.SetCameraPhase(BattleCameraPhase::ACTOR_FOCUS,
                                 kActivePlayerSlot, -1);
        break;

    case PlayerInputPhase::TARGET_SELECT:
    case PlayerInputPhase::ITEM_TARGET_SELECT:
        // Target focus works the same way for both: pan to the picked
        // candidate.  Item targets may resolve to allies, in which case
        // SetCameraPhase still pans correctly because slot indices are
        // shared between teams in BattleRenderer's coordinate system.
        mRenderer.SetCameraPhase(BattleCameraPhase::TARGET_FOCUS,
                                 kActivePlayerSlot, mTargetIndex);
        break;

    case PlayerInputPhase::COMMAND_SELECT:
    default:
        mRenderer.SetCameraPhase(BattleCameraPhase::OVERVIEW, -1, -1);
        break;
    }

    mState.DumpStateToDebugOutput();
}
