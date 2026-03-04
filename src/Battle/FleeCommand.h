// ============================================================
// File: FleeCommand.h
// Responsibility: Top-level "Flee" battle menu command.
//
// Attempts to flee combat.  In the MVP all flee attempts succeed.
// Future: add a success-chance roll based on party SPD vs enemy SPD;
// on failure, broadcast "flee_failed" and stay in COMMAND_SELECT.
//
// On success: broadcasts "battle_flee" via EventManager so any listener
// (e.g. PlayState) can react, then calls StateManager::PopState() to
// leave the battle.
// ============================================================
#pragma once
#include "IBattleCommand.h"

class FleeCommand : public IBattleCommand
{
public:
    const char* GetLabel() const override { return "Flee"; }

    // Broadcast "battle_flee" and pop BattleState.
    void Execute(BattleState& state) const override;
};
