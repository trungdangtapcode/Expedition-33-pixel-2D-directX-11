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
    BuildCommandList();
}

void BattleInputController::BuildCommandList()
{
    mCommands.clear();
    mCommands.push_back(std::make_unique<FightCommand>());
    mCommands.push_back(std::make_unique<FleeCommand>());
    // Future: mCommands.push_back(std::make_unique<ItemCommand>());
    // Future: mCommands.push_back(std::make_unique<DefendCommand>());
}

void BattleInputController::HandleInput()
{
    switch (mInputPhase)
    {
    case PlayerInputPhase::COMMAND_SELECT: HandleCommandSelect(); break;
    case PlayerInputPhase::SKILL_SELECT:   HandleSkillSelect();   break;
    case PlayerInputPhase::TARGET_SELECT:  HandleTargetSelect();  break;
    }
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
        if (!skill || !skill->CanUse(*player))
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
    if (!skill || !skill->CanUse(*player))
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

void BattleInputController::SetInputPhase(PlayerInputPhase phase)
{
    mInputPhase = phase;

    if (phase == PlayerInputPhase::COMMAND_SELECT) mCommandIndex = 0;
    if (phase == PlayerInputPhase::SKILL_SELECT)   mSkillIndex   = 0;
    if (phase == PlayerInputPhase::TARGET_SELECT)  mTargetIndex  = 0;

    constexpr int kActivePlayerSlot = 0;

    switch (phase)
    {
    case PlayerInputPhase::SKILL_SELECT:
        mRenderer.SetCameraPhase(BattleCameraPhase::ACTOR_FOCUS,
                                 kActivePlayerSlot, -1);
        break;

    case PlayerInputPhase::TARGET_SELECT:
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
