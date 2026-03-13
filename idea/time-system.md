Để xây dựng một hệ thống thời gian (Time System) trong một game engine lớn, đặc biệt là khi muốn hỗ trợ các tính năng như **Slow-motion (làm chậm thời gian)**, **Pause (tạm dừng)**, hoặc **Fast-forward (tua nhanh)** mà không làm hỏng logic của UI hay Âm thanh, bạn **tuyệt đối không được dùng một biến `deltaTime` toàn cục (Global Variable/Singleton)**.

Nếu dùng một biến toàn cục, khi bạn giảm `deltaTime` để làm slow-motion, thanh máu UI của bạn cũng sẽ mờ đi một cách chậm chạp, và game menu cũng sẽ bị lag theo.

Dưới đây là kiến trúc chuẩn mực (Architecture & Patterns) được sử dụng trong các AAA Engine để giải quyết bài toán này:

### 1. Core Pattern: Hierarchical Clocks (Đồng hồ phân cấp - Ứng dụng Composite Pattern)

Thay vì có một khái niệm "Thời gian" duy nhất, bạn tạo ra một class `Clock` (hoặc `Timer`). Các Clock này được tổ chức theo dạng cây (Tree/Hierarchy). Mỗi Clock sẽ có một hệ số nhân `TimeScale` (mặc định = 1.0).

**Cấu trúc cây Clock:**

* **Root Clock (Hardware/OS Clock):** Luôn chạy theo thời gian thực (TimeScale = 1.0). Không bao giờ được can thiệp.
* **UI/System Clock (Child của Root):** Dùng cho Menu, Animations của UI. Thường giữ TimeScale = 1.0 ngay cả khi game đang pause.
* **Gameplay Clock (Child của Root):** Đây là nơi phép thuật xảy ra. Dùng cho toàn bộ thế giới game.
* **Player Clock (Child của Gameplay):** Dùng riêng cho nhân vật chính.
* **Environment/Enemy Clock (Child của Gameplay):** Dùng cho quái vật, vật lý, thời tiết.





**Cơ chế hoạt động:**
Khi `Clock` cha cập nhật, nó sẽ tính `DeltaTime` của nó = `RealDeltaTime * TimeScale`. Sau đó, nó truyền `DeltaTime` này xuống cho các `Clock` con. Các con lại tiếp tục nhân với `TimeScale` riêng của chúng.

**Lợi ích cho Slow-Motion:**

* **Global Slow-mo (Bullet Time như Max Payne):** Bạn chỉ cần set `GameplayClock->SetTimeScale(0.2f)`. Toàn bộ game sẽ chậm lại 5 lần, nhưng UI Menu (dùng UI Clock) vẫn hoạt động mượt mà ở tốc độ bình thường.
* **Local Slow-mo (Skill làm chậm của pháp sư):** Nếu quái vật trúng bùa, bạn chỉ cần giảm `TimeScale` trong Clock hoặc Component của quái vật đó. Mọi thứ khác vẫn bình thường.
* **Super Hot effect (Chỉ khi di chuyển thời gian mới trôi):** Bind tốc độ di chuyển của Player vào `TimeScale` của `GameplayClock`.

### 2. Dependency Injection / Context Passing (Thay thế Singleton)

Làm sao để các hệ thống (Systems) lấy được đúng Clock nó cần? Đừng dùng `TimeManager::GetInstance().GetDeltaTime()`.

Trong kiến trúc ECS (Entity-Component-System) hoặc Game Loop hiện đại, bạn hãy **truyền Clock như một Context (Tham số)** vào hàm Update.

```cpp
// Interface của một System trong engine
class ISystem {
public:
    virtual void Update(const Clock& contextClock) = 0;
};

// Khi Main Loop chạy:
uiSystem->Update(uiClock);
physicsSystem->Update(gameplayClock);
aiSystem->Update(enemyClock);

```

Cách này giúp code cực kỳ dễ test (bạn có thể mock một Clock chạy cực nhanh để test AI) và maintain rất dễ vì biết chính xác hệ thống nào đang xài dòng thời gian nào.

### 3. Vấn đề chí mạng: Tách biệt "Variable Update" và "Fixed Update"

Khi bạn làm slow-motion, có một rủi ro cực lớn: **Hệ thống Vật lý (Physics Engine) sẽ bị lỗi xuyên tường (tunneling) hoặc tính toán sai.** Vật lý (như Box2D, Havok, PhysX) yêu cầu một bước nhảy thời gian cố định (Fixed Time Step - ví dụ: luôn là 0.016s). Nếu bạn làm slow-mo bằng cách truyền một cái `deltaTime` nhỏ xíu (ví dụ 0.002s) vào Physics Engine, nó sẽ sinh ra sai số dấu phẩy động (float precision) và game sẽ hỏng.

**Giải pháp (Accumulator Pattern - Tích lũy thời gian):**
Bạn phải áp dụng mô hình "Fix Your Timestep" nổi tiếng.

```cpp
// Mô hình logic cơ bản bên trong Game Loop
void GameplayLoop(float rawDeltaTime) 
{
    float scaledDeltaTime = rawDeltaTime * GameplayClock.TimeScale; // Slow-mo áp dụng ở đây
    
    // 1. Cập nhật Logic/Đồ họa (Variable Update)
    GraphicsSystem.Update(scaledDeltaTime);
    InputSystem.Update(scaledDeltaTime);

    // 2. Cập nhật Vật lý (Fixed Update)
    PhysicsAccumulator += scaledDeltaTime;
    
    while (PhysicsAccumulator >= FIXED_TIMESTEP) 
    {
        PhysicsSystem.Step(FIXED_TIMESTEP); // Vật lý luôn chạy ở 1 step cố định, không quan tâm slow-mo
        PhysicsAccumulator -= FIXED_TIMESTEP;
    }
}

```

Khi có Slow-motion (`scaledDeltaTime` rất nhỏ), `PhysicsAccumulator` sẽ tăng lên rất chậm. Kết quả là vòng `while` bên trong sẽ ít được gọi hơn (có thể vài frame mới gọi 1 lần). Game vẫn chậm lại đúng như thiết kế, nhưng **Vật lý vẫn tính toán với độ chính xác tuyệt đối** vì nó luôn nhận hằng số `FIXED_TIMESTEP`.

### 4. Interpolation (Nội suy) chống giật hình

Khi slow-motion kết hợp với Fixed Update ở trên, game sẽ bị giật cục (stutter) vì Render chạy nhiều frame hơn Physics.

* **Pattern giải quyết:** Bạn phải lưu State của Entity ở 2 thời điểm: `PreviousState` và `CurrentState`.
* Khi Render trong trạng thái slow-mo, thay vì vẽ vị trí hiện tại, bạn dùng toán học **Lerp (Linear Interpolation)** để nội suy vị trí của model 3D giữa `PreviousState` và `CurrentState` dựa trên phần thời gian dư trong `PhysicsAccumulator`. Điều này đảm bảo slow-motion mượt như bơ (butter smooth) dù frame rate đồ họa là 144Hz mà logic chỉ chạy ở 60Hz.