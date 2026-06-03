// ============================================================
// File: StateManager.cpp
// Responsibility: Implement stack ownership and overlay-aware rendering.
//
// Common mistakes:
//   1. Updating lower states while an overlay is active freezes nothing.
//   2. Rendering only the top state prevents pause menus from showing gameplay.
//   3. Leaving old states below ChangeState() leaks scene intent and events.
//   4. Destroying the top state while its Update() method is still running
//      creates use-after-free behavior. Queue state operations during Update().
// ============================================================
#include "StateManager.h"
#include <cassert>
#include <cstddef>
#include <utility>

StateManager& StateManager::Get()
{
    static StateManager instance;
    return instance;
}

void StateManager::PushState(std::unique_ptr<IGameState> state)
{
    assert(state != nullptr && "StateManager::PushState - state must not be nullptr");

    PendingOperation operation{};
    operation.type = PendingOperationType::Push;
    operation.state = std::move(state);
    QueueOrApply(std::move(operation));
}

void StateManager::PopState()
{
    PendingOperation operation{};
    operation.type = PendingOperationType::Pop;
    QueueOrApply(std::move(operation));
}

void StateManager::ChangeState(std::unique_ptr<IGameState> state)
{
    assert(state != nullptr && "StateManager::ChangeState - state must not be nullptr");

    PendingOperation operation{};
    operation.type = PendingOperationType::Change;
    operation.state = std::move(state);
    QueueOrApply(std::move(operation));
}

void StateManager::Update(float dt)
{
    if (mStates.empty()) return;

    mIsUpdating = true;
    mStates.back()->Update(dt);
    mIsUpdating = false;

    ApplyPendingOperations();
}

void StateManager::Render()
{
    if (mStates.empty()) return;

    std::size_t firstVisible = mStates.size() - 1;
    while (firstVisible > 0 && mStates[firstVisible]->ShouldRenderBelow())
    {
        --firstVisible;
    }

    for (std::size_t i = firstVisible; i < mStates.size(); ++i)
    {
        mStates[i]->Render();
    }
}

bool StateManager::IsEmpty() const
{
    return mStates.empty();
}

IGameState* StateManager::GetCurrentState() const
{
    if (mStates.empty()) return nullptr;
    return mStates.back().get();
}

void StateManager::QueueOrApply(PendingOperation operation)
{
    if (mIsUpdating)
    {
        mPendingOperations.push_back(std::move(operation));
        return;
    }

    ApplyOperation(operation);
}

void StateManager::ApplyOperation(PendingOperation& operation)
{
    switch (operation.type)
    {
    case PendingOperationType::Push:
        assert(operation.state != nullptr && "StateManager push operation requires a state.");
        operation.state->OnEnter();
        mStates.push_back(std::move(operation.state));
        break;

    case PendingOperationType::Pop:
        if (mStates.empty()) return;
        mStates.back()->OnExit();
        mStates.pop_back();
        break;

    case PendingOperationType::Change:
        assert(operation.state != nullptr && "StateManager change operation requires a state.");
        while (!mStates.empty())
        {
            mStates.back()->OnExit();
            mStates.pop_back();
        }
        operation.state->OnEnter();
        mStates.push_back(std::move(operation.state));
        break;
    }
}

void StateManager::ApplyPendingOperations()
{
    if (mPendingOperations.empty()) return;

    std::vector<PendingOperation> operations;
    operations.swap(mPendingOperations);

    for (PendingOperation& operation : operations)
    {
        ApplyOperation(operation);
    }
}
