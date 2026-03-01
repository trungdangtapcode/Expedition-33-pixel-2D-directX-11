#include "StateManager.h"
#include <cassert>

StateManager& StateManager::Get() {
    // Meyers' Singleton: thread-safe from C++11
    static StateManager instance;
    return instance;
}

void StateManager::PushState(std::unique_ptr<IGameState> state) {
    assert(state != nullptr && "StateManager::PushState - state must not be nullptr");

    // If there is a current state, temporarily "pause" it (no additional calls here -
    // the state below will not be Update/Render anymore because Update/Render
    // only calls the top of the stack)
    state->OnEnter();
    mStates.push(std::move(state));
}

void StateManager::PopState() {
    if (!mStates.empty()) {
        mStates.top()->OnExit();   // Call to clean up before deleting
        mStates.pop();             // Release unique_ptr → memory automatically freed

        // If there is a state below, no need to call OnEnter again -
        // that state was already initialized before, just continue Update/Render
    }
}

void StateManager::ChangeState(std::unique_ptr<IGameState> state) {
    assert(state != nullptr && "StateManager::ChangeState - state must not be nullptr");

    // Pop current state (if any)
    if (!mStates.empty()) {
        mStates.top()->OnExit();
        mStates.pop();
    }
    // Push new state
    state->OnEnter();
    mStates.push(std::move(state));
}

void StateManager::Update(float dt) {
    if (!mStates.empty()) {
        // Only update the state at the top of the stack
        mStates.top()->Update(dt);
    }
}

void StateManager::Render() {
    if (!mStates.empty()) {
        // Only render the state at the top of the stack
        // (Future extension: can render multiple states if the state below is "transparent")
        mStates.top()->Render();
    }
}

bool StateManager::IsEmpty() const {
    return mStates.empty();
}

IGameState* StateManager::GetCurrentState() const {
    if (mStates.empty()) return nullptr;
    return mStates.top().get();
}
