// ============================================================
// File: MenuState.cpp
// Responsibility: Route menu input to New Game or Continue.
//
// Transitions:
//   Enter -> reset durable systems, create an initial save in slot 1, then
//            ChangeState(OverworldState).
//   C     -> load the first occupied save slot, then ChangeState(OverworldState).
//   1..9  -> load that numbered save slot when it exists.
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
    for (bool& wasDown : mSlotWasDown)
    {
        wasDown = false;
    }

    LOG("[MenuState] OnEnter. Enter = New Game Slot 1. C = Continue first slot. 1-9 = Continue specific slot.");
    for (const SaveSlotInfo& slot : SaveManager::Get().GetSlotInfos())
    {
        if (slot.exists)
        {
            LOG("[MenuState] Slot %d available at '%s'.",
                slot.slotIndex + 1, slot.path.c_str());
        }
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
        SaveManager::Get().SaveCheckpointToSlot(0, "new_game");

        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
        return;
    }

    const bool continueDown = (GetAsyncKeyState('C') & 0x8000) != 0;
    const bool continuePressed = continueDown && !mContinueWasDown;
    mContinueWasDown = continueDown;

    if (continuePressed)
    {
        const int firstSlot = SaveManager::Get().FindFirstExistingSlot();
        if (firstSlot < 0)
        {
            AudioManager::Get().PlaySfx("battle_no_ap");
            LOG("[MenuState] Continue requested, but no save slot exists.");
            return;
        }

        std::string sceneId;
        if (!SaveManager::Get().LoadCheckpointFromSlot(firstSlot, &sceneId))
        {
            AudioManager::Get().PlaySfx("battle_no_ap");
            LOG("[MenuState] Continue requested, but slot %d could not be loaded.",
                firstSlot + 1);
            return;
        }

        AudioManager::Get().PlaySfx("ui_confirm");
        if (sceneId != "overworld")
        {
            LOG("[MenuState] Save scene '%s' is not implemented yet; loading overworld fallback.",
                sceneId.c_str());
        }

        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
        return;
    }

    const int slotCount = SaveManager::Get().GetSlotCount();
    const int checkedSlots = (slotCount < 9) ? slotCount : 9;
    for (int i = 0; i < checkedSlots; ++i)
    {
        const int key = '1' + i;
        const bool slotDown = (GetAsyncKeyState(key) & 0x8000) != 0;
        const bool slotPressed = slotDown && !mSlotWasDown[i];
        mSlotWasDown[i] = slotDown;

        if (!slotPressed) continue;

        std::string sceneId;
        if (!SaveManager::Get().LoadCheckpointFromSlot(i, &sceneId))
        {
            AudioManager::Get().PlaySfx("battle_no_ap");
            LOG("[MenuState] Slot %d requested, but it is empty or invalid.", i + 1);
            return;
        }

        AudioManager::Get().PlaySfx("ui_confirm");
        if (sceneId != "overworld")
        {
            LOG("[MenuState] Save scene '%s' is not implemented yet; loading overworld fallback.",
                sceneId.c_str());
        }

        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
        return;
    }
}

void MenuState::Render()
{
    // The current menu has no authored UI renderer yet. D3DContext clears the
    // back buffer, and logs document the available input paths during this
    // checkpoint-system milestone.
}
