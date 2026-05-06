// ============================================================
// File: FightCommand.cpp
// ============================================================
#include "FightCommand.h"
#include "../States/BattleState.h"   // needs full type to call SetInputPhase
#include "../Audio/AudioManager.h"

void FightCommand::Execute(BattleState& state) const
{
    // Audible feedback that the skill sub-menu is opening.
    AudioManager::Get().PlaySfx("battle_skill_open");

    // Transition the input phase so HandleInput now listens for skill keys
    // (1/2/3) instead of command cursor keys.
    state.SetInputPhase(PlayerInputPhase::SKILL_SELECT);
}
