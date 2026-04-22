// ============================================================
// File: ItemCommand.h
// Responsibility: Top-level "Item" battle menu command.
//
// When the player picks "Item", advance the input phase to ITEM_SELECT
// so HandleInput now lists the inventory.  Pure UI routing — no
// simulation side-effects.
//
// Symmetry note:
//   Mirrors FightCommand exactly.  Both are pure phase transitions —
//   the actual gameplay happens after the player confirms a target,
//   inside BattleInputController::ConfirmItemAndTarget.
// ============================================================
#pragma once
#include "IBattleCommand.h"

class ItemCommand : public IBattleCommand
{
public:
    const char* GetLabel() const override { return "Item"; }

    // Advances BattleState input phase to ITEM_SELECT.
    void Execute(BattleState& state) const override;
};
