#pragma once
#include "IGameState.h"

// ============================================================
// PlayState - State gameplay chính (In-Game)
//
// Đây là nơi sẽ chứa BattleSystem, EntityManager, CameraSystem...
// Hiện tại là placeholder - chỉ hiển thị màu nền khác để xác nhận
// State transition hoạt động đúng.
//
// Mở rộng sau:
//   void PlayState::Update(float dt) {
//       entityManager.UpdateAll(dt);
//       turnManager.Update(dt);
//   }
// ============================================================
class PlayState : public IGameState {
public:
    void OnEnter() override;
    void OnExit()  override;
    void Update(float dt) override;
    void Render()  override;
    const char* GetName() const override { return "PlayState"; }

private:
    float mRotation = 0.0f; // Biến test: xoay hình đơn giản
};
