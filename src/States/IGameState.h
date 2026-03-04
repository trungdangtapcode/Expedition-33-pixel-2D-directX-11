#pragma once

// ============================================================
// IGameState - Interface (pure virtual) cho mọi trạng thái game
//
// Mọi State (MenuState, BattleState, CutsceneState...) đều kế thừa interface này.
// GameApp và StateManager KHÔNG cần biết bên trong State làm gì.
// Chúng chỉ gọi Update(dt) và Render() - đây là "Dependency Inversion Principle".
//
// VÒNG ĐỜI CỦA MỘT STATE:
//   OnEnter()  → được gọi 1 lần khi State được push vào stack
//   Update(dt) → được gọi mỗi frame khi State đang active
//   Render()   → được gọi mỗi frame để vẽ
//   OnExit()   → được gọi 1 lần khi State bị pop ra khỏi stack
// ============================================================
class IGameState {
public:
    virtual ~IGameState() = default;

    // Khởi tạo tài nguyên khi state được kích hoạt
    virtual void OnEnter() = 0;

    // Giải phóng tài nguyên khi state bị đóng
    virtual void OnExit() = 0;

    // Cập nhật logic game mỗi frame
    // dt = deltaTime (giây) - dùng để nhân với mọi chuyển động, animation
    virtual void Update(float dt) = 0;

    // Vẽ đồ họa mỗi frame
    virtual void Render() = 0;

    // (Optional) Tên state để debug/logging
    virtual const char* GetName() const = 0;
};
