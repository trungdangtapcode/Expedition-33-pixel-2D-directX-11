// ============================================================
// File: FightCommand.cpp
// ============================================================
#include "FightCommand.h"
#include "../States/BattleState.h"   // needs full type to call SetInputPhase

void FightCommand::Execute(BattleState& state) const
{
    // Transition the input phase so HandleInput now listens for skill keys
    // (1/2/3) instead of command cursor keys.
    state.SetInputPhase(PlayerInputPhase::SKILL_SELECT);
}
