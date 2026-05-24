// ============================================================
// File: ItemCommand.cpp
// ============================================================
#include "ItemCommand.h"
#include "../States/BattleState.h"   // SetInputPhase needs full type
#include "../Audio/AudioManager.h"
#include "../Systems/LocalizationManager.h"

std::string ItemCommand::GetLabel() const
{
    return LocalizationManager::Get().Text("battle.command.item");
}

std::string ItemCommand::GetDebugLabel() const
{
    return LocalizationManager::Get().TextEnglish("battle.command.item");
}

void ItemCommand::Execute(BattleState& state) const
{
    // Audible feedback that the item sub-menu is opening.
    AudioManager::Get().PlaySfx("battle_item_open");

    // Switch the input FSM to inventory selection.  HandleInput then
    // lists owned items and waits for Up/Down/Enter/Esc.
    state.SetInputPhase(PlayerInputPhase::ITEM_SELECT);
}
