#include "StateManager.h"
#include <cassert>

StateManager& StateManager::Get() {
    // Meyers' Singleton: thread-safe kể từ C++11
    static StateManager instance;
    return instance;
}

void StateManager::PushState(std::unique_ptr<IGameState> state) {
    assert(state != nullptr && "StateManager::PushState - state không được là nullptr");

    // Nếu đang có state, tạm "pause" nó (không gọi gì thêm ở đây -
    // state dưới sẽ không bị Update/Render nữa vì Update/Render
    // chỉ gọi top của stack)
    state->OnEnter();
    mStates.push(std::move(state));
}

void StateManager::PopState() {
    if (!mStates.empty()) {
        mStates.top()->OnExit();   // Dọn dẹp trước khi xóa
        mStates.pop();             // Giải phóng unique_ptr → bộ nhớ tự động free

        // Nếu còn state phía dưới, không cần gọi OnEnter lại -
        // state đó đã được init từ trước, chỉ cần tiếp tục Update/Render
    }
}

void StateManager::ChangeState(std::unique_ptr<IGameState> state) {
    assert(state != nullptr && "StateManager::ChangeState - state không được là nullptr");

    // Pop state hiện tại (nếu có)
    if (!mStates.empty()) {
        mStates.top()->OnExit();
        mStates.pop();
    }
    // Push state mới
    state->OnEnter();
    mStates.push(std::move(state));
}

void StateManager::Update(float dt) {
    if (!mStates.empty()) {
        // Chỉ Update state đang ở đỉnh stack
        mStates.top()->Update(dt);
    }
}

void StateManager::Render() {
    if (!mStates.empty()) {
        // Chỉ Render state đang ở đỉnh stack
        // (Mở rộng sau: có thể render nhiều state nếu state dưới là "transparent")
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
