// ============================================================
// File: CampfireState.h
// Responsibility: Modal campfire hub for rest, save, load,
//                 training, and party lineup access.
//
// Lifetime:
//   Pushed in  -> OverworldState when the player presses U near a campfire.
//   Popped via -> Escape, Backspace, U, or the Exit menu option.
//
// Important:
//   - The campfire entity remains passive. This state owns menu input.
//   - Save/load still goes through SaveManager.
//   - Party recovery and training still go through PartyManager.
// ============================================================
#pragma once

#include "IGameState.h"
#include "../Renderer/NineSliceRenderer.h"
#include "../UI/BattleTextRenderer.h"
#include "../UI/CurrencyHudRenderer.h"
#include <string>

class CampfireState : public IGameState
{
public:
    CampfireState(std::string campfireId, int upgradeExpReward, float playerX, float playerY);

    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    const char* GetName() const override { return "CampfireState"; }

private:
    enum class Phase
    {
        MainMenu,
        SaveSlotSelect,
        LoadSlotSelect
    };

    enum class MenuOption
    {
        Rest,
        Save,
        Load,
        Training,
        Lineup,
        Exit,
        Count
    };

    bool Pressed(int vk, bool& wasDown);
    void ActivateSelection();
    void ActivateSlotSelection();
    void Flash(const std::string& message);
    void UpdateSavedOverworldSnapshot();

    static std::string OptionLabel(MenuOption option);
    static int OptionCount()
    {
        return static_cast<int>(MenuOption::Count);
    }

    void RenderMainMenu(float panelX, float panelY);
    void RenderSlotMenu(float panelX, float panelY);

    std::string mCampfireId;
    int mUpgradeExpReward = 0;
    float mPlayerX = 0.0f;
    float mPlayerY = 0.0f;

    NineSliceRenderer mDialogBox;
    BattleTextRenderer mTextRenderer;
    CurrencyHudRenderer mCurrencyHud;

    Phase mPhase = Phase::MainMenu;
    int mCursor = 0;
    int mSlotCursor = 0;
    std::string mFlashMessage;
    float mFlashTimer = 0.0f;
    float mElapsed = 0.0f;

    bool mUpWasDown = false;
    bool mDownWasDown = false;
    bool mEnterWasDown = false;
    bool mEscWasDown = false;
    bool mBackWasDown = false;
    bool mUWasDown = false;
};
