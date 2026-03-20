Để thiết kế một hệ thống Agility (Tốc độ/Nhanh nhẹn) linh hoạt, chuẩn mực và có thể dễ dàng mở rộng cho game turn-based hiện đại, kiến trúc tốt nhất là sử dụng hệ thống **Tick-based** kết hợp với khái niệm **Action Value (Giá trị Hành động - AV)**.

Hệ thống này bỏ qua khái niệm "hiệp đấu" (round) cứng nhắc. Thay vào đó, nó biến trận đấu thành một đường đua liên tục.

Dưới đây là thiết kế chi tiết toàn bộ hệ thống từ công thức toán học đến luồng xử lý code:

### 1. Khái niệm cốt lõi: Đường đua và Toán học

Hãy tưởng tượng mỗi nhân vật đang chạy trên một đường đua vô hình. Khi chạm vạch đích, nhân vật đó sẽ có lượt.
* **Action Gauge (Khoảng cách đường đua - Hằng số):** Một con số cố định mà mọi nhân vật phải vượt qua để có lượt. Thường đặt là `10,000` để tránh số thập phân quá nhỏ.
* **Agility (Vận tốc):** Chỉ số tốc độ hiện tại của nhân vật.
* **Action Value - AV (Thời gian chờ):** Khoảng "thời gian" (hay số Tick) cần thiết để nhân vật chạy tới đích. AV càng nhỏ, nhân vật càng sớm đến lượt.

**Công thức gốc:**
$$Action Value (AV) = \frac{Action Gauge}{Agility}$$

*Ví dụ:* Nếu Khoảng cách là 10,000.
* Nhân vật A (Agility 100) -> Cần 100 AV (Tick) để đến lượt.
* Quái vật B (Agility 80) -> Cần 125 AV (Tick) để đến lượt.
* Nhân vật C (Agility 200) -> Cần 50 AV (Tick) để đến lượt.

### 2. Cấu trúc dữ liệu (Data Structure)

Bạn cần quản lý danh sách các nhân vật và thời gian chờ của họ. Thay vì dùng `std::queue` (First-In-First-Out) hay `std::priority_queue` (khó can thiệp dữ liệu ở giữa hàng), cách tối ưu nhất là dùng một mảng động (`std::vector` hoặc `List`) và liên tục sắp xếp nó.

```cpp
// Dữ liệu lượt của 1 thực thể trên đường đua
struct TurnNode {
    int entityID;      // ID của nhân vật/quái vật
    float currentAV;   // Action Value còn lại để tới đích (càng nhỏ càng tốt)
    float baseAgility; // Lưu lại Agility lúc bắt đầu tính để xử lý buff/debuff
};

// Hàng đợi chính của trận đấu
class TurnQueueSystem {
private:
    std::vector<TurnNode> timeline;
    const float ACTION_GAUGE = 10000.0f;
    
public:
    // ... các hàm xử lý
};
```

### 3. Vòng lặp chiến đấu (The Battle Loop)

Luồng hoạt động chính để sinh ra Queue và vận hành các turn diễn ra theo các bước sau:

**Bước 1: Khởi tạo trận đấu (Roll Initiative)**
Khi bắt đầu, tính AV ban đầu cho tất cả nhân vật có mặt trên sân dựa theo Agility của họ và đẩy vào `timeline`.
Sắp xếp `timeline` theo chiều tăng dần của `currentAV` (Nhân vật có AV nhỏ nhất đứng vị trí số 0).

**Bước 2: Tịnh tiến thời gian (Time Advance)**
Nhân vật ở vị trí `timeline[0]` sẽ là người đi trước. Để hệ thống trôi chảy, bạn tịnh tiến thời gian của toàn bộ đấu trường bằng chính số AV của người đứng đầu.

*Ví dụ:* `timeline` đang là `[A: 50, B: 100, C: 125]`.
Hệ thống lấy 50 tick. Trừ 50 tick cho tất cả mọi người.
Kết quả: `[A: 0, B: 50, C: 75]`. 
Lúc này `A` chạm vạch đích (`AV == 0`).

**Bước 3: Thực thi hành động (Action Execution)**
Cho phép `A` chọn kỹ năng và tấn công. Trận đấu tạm dừng chờ người chơi thao tác.

**Bước 4: Trả về vạch xuất phát (Reset / Push)**
Sau khi `A` đánh xong, `A` bị đưa lại về vạch xuất phát. 
Hệ thống tính toán lại AV mới cho `A` = `10,000 / Agility hiện tại của A`.
Gán AV mới này cho `A` và sắp xếp (Sort) lại toàn bộ `timeline` từ bé đến lớn.
Quay lại **Bước 2**.

### 4. Xử lý các cơ chế nâng cao (Thao túng Hàng đợi)

Linh hồn của hệ thống này nằm ở việc nó dễ dàng xử lý các hiệu ứng thao túng lượt cực kỳ phức tạp một cách "sạch sẽ":

#### A. Kéo lượt / Đẩy lượt (Action Advance / Delay)
Một số kỹ năng trực tiếp tác động vào thanh hành động mà không đổi chỉ số Agility. (Ví dụ: Đánh choáng đẩy lùi quái vật 20%, hoặc Hỗ trợ kéo đồng đội lên đánh ngay lập tức).
* **Logic:** Bạn chỉ cần tác động trực tiếp vào `currentAV` của mục tiêu trong `timeline` rồi sắp xếp lại.
* *Đẩy lùi 20%: Mục tiêu bị cộng thêm một lượng `AV = (10,000 / Agility) * 0.2`.
* *Ưu tiên đánh ngay 100%:* Gán thẳng `currentAV = 0` cho mục tiêu.

#### B. Buff / Debuff Agility giữa hiệp đấu
Nếu Quái vật B đang có Agility là 80, cần 125 AV để tới đích. Nó đã chạy được một lúc và đang còn lại 50 AV. Bất ngờ, nó bị trúng bùa "Làm chậm 50%" (Agility giảm còn 40). Chuyện gì xảy ra?

Thời gian chờ còn lại của nó phải bị **kéo dài ra** tương ứng với tỷ lệ tốc độ bị mất.
* **Công thức quy đổi:** $New\_AV = Old\_AV \times (\frac{Old\_Agility}{New\_Agility})$
* *Áp dụng:* $New\_AV = 50 \times (\frac{80}{40}) = 100$.
* Quái vật B từ chỗ chỉ còn chờ 50 tick, nay bị đẩy ra xa thành 100 tick. Bạn cập nhật lại `currentAV = 100`, cập nhật `baseAgility = 40`, và gọi hàm Sort lại `timeline`. Hàng đợi sẽ lập tức thay đổi thứ tự một cách cực kỳ chính xác và logic.

### 5. Sinh Queue cho UI (Dự đoán tương lai)

Trong game, UI thường hiển thị danh sách 5-10 lượt tiếp theo ở góc màn hình. Làm sao để vẽ được UI này nếu một nhân vật đánh xong mới tính toán lại lượt?

**Cách làm:** Bạn tạo ra một hàm "Mô phỏng" (Simulate). 
Hàm này clone (sao chép) cái `timeline` hiện tại ra một mảng ảo. Nó chạy vòng lặp **Bước 2 -> Bước 4** liên tục (nhưng bỏ qua Bước 3 - không thực thi sát thương hay skill). Mỗi lần có một người chạm đích (AV = 0), nó ghi nhận EntityID đó vào danh sách UI, reset AV của ảo ảnh đó, rồi tiếp tục chạy vòng lặp cho đến khi thu thập đủ 10 ID.

Hệ thống này tách biệt hoàn toàn Logic chiến đấu và UI dự đoán, đảm bảo game chạy mượt mà, scale tốt và có thể bổ sung vô vàn cơ chế buff/debuff tốc độ phức tạp mà không sợ sập logic turn.


---


Để framework của bạn dễ bảo trì (Maintain) và mở rộng (Scaling), bạn phải áp dụng nguyên lý **Separation of Concerns (Tách biệt mối quan tâm)** kết hợp với **Event-Driven Architecture (Kiến trúc hướng sự kiện)**.
Dưới đây là sơ đồ kiến trúc, nguyên tắc và cách tổ chức các Interface/Class chuẩn mực cho bài toán này.
### 1. Phân tầng Kiến trúc (Layered Architecture)
Bạn cần chia code thành 3 tầng (Layers) hoàn toàn độc lập. Chúng không được gọi trực tiếp lẫn nhau mà phải giao tiếp qua Interfaces hoặc Event Bus.
- **Tầng Logic (Gameplay/Simulation Layer):** Chứa các thuật toán toán học, tính toán Action Value, sắp xếp Queue. Không chứa bất kỳ dòng code DirectX nào.
- **Tầng Cầu nối (UI Controller/Presenter Layer):** Lắng nghe dữ liệu từ Tầng Logic, dịch nó thành tọa độ hình học (ví dụ: Tính toán vị trí của 10 cái icon nhân vật trên thanh timeline).
- **Tầng Hiển thị (Render Layer - DirectX):** Nhận dữ liệu hình học (Vertices, UV, Texture) từ Tầng Cầu nối và vẽ ra màn hình.
### 2. Thiết kế Interfaces và Classes (C++)
#### A. Tầng Logic (The Brain)
Tầng này quản lý dữ liệu gốc. C++
```cpp
// Interface cốt lõi để các hệ thống khác lắng nghe sự kiện
class ITurnObserver {
public:
    virtual void OnTurnQueueUpdated(const std::vector<int>& simulatedEntityIDs) = 0;
    virtual void OnEntityTurnStart(int entityID) = 0;
};

// Class quản lý Logic
class BattleTurnManager {
private:
    std::vector<TurnNode> m_timeline;
    std::vector<ITurnObserver*> m_observers; // Danh sách người lắng nghe

    // Hàm nội bộ chạy thuật toán Action Value đã bàn ở phần trước
    void CalculateNextTurn(); 
    
public:
    void RegisterObserver(ITurnObserver* observer);
    
    // Khi gọi hàm này, nó tính toán xong sẽ phát sự kiện (Notify)
    void AdvanceTime() {
        // ... Logic tính toán tick và AV ...
        
        // Sinh ra mảng dự đoán 10 lượt tiếp theo
        std::vector<int> futureTurns = SimulateFutureTurns(10);
        
        // Thông báo cho tất cả Observers
        for(auto obs : m_observers) {
            obs->OnTurnQueueUpdated(futureTurns);
        }
    }
};
```
#### B. Tầng Cầu nối (The Bridge / UI Controller)
Tầng này đóng vai trò dịch thuật. Nó implement `ITurnObserver` để nghe ngóng từ Logic, và dùng hệ thống UI (9-Slice).
#### C. Tầng Hiển thị (The Brush / DirectX)
Một Interface thuần túy giấu đi sự phức tạp của DirectX.
### 3. Nguyên tắc Code (Clean Code & SOLID) để dễ Scale
- **Single Responsibility Principle (SRP):** `BattleTurnManager` chỉ biết đếm số. Nó không biết nhân vật trông như thế nào. `TurnTimelineUI` chỉ biết sắp xếp các ô vuông trên màn hình. Nó không biết tại sao nhân vật lại bị trừ Agility.
- **Dependency Inversion Principle (DIP):** `TurnTimelineUI` không phụ thuộc trực tiếp vào `DirectX11Renderer`. Nó phụ thuộc vào Interface `IRenderer`. Nhờ vậy, nếu sau này bạn muốn port game sang Vulkan hay PlayStation, bạn chỉ việc viết class mới mà không cần sửa code UI.
- **Data-Driven Design:** Không gán cứng chỉ số Agility trong code C++. Toàn bộ chỉ số quái vật và tọa độ khung UI phải được đọc từ file JSON.
### 4. Cấu trúc thư mục (Folder Structure)
Một project C++ lớn cần tổ chức thư mục phản ánh đúng kiến trúc trên:
Plaintext
```
GameEngine/
│
├── Core/                   # Các công cụ xài chung
│   ├── EventBus/           # Hệ thống bắn sự kiện tổng
│   └── Math/               # Vector, Matrix
│
├── Gameplay/               # Tầng Logic (Chỉ dùng C++ chuẩn)
│   ├── Stats/              # Agility, HP, MP
│   └── BattleSystem/       # BattleTurnManager.h/.cpp
│
├── UI/                     # Tầng Cầu nối
│   ├── Core/               # Thuật toán sinh lưới 9-Slice 16 đỉnh
│   └── Battle/             # TurnTimelineUI.h/.cpp
│
└── Graphics/               # Tầng DirectX (Chứa mọi code phụ thuộc Windows/DX)
    ├── Renderer/           # IRenderer.h, DirectX11Renderer.cpp
    └── Shaders/            # HLSL code cho UI Pixel Art
```

Cách thiết kế này đảm bảo rằng: Khi Designer yêu cầu "Đổi giao diện Queue từ hàng ngang sang xếp thành vòng tròn", bạn chỉ cần vào sửa đúng class `TurnTimelineUI` (đổi trục X thành hàm Cos/Sin). Logic đánh turn và hệ thống DirectX vẫn đứng im không bị chạm tới một dòng code nào.
