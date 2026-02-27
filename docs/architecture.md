# Kiến trúc tổng quan

Tài liệu này mô tả toàn bộ kiến trúc của project, bao gồm các **Design Pattern**,
**Principle**, và **cấu trúc thư mục** đã được áp dụng.

---

## Cấu trúc thư mục

```
src/
├── main.cpp              ← Entry point duy nhất (WinMain)
├── Core/
│   ├── GameApp           ← Vòng lặp chính, quản lý cửa sổ
│   └── GameTimer         ← High-resolution timer
├── Renderer/
│   └── D3DContext        ← Wrapper toàn bộ DirectX 11
├── States/
│   ├── IGameState        ← Interface chuẩn cho mọi State
│   ├── StateManager      ← Quản lý stack State
│   ├── MenuState         ← State màn hình chính
│   └── PlayState         ← State gameplay
├── Events/
│   └── EventManager      ← Hệ thống sự kiện toàn cục
├── ECS/                  ← (Chờ mở rộng) Entity-Component-System
├── Platform/             ← (Chờ mở rộng) Input, Window platform layer
└── Utils/                ← (Chờ mở rộng) Helpers, Math wrappers
```

---

## Luồng chạy tổng quát

```
WinMain (main.cpp)
  └── GameApp::Initialize()
        ├── InitWindow()           → Tạo cửa sổ Win32
        ├── D3DContext::Initialize() → Khởi tạo DirectX 11
        └── StateManager::PushState(MenuState)

  └── GameApp::Run()  ← Game Loop
        ├── PeekMessage()          → Xử lý sự kiện Windows
        ├── GameTimer::Tick()      → Cập nhật deltaTime
        ├── StateManager::Update(dt)  → Delegate xuống State hiện tại
        └── StateManager::Render()    → Delegate xuống State hiện tại
```

Xem chi tiết từng phần trong các file docs khác trong thư mục này.
