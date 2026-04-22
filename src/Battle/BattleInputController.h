// ============================================================
// File: BattleInputController.h
// Responsibility: Handles player input during PLAYER_TURN in battle.
//                 Manages the combat menu FSM and command list.
// ============================================================
#pragma once
#include <vector>
#include <memory>
#include <string>

class BattleState;
class BattleManager;
class BattleRenderer;
class IBattleCommand;

enum class PlayerInputPhase
{
    COMMAND_SELECT,      // top-level: Fight / Item / Flee …
    SKILL_SELECT,        // which skill (1/2/3)
    TARGET_SELECT,       // which enemy (Up/Down to cycle, Enter to confirm)
    ITEM_SELECT,         // which item from inventory
    ITEM_TARGET_SELECT   // which battler (ally OR enemy) the item lands on
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
    int GetItemIndex()   const { return mItemIndex; }

    const std::vector<std::unique_ptr<IBattleCommand>>& GetCommands() const { return mCommands; }

    // Owned-id snapshot rebuilt at the start of every ITEM_SELECT entry.
    // BattleState's debug HUD reads this to render the inventory list with
    // the same indexing the controller is currently using.
    const std::vector<std::string>& GetItemIds() const { return mItemIds; }

private:
    void BuildCommandList();
    void HandleCommandSelect();
    void HandleSkillSelect();
    void HandleTargetSelect();
    void HandleItemSelect();
    void HandleItemTargetSelect();
    void ConfirmSkillAndTarget();
    void ConfirmItemAndTarget();

    // Refresh mItemIds from Inventory::OwnedIds().  Called whenever the
    // controller enters ITEM_SELECT so the menu reflects current counts
    // (an item used last turn vanishes if its stack hit 0).
    void RefreshItemList();

    BattleState& mState;
    BattleManager& mBattle;
    BattleRenderer& mRenderer;

    PlayerInputPhase mInputPhase = PlayerInputPhase::COMMAND_SELECT;
    int mCommandIndex = 0;
    int mSkillIndex = 0;
    int mTargetIndex = 0;
    int mItemIndex   = 0;

    // Snapshot of Inventory::OwnedIds() taken when the player enters
    // ITEM_SELECT.  Refreshed only on phase entry, not per-frame, so
    // the cursor stays stable while scrolling.
    std::vector<std::string> mItemIds;

    bool mKeyUpWasDown = false;
    bool mKeyDownWasDown = false;
    bool mEnterWasDown = false;
    bool mBackWasDown = false;

    std::vector<std::unique_ptr<IBattleCommand>> mCommands;
};
