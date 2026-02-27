# Design Patterns

Tài liệu này giải thích **tại sao** và **cách** mỗi Design Pattern được áp dụng trong project.

---

## 1. Singleton Pattern

**Áp dụng tại:** `StateManager`, `EventManager`, `D3DContext`

### Vấn đề

Một số hệ thống cần được truy cập từ **khắp nơi** trong codebase, nhưng chỉ được tồn tại **một instance duy nhất**:

- `D3DContext` — chỉ có 1 GPU, 1 swap chain.
- `StateManager` — chỉ có 1 stack state tại 1 thời điểm.
- `EventManager` — chỉ có 1 hệ thống event toàn cục.

### Giải pháp: Meyers' Singleton (C++11)

```cpp
static StateManager& Get() {
    static StateManager instance; // Khởi tạo 1 lần duy nhất, thread-safe từ C++11
    return instance;
}

// Xóa copy constructor và copy assignment để không ai tạo được bản sao
StateManager(const StateManager&) = delete;
StateManager& operator=(const StateManager&) = delete;
```

### Cách dùng

```cpp
// Từ bất kỳ đâu trong code:
StateManager::Get().PushState(...);
EventManager::Get().Broadcast("boss_died");
auto* device = D3DContext::Get().GetDevice();
```

### Khi nào KHÔNG nên dùng Singleton

- Khi cần nhiều instances (ví dụ: nhiều cửa sổ, nhiều renderer).
- Khi cần unit test riêng biệt (Singleton khó mock).
- Thay thế: **Dependency Injection** — truyền dependency qua constructor.

---

## 2. State Pattern (Stack-based State Machine)

**Áp dụng tại:** `StateManager`, `IGameState`, `MenuState`, `PlayState`

### Vấn đề

Một game có nhiều "màn hình" với logic hoàn toàn khác nhau:
`MainMenu` → `Loading` → `Gameplay` → `PauseMenu` → `BattleState` → `CutsceneState`

Nếu dùng `if/else` hay `enum`:
```cpp
// ❌ SAI — Game Loop biết quá nhiều thứ, không thể mở rộng
void Update(float dt) {
    if (state == MENU)    UpdateMenu(dt);
    if (state == PLAY)    UpdatePlay(dt);
    if (state == BATTLE)  UpdateBattle(dt);
    // Thêm state mới → phải sửa hàm này!
}
```

### Giải pháp: Interface + Stack

**Interface `IGameState`** — hợp đồng mà mọi State phải thực hiện:

```cpp
class IGameState {
public:
    virtual void OnEnter() = 0;  // Gọi khi state được kích hoạt
    virtual void OnExit()  = 0;  // Gọi khi state bị đóng
    virtual void Update(float dt) = 0;
    virtual void Render() = 0;
};
```

**Game Loop không cần biết gì về logic cụ thể:**

```cpp
// ✅ ĐÚNG — Game Loop chỉ delegate, không biết đang ở state nào
void GameApp::Update(float dt) {
    StateManager::Get().Update(dt);
}
```

### Tại sao dùng Stack thay vì con trỏ đơn?

```
Stack biểu diễn lịch sử màn hình:

[CutsceneState]  ← Đang active (đỉnh stack)
[BattleState]    ← Đang chờ, bị "pause"
[PlayState]      ← Đang chờ

→ Pop CutsceneState → BattleState tiếp tục ngay lập tức
```

**Các operation:**

| Hàm | Dùng khi | Ví dụ |
|---|---|---|
| `PushState(state)` | Mở state mới, giữ state cũ | Mở PauseMenu trong BattleState |
| `PopState()` | Đóng state hiện tại, quay lại | Đóng PauseMenu |
| `ChangeState(state)` | Chuyển cảnh không cần quay lại | MainMenu → InGame |

### Vòng đời của một State

```
      PushState()          PopState() / ChangeState()
          ↓                          ↓
      OnEnter()                   OnExit()
          ↓                          ↑
      Update(dt) ←──── (mỗi frame) ──┘
      Render()
```

---

## 3. Observer Pattern (Event System)

**Áp dụng tại:** `EventManager`

### Vấn đề: Tight Coupling

Giả sử khi Boss còn 50% HP, game cần kích hoạt cutscene:

```cpp
// ❌ SAI — BattleState biết quá nhiều về CutsceneSystem
void BattleState::Update(float dt) {
    if (boss.hp < boss.maxHp * 0.5f && !halfHpTriggered) {
        CutsceneSystem::Get().Play("boss_intro"); // tight coupling!
        UISystem::Get().ShowBossWarning();         // tight coupling!
        AudioSystem::Get().PlayStinger("danger");  // tight coupling!
        halfHpTriggered = true;
    }
}
```

Vấn đề: `BattleState` phải biết về `CutsceneSystem`, `UISystem`, `AudioSystem` — **không thể tái sử dụng** hoặc **test độc lập**.

### Giải pháp: Publish/Subscribe

```cpp
// BattleState chỉ "hét lên" (broadcast), không quan tâm ai lắng nghe:
void BattleState::Update(float dt) {
    if (boss.hp < boss.maxHp * 0.5f && !halfHpTriggered) {
        EventManager::Get().Broadcast("boss_half_health", { .value = boss.hp });
        halfHpTriggered = true;
    }
}

// Mỗi hệ thống tự đăng ký và tự xử lý:
// (trong CutsceneSystem::Initialize)
EventManager::Get().Subscribe("boss_half_health", [this](const EventData& d) {
    Play("boss_intro");
});

// (trong UISystem::Initialize)
EventManager::Get().Subscribe("boss_half_health", [this](const EventData& d) {
    ShowBossWarning();
});
```

### Cơ chế bên trong

```cpp
// EventManager lưu listeners theo tên event:
std::unordered_map<std::string, std::vector<Listener>> mListeners;

// Broadcast duyệt qua danh sách và gọi từng callback:
void Broadcast(const std::string& eventName, const EventData& data) {
    auto listenersCopy = mListeners[eventName]; // Bản sao để tránh invalidation
    for (const auto& listener : listenersCopy) {
        listener.callback(data);
    }
}
```

### Quy tắc quan trọng: Unsubscribe trong OnExit

```cpp
// ❌ NGUY HIỂM: State bị xóa nhưng lambda vẫn giữ con trỏ this!
// Nếu event được broadcast sau khi State đã bị pop → crash!

// ✅ ĐÚNG: Lưu ID và hủy đăng ký khi State kết thúc
class BattleState : public IGameState {
    ListenerID mBossListenerID;

    void OnEnter() override {
        mBossListenerID = EventManager::Get().Subscribe("boss_half_health",
            [this](const EventData& d) { TriggerCutscene(); });
    }

    void OnExit() override {
        EventManager::Get().Unsubscribe("boss_half_health", mBossListenerID);
    }
};
```

---

## 4. Facade Pattern

**Áp dụng tại:** `D3DContext`

### Vấn đề

Khởi tạo DirectX 11 yêu cầu ~50 dòng code: tạo Device, SwapChain, RenderTargetView, DepthStencilView, Viewport, xử lý HRESULT... Nếu trải thẳng ra khắp nơi thì mỗi State đều phải biết tất cả chi tiết này.

### Giải pháp: Facade — giao diện đơn giản che phức tạp

```
Bên ngoài nhìn thấy:           Bên trong thực sự:
┌─────────────────┐            ┌──────────────────────────────────┐
│  D3DContext      │            │  IDXGISwapChain                   │
│                  │ ────────>  │  ID3D11Device                     │
│  Initialize()    │            │  ID3D11DeviceContext               │
│  BeginFrame()    │            │  ID3D11RenderTargetView            │
│  EndFrame()      │            │  ID3D11DepthStencilView            │
│  GetDevice()     │            │  D3D11_VIEWPORT                   │
└─────────────────┘            │  DXGI_SWAP_CHAIN_DESC              │
                                │  HRESULT checks...                 │
                                └──────────────────────────────────┘
```

State không cần biết DirectX tồn tại. Chúng chỉ cần:

```cpp
auto* device  = D3DContext::Get().GetDevice();
auto* context = D3DContext::Get().GetContext();
```

---

## 5. Resource Acquisition Is Initialization (RAII)

**Áp dụng tại:** `D3DContext` với `Microsoft::WRL::ComPtr`, `StateManager` với `std::unique_ptr`

### Vấn đề

DirectX dùng **COM (Component Object Model)** — mọi object đều phải gọi `Release()` thủ công khi không dùng nữa. Quên `Release()` → memory leak, quên tắt fullscreen trước khi release → crash.

### Giải pháp: Smart Pointer

**`ComPtr<T>`** — smart pointer cho COM objects:

```cpp
// ❌ SAI — dễ quên Release(), dễ leak khi exception
ID3D11Device* device = nullptr;
D3D11CreateDevice(..., &device, ...);
// ... nếu throw exception ở đây → device không bao giờ được Release!
device->Release();

// ✅ ĐÚNG — tự động Release() khi ra khỏi scope
Microsoft::WRL::ComPtr<ID3D11Device> device;
D3D11CreateDevice(..., device.GetAddressOf(), ...);
// Khi D3DContext bị destroy → ComPtr destructor tự gọi Release()
```

**`unique_ptr<IGameState>`** — đảm bảo State tự giải phóng:

```cpp
// Stack lưu unique_ptr — khi pop() được gọi:
// 1. unique_ptr bị destroy
// 2. Destructor của State được gọi tự động
std::stack<std::unique_ptr<IGameState>> mStates;
```

### Nguyên tắc RAII

> **"Tài nguyên được cấp phát trong constructor, giải phóng trong destructor."**
> Scope của object = lifetime của resource.

---

## 6. Dependency Inversion Principle (DIP)

**Áp dụng tại:** `IGameState` interface

### Định nghĩa

> "Module cấp cao không phụ thuộc vào module cấp thấp. Cả hai phụ thuộc vào abstraction."

### Áp dụng trong project

```
               ┌──────────────┐
               │  GameApp     │  (Module cấp cao)
               │  (High-level)│
               └──────┬───────┘
                      │ depends on abstraction
                      ▼
               ┌──────────────┐
               │  IGameState  │  (Abstraction / Interface)
               └──────┬───────┘
                      │ implemented by
          ┌───────────┼───────────┐
          ▼           ▼           ▼
     MenuState   PlayState   BattleState   (Module cấp thấp)
```

`GameApp` chỉ biết về `IGameState` — không bao giờ biết `MenuState` hay `BattleState` tồn tại (ngoại trừ khi tạo chúng lần đầu trong `Initialize`). Khi thêm `CutsceneState`, `BattleState`... không cần sửa `GameApp` một dòng nào.

---

## 7. Single Responsibility Principle (SRP)

**Áp dụng toàn bộ kiến trúc**

Mỗi class chỉ có **một lý do để thay đổi**:

| Class | Trách nhiệm duy nhất |
|---|---|
| `GameTimer` | Đo thời gian chính xác |
| `D3DContext` | Quản lý tài nguyên DirectX 11 |
| `GameApp` | Game loop + cửa sổ Win32 |
| `StateManager` | Quản lý stack các State |
| `EventManager` | Định tuyến sự kiện giữa các hệ thống |
| `MenuState` | Logic và render của màn hình chính |
| `PlayState` | Logic và render gameplay |

Nếu cần thay đổi cách render (từ DirectX 11 sang DirectX 12), chỉ cần sửa `D3DContext` — không một class nào khác bị ảnh hưởng.
