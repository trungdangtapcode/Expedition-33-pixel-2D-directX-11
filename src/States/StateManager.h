#pragma once
#include <stack>
#include <memory>
#include "IGameState.h"

// ============================================================
// StateManager - Quản lý stack các trạng thái game
//
// PATTERN: Singleton + Stack-based State Machine
//
// Dùng STACK (không phải đơn giản là 1 con trỏ) vì:
//   - Khi BattleState cần hiện PauseMenu, push PauseMenu lên stack.
//     BattleState vẫn còn đó, chờ ở dưới.
//   - Khi đóng PauseMenu, pop nó ra. BattleState tiếp tục.
//   - Khi Boss 50% HP kích hoạt Cutscene:
//     push CutsceneState → chạy → pop → BattleState tiếp tục.
//
// CÁCH DÙNG:
//   StateManager::Get().PushState(std::make_unique<MenuState>());
//   StateManager::Get().Update(dt);   // gọi trong GameApp
//   StateManager::Get().Render();     // gọi trong GameApp
// ============================================================
class StateManager {
public:
    // Singleton accessor
    static StateManager& Get();

    // Không cho phép copy hoặc move Singleton
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    // --- Quản lý State Stack ---

    // Đẩy state mới lên đỉnh stack và gọi OnEnter()
    void PushState(std::unique_ptr<IGameState> state);

    // Pop state hiện tại, gọi OnExit(), quay lại state phía dưới
    void PopState();

    // Thay thế state hiện tại bằng state mới (Pop + Push cùng lúc)
    // Dùng khi chuyển cảnh không cần quay lại (vd: MainMenu → InGame)
    void ChangeState(std::unique_ptr<IGameState> state);

    // --- Vòng lặp chính - được GameApp gọi mỗi frame ---
    void Update(float dt);
    void Render();

    // Kiểm tra stack có rỗng không (nếu rỗng, game nên thoát)
    bool IsEmpty() const;

    // Lấy state đang active (đỉnh stack) để debug
    IGameState* GetCurrentState() const;

private:
    StateManager() = default;

    // Stack các state - đỉnh stack là state đang active
    // unique_ptr đảm bảo tự động giải phóng bộ nhớ
    std::stack<std::unique_ptr<IGameState>> mStates;

    // Cờ cho biết cần xử lý thay đổi state ở đầu frame tiếp theo
    // (tránh thay đổi stack TRONG khi đang Update - gây undefined behavior)
    bool mPendingPop = false;
};
