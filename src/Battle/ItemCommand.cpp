// ============================================================
// File: ItemCommand.cpp
// ============================================================
#include "ItemCommand.h"
#include "../States/BattleState.h"   // SetInputPhase needs full type

void ItemCommand::Execute(BattleState& state) const
{
    // Switch the input FSM to inventory selection.  HandleInput then
    // lists owned items and waits for Up/Down/Enter/Esc.
    state.SetInputPhase(PlayerInputPhase::ITEM_SELECT);
}
