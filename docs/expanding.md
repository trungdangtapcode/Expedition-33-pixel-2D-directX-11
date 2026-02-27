# Mở Rộng Dự Án

Hướng dẫn thực tiễn để thêm tính năng mới mà **không phá vỡ** cấu trúc hiện tại.

---

## 1. Thêm State mới (BattleState)

### Bước 1 — Tạo file header

```cpp
// src/States/BattleState.h
#pragma once
#include "IGameState.h"
#include "../Events/EventManager.h"

class BattleState : public IGameState {
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    const char* GetName() const override { return "BattleState"; }

private:
    ListenerID mEnemyDeadListener = -1;
    float mBattleTimer = 0.0f;
};
```

### Bước 2 — Implement

```cpp
// src/States/BattleState.cpp
#include "BattleState.h"
#include "StateManager.h"
#include "PlayState.h"

void BattleState::OnEnter() {
    OutputDebugStringA("[BattleState] Entered\n");

    // Đăng ký lắng nghe event "enemy_dead"
    mEnemyDeadListener = EventManager::Get().Subscribe("enemy_dead",
        [this](const EventData& data) {
            OutputDebugStringA("[BattleState] Enemy defeated!\n");
            StateManager::Get().PopState(); // Quay lại PlayState
        });
}

void BattleState::OnExit() {
    EventManager::Get().Unsubscribe("enemy_dead", mEnemyDeadListener);
    OutputDebugStringA("[BattleState] Exited\n");
}

void BattleState::Update(float dt) {
    mBattleTimer += dt;

    // Giả lập: sau 5 giây thì địch chết
    if (mBattleTimer > 5.0f) {
        EventManager::Get().Broadcast("enemy_dead", { .name = "Goblin" });
    }
}

void BattleState::Render() {
    // TODO: vẽ enemy sprites, HP bar, action menu...
}
```

### Bước 3 — Kích hoạt từ PlayState

```cpp
// Trong PlayState::Update:
if (GetAsyncKeyState('B') & 0x8000) {
    StateManager::Get().PushState(std::make_unique<BattleState>());
}
```

> **Không cần sửa bất kỳ file nào khác.** GameApp và StateManager hoàn toàn không biết BattleState tồn tại cho đến khi nó được push lên stack.

---

## 2. Thêm Entity Component System (ECS) cơ bản

Folder `src/ECS/` đã được tạo sẵn. ECS giải quyết vấn đề: khi game có hàng trăm "đối tượng" với các thuộc tính khác nhau, **kế thừa sâu** (`Enemy extends Character extends Entity`) trở nên rất khó quản lý.

### Khái niệm ECS

```
Entity   = ID đơn thuần (ví dụ: uint32_t id = 42)
Component = Dữ liệu thuần túy (ví dụ: struct Position { float x, y; })
System   = Logic xử lý trên tập Component (ví dụ: MoveSystem cập nhật Position)
```

### Triển khai tối giản

```cpp
// src/ECS/Components.h
#pragma once
#include <DirectXMath.h>

struct TransformComponent {
    DirectX::XMFLOAT3 position = { 0, 0, 0 };
    DirectX::XMFLOAT3 rotation = { 0, 0, 0 };
    DirectX::XMFLOAT3 scale    = { 1, 1, 1 };
};

struct HealthComponent {
    float current = 100.0f;
    float maximum = 100.0f;
    bool  isAlive = true;
};

struct VelocityComponent {
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
};
```

```cpp
// src/ECS/EntityManager.h
#pragma once
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <any>

using EntityID = uint32_t;

class EntityManager {
public:
    EntityID CreateEntity() { return mNextID++; }

    template<typename T>
    void AddComponent(EntityID id, T component) {
        mComponents[typeid(T)][id] = std::move(component);
    }

    template<typename T>
    T* GetComponent(EntityID id) {
        auto& bucket = mComponents[typeid(T)];
        auto it = bucket.find(id);
        if (it != bucket.end()) return std::any_cast<T>(&it->second);
        return nullptr;
    }

    template<typename T>
    void ForEach(std::function<void(EntityID, T&)> fn) {
        for (auto& [id, val] : mComponents[typeid(T)]) {
            fn(id, *std::any_cast<T>(&val));
        }
    }

private:
    EntityID mNextID = 1;
    std::unordered_map<std::type_index,
        std::unordered_map<EntityID, std::any>> mComponents;
};
```

### Sử dụng trong PlayState

```cpp
// src/States/PlayState.h
#include "../ECS/EntityManager.h"
#include "../ECS/Components.h"

class PlayState : public IGameState {
    EntityManager mEntities;
    EntityID mPlayer;
    EntityID mEnemy;
};

// PlayState.cpp
void PlayState::OnEnter() {
    mPlayer = mEntities.CreateEntity();
    mEntities.AddComponent(mPlayer, TransformComponent{ {0, 0, 0} });
    mEntities.AddComponent(mPlayer, HealthComponent{ 100, 100 });
    mEntities.AddComponent(mPlayer, VelocityComponent{});

    mEnemy = mEntities.CreateEntity();
    mEntities.AddComponent(mEnemy, TransformComponent{ {5, 0, 0} });
    mEntities.AddComponent(mEnemy, HealthComponent{ 50, 50 });
}

void PlayState::Update(float dt) {
    // System: cập nhật tất cả entity có Velocity
    mEntities.ForEach<VelocityComponent>([&](EntityID id, VelocityComponent& vel) {
        if (auto* t = mEntities.GetComponent<TransformComponent>(id)) {
            t->position.x += vel.velocity.x * dt;
            t->position.y += vel.velocity.y * dt;
        }
    });
}
```

---

## 3. Thêm sự kiện mới

### Ví dụ: "player_level_up"

```cpp
// Bước 1: Broadcast từ nơi xảy ra sự kiện (RPGSystem, LevelManager...)
EventData data;
data.name    = "player_level_up";
data.payload = "Maelle";  // Tên nhân vật
data.value   = 15;        // Level mới
EventManager::Get().Broadcast("player_level_up", data);

// Bước 2: Subscribe tại bất kỳ system nào quan tâm
// (trong UISystem::Initialize)
EventManager::Get().Subscribe("player_level_up", [](const EventData& d) {
    // Hiển thị "LEVEL UP!" animation
    // d.payload = "Maelle", d.value = 15
});

// (trong AudioSystem::Initialize)
EventManager::Get().Subscribe("player_level_up", [](const EventData& d) {
    PlaySFX("level_up_fanfare.wav");
});
```

Không cần sửa `EventManager` hay bất kỳ hệ thống nào khác.

---

## 4. Thêm CutsceneState (Push trên BattleState)

Trong Battle, khi boss còn 50% HP, muốn pause battle và chạy cutscene:

```cpp
// BattleState.cpp
void BattleState::Update(float dt) {
    if (boss.hp < boss.maxHp * 0.5f && !mHalfHpCutscenePlayed) {
        mHalfHpCutscenePlayed = true;

        // Push CutsceneState lên trên BattleState
        // BattleState vẫn nằm trong stack, không bị OnExit()
        StateManager::Get().PushState(
            std::make_unique<CutsceneState>("boss_intro_cutscene")
        );
    }
}
```

Stack sau khi push:
```
[CutsceneState]  ← Đang chạy
[BattleState]    ← Đang chờ (OnExit chưa được gọi)
[PlayState]      ← Đang chờ
```

Khi CutsceneState kết thúc:
```cpp
// CutsceneState.cpp
void CutsceneState::Update(float dt) {
    if (mCutscene.IsFinished()) {
        StateManager::Get().PopState(); // CutsceneState tự đóng
        // BattleState::OnEnter() KHÔNG được gọi lại — game tiếp tục
    }
}
```

---

## 5. Checklist mở rộng

Khi thêm một hệ thống mới, đảm bảo:

- [ ] Tạo file trong đúng folder (`src/States/`, `src/ECS/`, `src/Systems/`...)
- [ ] Kế thừa đúng interface (`IGameState` cho State, v.v.)
- [ ] Gọi `Unsubscribe()` trong `OnExit()` nếu có Subscribe
- [ ] Dùng `ComPtr` nếu cần DirectX COM object
- [ ] Dùng `unique_ptr` nếu cần quản lý heap object
- [ ] Thêm file `.cpp` vào `build_src.bat` trong danh sách source files
