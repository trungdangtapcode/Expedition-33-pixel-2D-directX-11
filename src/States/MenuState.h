#pragma once
#include "IGameState.h"

// ============================================================
// MenuState - State màn hình chính (Main Menu)
//
// Đây là State đầu tiên được push vào stack khi game khởi động.
// Khi người dùng bắt đầu chơi, MenuState sẽ gọi:
//   StateManager::Get().ChangeState(std::make_unique<PlayState>());
// ============================================================
class MenuState : public IGameState {
public:
    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "MenuState"; }
};
