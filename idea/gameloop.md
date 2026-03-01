Để xây dựng một Game Loop và hệ thống Quản lý Thời gian (Time) có thể scale (mở rộng) tốt, bạn cần hiểu rằng **Game Loop không nên biết chi tiết về game của bạn**. Nó chỉ nên đóng vai trò là một "nhịp tim", bơm máu (thời gian - `deltaTime`) đến các hệ thống khác.

Nếu bạn nhét thẳng logic đánh nhau, kiểm tra phím bấm vào vòng lặp chính, code sẽ nát bét chỉ sau 1 tuần.

Dưới đây là cách thiết kế Game Loop và Time chuẩn mực cho C++/DirectX (Win32), cùng các Design Pattern cần thiết.

### 1. Hệ thống Quản lý Thời gian (Time Management)

Trong Windows/DirectX, tuyệt đối **không** dùng hàm `clock()` hay `time()` của C++ chuẩn vì độ chính xác rất thấp. Bạn phải dùng **High-Resolution Timer** của Windows API thông qua 2 hàm: `QueryPerformanceFrequency` và `QueryPerformanceCounter`.

**Pattern áp dụng: Singleton (Tranh cãi nhưng hiệu quả)**
Biến `deltaTime` cần được truy cập ở mọi nơi (từ hệ thống vật lý, animation, đến QTE). Thay vì truyền tham số `deltaTime` qua hàng chục hàm lồng nhau, bạn nên biến class `Time` thành một Singleton hoặc có các static methods.

**Cấu trúc class `GameTimer`:**

```cpp
class GameTimer {
private:
    double secondsPerCount;
    __int64 deltaTime;
    __int64 baseTime;
    __int64 pausedTime;
    __int64 stopTime;
    __int64 prevTime;
    __int64 currTime;
    bool isStopped;

public:
    GameTimer();
    float TotalTime() const;  // Tổng thời gian game đã chạy (để tính toán Cutscene/Event)
    float DeltaTime() const;  // Thời gian giữa 2 frame (cực kỳ quan trọng)

    void Reset(); // Gọi trước khi vào game loop
    void Start(); // Khi unpause game
    void Stop();  // Khi pause game
    void Tick();  // Gọi ở mỗi frame của Game Loop
};

```

*Lưu ý: Trong hàm `Tick()`, bạn sẽ cập nhật `currTime`, tính `deltaTime = (currTime - prevTime) * secondsPerCount`, và gán `prevTime = currTime`.*

### 2. Thiết kế Game Loop (Vòng lặp Game)

Pattern quan trọng nhất ở đây chính là **Game Loop Pattern** kết hợp với **State Pattern** hoặc **Scene/Subsystem Pattern**.

Thay vì làm một vòng lặp nhồi nhét mọi thứ, class Game chính của bạn (ví dụ `GameApp`) chỉ nên gọi các Manager.

**Cấu trúc chuẩn của vòng lặp Win32:**

```cpp
int GameApp::Run() {
    MSG msg = {0};
    
    timer.Reset();

    // Vòng lặp vô tận cho đến khi tắt cửa sổ
    while(msg.message != WM_QUIT) {
        // 1. Ưu tiên xử lý thông điệp của Windows (Input, Resizing, v.v.)
        if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // 2. Nếu Windows rảnh, chạy Game Logic
        else {
            timer.Tick();

            if( !isAppPaused ) {
                CalculateFrameStats(); // (Tùy chọn) Tính FPS hiện lên thanh tiêu đề
                
                // Cập nhật Logic Game
                Update(timer.DeltaTime());
                
                // Vẽ Đồ họa
                Render();
            } else {
                Sleep(100); // Tránh ăn CPU khi game đang bị minimize hoặc pause
            }
        }
    }
    return (int)msg.wParam;
}

```

### 3. Cấu trúc để "Dễ Scale" về sau (Các Pattern bổ trợ)

Để hàm `Update(float dt)` và `Render()` ở trên không phình to thành hàng ngàn dòng code khi bạn thêm QTE, hội thoại, đánh theo lượt..., bạn **bắt buộc** phải sử dụng các Pattern sau:

**A. State Pattern (Máy trạng thái cho Game)**
Game Loop không cập nhật nhân vật. Nó cập nhật Trạng thái (State) hiện tại.

```cpp
void GameApp::Update(float dt) {
    // Game Loop chỉ cần 1 dòng này! Nó không cần biết game đang ở màn hình nào.
    StateManager::GetInstance()->GetCurrentState()->Update(dt);
}

void GameApp::Render() {
    // Tương tự cho render
    StateManager::GetInstance()->GetCurrentState()->Render();
}

```

* Khi bạn ở menu: `CurrentState` là `MenuState`. MenuState sẽ tự cập nhật UI.
* Khi chiến đấu: `CurrentState` là `BattleState`. BattleState sẽ cập nhật Turn, Action Queue, QTE.

**B. Entity-Component-System (ECS) hoặc Hệ thống Quản lý Đối tượng**
Bên trong `BattleState->Update(dt)`, thay vì bạn gọi thủ công: `Player1.Update()`, `Player2.Update()`, `Boss.Update()`, bạn sẽ có một class `EntityManager` chứa một List (danh sách) các đối tượng.

```cpp
void BattleState::Update(float dt) {
    // Cập nhật tất cả nhân vật, quái vật, hiệu ứng trên sân
    entityManager.UpdateAll(dt); 
    
    // Cấp nhật hệ thống quản lý đánh theo lượt
    turnManager.Update(dt);
}

```

**C. Observer Pattern (Hệ thống Event - Cực kỳ quan trọng cho dự án của bạn)**
Vì bạn có **In-combat events** (đang đánh nhau thì kích hoạt hội thoại). Đừng để code đánh nhau gọi thẳng hàm chiếu Cutscene.

* Khi Boss còn 50% máu, nó sẽ "hét" lên hệ thống: `EventManager::Broadcast("Boss_Half_Health")`.
* Game Loop hoặc State Manager lắng nghe event này, nó lập tức `Pause` cái `BattleState` lại, đẩy một `CutsceneState` lên trên cùng để chạy logic hội thoại lồng tiếng. Chạy xong thì xóa `CutsceneState`, nhường quyền Update lại cho `BattleState`.

### Tóm lại:

Bạn tạo một class `GameTimer` dùng `QueryPerformanceCounter` để có `deltaTime` chuẩn. Game Loop chính nằm ở Win32 `PeekMessage` sẽ gọi `timer.Tick()`, sau đó lấy `deltaTime` nhét vào hàm `Update(dt)`. Hàm `Update` này sẽ chuyển tiếp `dt` cho các hệ thống bên dưới theo dạng cây (Game -> StateManager -> CurrentState -> EntityManager -> Các Object cụ thể).

Đây là kiến trúc nền tảng của mọi Game Engine chuyên nghiệp, đảm bảo bạn thêm hàng trăm skill hay event thì cấu trúc lõi vẫn không bao giờ bị phá vỡ.