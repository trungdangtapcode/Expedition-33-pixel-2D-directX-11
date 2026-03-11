// ============================================================
// File: FleeCommand.cpp
// ============================================================
#include "FleeCommand.h"
#include "../States/BattleState.h"
#include "../Utils/Log.h"

void FleeCommand::Execute(BattleState& state) const
{
    LOG("%s", "[FleeCommand] Player chose to flee — deferring pop to end of Update().");

    // DO NOT call StateManager::PopState() here.
    // Execute() is called from inside BattleState::HandleCommandSelect(),
    // which is called from BattleState::Update().  Popping the state here
    // destroys BattleState while it is still on the call stack
    // (HandleCommandSelect -> HandleInput -> Update -> ...) — use-after-free crash.
    //
    // RequestFlee() sets a flag that BattleState::Update() checks AFTER all
    // handlers have returned and the call stack is clean.
    state.RequestFlee();
}
