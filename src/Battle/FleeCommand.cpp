// ============================================================
// File: FleeCommand.cpp
// ============================================================
#include "FleeCommand.h"
#include "../States/BattleState.h"
#include "../States/StateManager.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"

void FleeCommand::Execute(BattleState& /*state*/) const
{
    LOG("%s", "[FleeCommand] Player fled from battle.");

    // Notify any listener (PlayState, CutsceneSystem, …) that the player
    // chose to flee.  The listener decides what to show — no logic here.
    EventData data;
    data.name = "battle_flee";
    EventManager::Get().Broadcast("battle_flee", data);

    // Leave BattleState — equivalent to a defeat outcome for inventory
    // purposes; loot is NOT awarded.  PartyManager HP is NOT written back
    // here; BattleState::OnExit() handles persistence.
    StateManager::Get().PopState();
}
