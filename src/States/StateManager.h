#pragma once
#include <stack>
#include <memory>
#include "IGameState.h"

// ============================================================
// StateManager - Stack-based manager for game states (Menu, Play, Pause, Cutscene...)
//
// PATTERN: Singleton + Stack-based State Machine
//
// Using a STACK (not just a simple pointer) because:
//   - When BattleState need to show PauseMenu, push PauseMenu onto stack.
//     BattleState remain underneath, waiting.
//   - When closing PauseMenu, pop it off. BattleState resumes.
//   - When Boss 50% HP triggers Cutscene:
//     push CutsceneState → run → pop → BattleState resumes.
//
// USAGE:
//   StateManager::Get().PushState(std::make_unique<MenuState>());
//   StateManager::Get().Update(dt);   // called in GameApp
//   StateManager::Get().Render();     // called in GameApp
// ============================================================
class StateManager {
public:
    // Singleton accessor
    static StateManager& Get();

    // Not allowing copy or move of Singleton
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    // --- State Stack Management ---

    // Push new state onto the stack and call OnEnter()
    void PushState(std::unique_ptr<IGameState> state);

    // Pop the current state, call OnExit(), and return to the state below
    void PopState();

    // Replace the current state with a new state (Pop + Push at the same time)
    // Used when transitioning scenes without needing to return (e.g., MainMenu → InGame)
    void ChangeState(std::unique_ptr<IGameState> state);

    // --- Main Loop - called by GameApp each frame ---
    void Update(float dt);
    void Render();

    // Check if the stack is empty (if empty, the game should exit)
    bool IsEmpty() const;

    // Get the currently active state (top of the stack) for debugging
    IGameState* GetCurrentState() const;

private:
    StateManager() = default;

    // Stack the state - top of the stack is the currently active state
    // unique_ptr ensures automatic memory management
    std::stack<std::unique_ptr<IGameState>> mStates;

    // Flag indicating whether a state change needs to be processed at the beginning of the next frame
    // (avoids modifying the stack DURING Update - which would cause undefined behavior). NOTE: This flag is checked in the Update() method.
    bool mPendingPop = false;
};
