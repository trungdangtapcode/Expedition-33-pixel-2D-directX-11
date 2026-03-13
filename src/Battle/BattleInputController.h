// ============================================================
// File: BattleInputController.h
// Responsibility: Handles player input during PLAYER_TURN in battle.
//                 Manages the combat menu FSM and command list.
// ============================================================
#pragma once
#include <vector>
#include <memory>

class BattleState;
class BattleManager;
class BattleRenderer;
class IBattleCommand;

enum class PlayerInputPhase
{
    COMMAND_SELECT,   // top-level: Fight / Flee / Item …
    SKILL_SELECT,     // which skill (1/2/3)
    TARGET_SELECT     // which enemy (Tab to cycle, Enter to confirm)
};

class BattleInputController
{
public:
    BattleInputController(BattleState& state, BattleManager& battle, BattleRenderer& renderer);
    ~BattleInputController();

    void Initialize();
    void HandleInput();
    void SetInputPhase(PlayerInputPhase phase);

    PlayerInputPhase GetInputPhase() const { return mInputPhase; }
    int GetCommandIndex() const { return mCommandIndex; }
    int GetSkillIndex() const { return mSkillIndex; }
    int GetTargetIndex() const { return mTargetIndex; }

    const std::vector<std::unique_ptr<IBattleCommand>>& GetCommands() const { return mCommands; }

private:
    void BuildCommandList();
    void HandleCommandSelect();
    void HandleSkillSelect();
    void HandleTargetSelect();
    void ConfirmSkillAndTarget();

    BattleState& mState;
    BattleManager& mBattle;
    BattleRenderer& mRenderer;

    PlayerInputPhase mInputPhase = PlayerInputPhase::COMMAND_SELECT;
    int mCommandIndex = 0;
    int mSkillIndex = 0;
    int mTargetIndex = 0;

    bool mKeyUpWasDown = false;
    bool mKeyDownWasDown = false;
    bool mEnterWasDown = false;
    bool mBackWasDown = false;

    std::vector<std::unique_ptr<IBattleCommand>> mCommands;
};
