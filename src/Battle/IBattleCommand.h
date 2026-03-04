// ============================================================
// File: IBattleCommand.h
// Responsibility: Pure virtual interface for one top-level battle menu entry.
//
// Each option the player sees before choosing a skill (Fight, Flee, Item, …)
// is one concrete IBattleCommand.  BattleState owns a flat list of these and
// knows nothing about what any of them does — it just calls Execute().
//
// Design rationale — why a separate interface instead of a plain enum?
//   An enum forces a switch statement in BattleState that grows with every
//   new feature (Open/Closed violation).  With IBattleCommand, adding "Item"
//   means writing one new class and appending it to the command list in
//   BattleState::BuildCommandList() — zero edits to existing code.
//
// Relationship to IAction:
//   IAction is a one-frame simulation step (DamageAction, WaitAction).
//   IBattleCommand is a one-press UI decision that changes the INPUT phase.
//   They are orthogonal abstractions and must never be mixed.
//
// Concrete implementations:
//   FightCommand  — advance input phase to SKILL_SELECT
//   FleeCommand   — attempt to flee; pop BattleState on success
//   (future) ItemCommand — advance to ITEM_SELECT sub-menu
//
// Ownership:
//   BattleState owns all commands via unique_ptr, stored in mCommands.
//   Execute() receives a raw BattleState& — non-owning, valid for the call.
// ============================================================
#pragma once
#include <string>

// Forward declaration — IBattleCommand must not pull in the full BattleState header
// (BattleState.h would create a circular include).  Concrete implementations
// that actually need BattleState members include BattleState.h in their .cpp.
class BattleState;

class IBattleCommand
{
public:
    virtual ~IBattleCommand() = default;

    // ------------------------------------------------------------
    // GetLabel: display name shown in the command menu.
    //   Used by DumpStateToDebugOutput — all UI lives in the debug console
    //   until a real on-screen menu is implemented.
    // ------------------------------------------------------------
    virtual const char* GetLabel() const = 0;

    // ------------------------------------------------------------
    // Execute: respond to the player pressing Enter on this command.
    //   state — the owning BattleState; use its public API to change
    //           the input phase or enqueue simulation actions.
    //   Implementations MUST be idempotent for a single press —
    //   BattleState guarantees Execute is called at most once per key event.
    // ------------------------------------------------------------
    virtual void Execute(BattleState& state) const = 0;
};
