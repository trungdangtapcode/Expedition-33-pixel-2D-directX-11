// ============================================================
// File: FightCommand.h
// Responsibility: Top-level "Fight" battle menu command.
//
// When the player selects "Fight", this command advances the input phase
// to SKILL_SELECT so the player can then choose which skill to use.
//
// No simulation side-effects — this is a pure UI routing decision.
// ============================================================
#pragma once
#include "IBattleCommand.h"

class FightCommand : public IBattleCommand
{
public:
    const char* GetLabel() const override { return "Fight"; }

    // Advance the BattleState input phase to SKILL_SELECT.
    void Execute(BattleState& state) const override;
};
