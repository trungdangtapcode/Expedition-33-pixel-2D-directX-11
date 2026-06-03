#pragma once

#include "IGameState.h"
#include <memory>
#include <vector>

// ============================================================
// File: StateManager.h
// Responsibility: Stack-based owner for active game states.
//
// Owns:
//   A vector-backed stack of unique_ptr<IGameState>.
//
// Lifetime:
//   Created in  -> StateManager::Get() on first use.
//   Destroyed in -> Process shutdown.
//
// Important:
//   - Update() only advances the top state.
//   - Render() can draw a visible chain when the top state is an overlay.
//   - ChangeState() clears the full stack before pushing the replacement.
//   - Stack mutations requested during Update() are applied after Update()
//     returns, so a state cannot destroy itself while executing its own method.
// ============================================================
class StateManager
{
public:
    static StateManager& Get();

    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    void PushState(std::unique_ptr<IGameState> state);
    void PopState();
    void ChangeState(std::unique_ptr<IGameState> state);

    void Update(float dt);
    void Render();

    bool IsEmpty() const;
    IGameState* GetCurrentState() const;

private:
    StateManager() = default;

    enum class PendingOperationType
    {
        Push,
        Pop,
        Change
    };

    struct PendingOperation
    {
        PendingOperationType type = PendingOperationType::Pop;
        std::unique_ptr<IGameState> state;
    };

    void QueueOrApply(PendingOperation operation);
    void ApplyOperation(PendingOperation& operation);
    void ApplyPendingOperations();

    // Vector is used as a stack so Render() can walk the visible overlay chain
    // from bottom to top. The back element is always the active state.
    std::vector<std::unique_ptr<IGameState>> mStates;

    // State operations requested during Update() are stored here until the
    // active state's call stack has fully unwound.
    std::vector<PendingOperation> mPendingOperations;
    bool mIsUpdating = false;
};
