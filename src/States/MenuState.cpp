// ============================================================
// File: MenuState.cpp
// Responsibility: Route menu input to New Game or Continue.
//
// Transitions:
//   Enter -> reset durable systems, create an initial checkpoint, then
//            ChangeState(OverworldState).
//   C     -> load the checkpoint, then ChangeState(OverworldState).
//
// Lifetime:
//   OnEnter is called when StateManager pushes this state.
//   OnExit is called before the state is popped or replaced.
// ============================================================
#include "MenuState.h"
#include "OverworldState.h"
#include "StateManager.h"
#include "../Audio/AudioManager.h"
#include "../Systems/GameProgress.h"
#include "../Systems/Inventory.h"
#include "../Systems/PartyManager.h"
#include "../Systems/SaveManager.h"
#include "../Utils/Log.h"
#include <memory>
#include <string>
#include <windows.h>

void MenuState::OnEnter()
{
    SaveManager::Get().Initialize();
    mEnterWasDown = false;
    mContinueWasDown = false;

    LOG("[MenuState] OnEnter. Enter = New Game. C = Continue.");
    if (SaveManager::Get().CheckpointExists())
    {
        LOG("[MenuState] Continue checkpoint available at '%s'.",
            SaveManager::Get().GetConfig().slotPath.c_str());
    }
}

void MenuState::OnExit()
{
    LOG("[MenuState] OnExit");
}

void MenuState::Update(float /*dt*/)
{
    const bool enterDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    const bool enterPressed = enterDown && !mEnterWasDown;
    mEnterWasDown = enterDown;

    if (enterPressed)
    {
        AudioManager::Get().PlaySfx("ui_confirm");

        PartyManager::Get().ResetToDefaults();
        Inventory::Get().ResetToDefaults();
        GameProgress::Get().Reset();
        SaveManager::Get().SaveCheckpoint("new_game");

        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
        return;
    }

    const bool continueDown = (GetAsyncKeyState('C') & 0x8000) != 0;
    const bool continuePressed = continueDown && !mContinueWasDown;
    mContinueWasDown = continueDown;

    if (continuePressed)
    {
        std::string sceneId;
        if (!SaveManager::Get().LoadCheckpoint(&sceneId))
        {
            AudioManager::Get().PlaySfx("battle_no_ap");
            LOG("[MenuState] Continue requested, but no valid checkpoint was loaded.");
            return;
        }

        AudioManager::Get().PlaySfx("ui_confirm");
        if (sceneId != "overworld")
        {
            LOG("[MenuState] Save scene '%s' is not implemented yet; loading overworld fallback.",
                sceneId.c_str());
        }

        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
    }
}

void MenuState::Render()
{
    // The current menu has no authored UI renderer yet. D3DContext clears the
    // back buffer, and logs document the available input paths during this
    // checkpoint-system milestone.
}
