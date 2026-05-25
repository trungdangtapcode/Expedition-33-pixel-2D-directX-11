// ============================================================
// File: StateManager.cpp
// Responsibility: Implement stack ownership and overlay-aware rendering.
//
// Common mistakes:
//   1. Updating lower states while an overlay is active freezes nothing.
//   2. Rendering only the top state prevents pause menus from showing gameplay.
//   3. Leaving old states below ChangeState() leaks scene intent and events.
// ============================================================
#include "StateManager.h"
#include <cassert>
#include <cstddef>

StateManager& StateManager::Get()
{
    static StateManager instance;
    return instance;
}

void StateManager::PushState(std::unique_ptr<IGameState> state)
{
    assert(state != nullptr && "StateManager::PushState - state must not be nullptr");

    state->OnEnter();
    mStates.push_back(std::move(state));
}

void StateManager::PopState()
{
    if (mStates.empty()) return;

    mStates.back()->OnExit();
    mStates.pop_back();
}

void StateManager::ChangeState(std::unique_ptr<IGameState> state)
{
    assert(state != nullptr && "StateManager::ChangeState - state must not be nullptr");

    while (!mStates.empty())
    {
        mStates.back()->OnExit();
        mStates.pop_back();
    }

    state->OnEnter();
    mStates.push_back(std::move(state));
}

void StateManager::Update(float dt)
{
    if (mStates.empty()) return;

    mStates.back()->Update(dt);
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
