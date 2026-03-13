# Refactoring BattleState: Clean Code & SRP

## 1. Vấn đề trước khi Refactor (The Problem)
File BattleState.cpp ban đầu đang rơi vào tình trạng **God Object** (một class xử lý quá nhiều trách nhiệm). Nó phải đồng thời:
- Khởi tạo tất cả dữ liệu trận đấu, sprites, UI, và âm thanh.
- Quản lý vòng lặp trạng thái nhập liệu của người chơi (Chọn Lệnh -> Chọn Chiêu -> Chọn Mục tiêu).
- Liên tục lắng nghe phím bấm trực tiếp (GetAsyncKeyState).
- Đồng bộ giao diện UI theo dữ liệu trận đấu (chia scale, rung lắc).
- Kiểm tra logic Thắng/Thua và cập nhật hoạt ảnh.

Điều này khiến file phình to quá mức, rất khó để đọc hiểu, test hay mở rộng.

## 2. Các bước Refactor đã thực hiện

### A. Tách riêng BattleInputController
Tất cả logic liên quan đến tương tác của người chơi đã được chuyển sang thư mục src/Battle/BattleInputController.h & .cpp.
- **Quản lý Input FSM (Finite State Machine):** Vòng lặp nhập liệu (COMMAND_SELECT, SKILL_SELECT, TARGET_SELECT) giờ được đóng gói và có Controller riêng.
- **Điều khiển UI/Camera liên kết Input:** Khi người chơi đổi menu chọn, Controller sẽ quyết định logic zoom cận nhân vật hay tập trung sang quái vật.
- **Áp dụng Nguyên tắc SRP:** BattleState giờ không cần biết VK_UP hay VK_ENTER là gì, BattleInputController xử lý và gọi hàm chuyển State một cách gọn gàng.

### B. Rút gọn hàm bằng Extract Method
Kỹ thuật "Extract Method" được sử dụng để chia những khối code hàng trăm dòng thành các hàm private nhỏ có tên gọi thể hiện rõ chức năng (Self-documenting code).

**Chia nhỏ Khởi tạo màn chơi (OnEnter):**
- InitAudio(): Chuyển đổi nhạc nền.
- InitBattleSlots(): Nạp vị trí grid tọa độ (từ ormations.json) cho Player và Enemies.
- InitUIRenderers(): Load HP bars, Font chữ, Text renderers.

**Quản lý Vòng lặp (Update):** Vòng lặp sinh tử của game giờ giống như một bản tóm tắt công việc:
`cpp
void BattleState::Update(float dt) {
    if (mPendingSafeExit) { 
        HandleSafeExit(); 
        return; 
    }

    if (!mExitTransitionStarted) {
        UpdateLogic(dt);            // Core cập nhật Battle & Controller
        CheckDeathAnimations();     // Chạy chặn tiến trình nếu có entity chết
        UpdateUIRenderers(dt, ...); // Tính toán rung lắc, đổi màu scale thanh máu
        CheckBattleEnd();           // Đóng game logic, nảy Iris chuyển cảnh
    }
}
`

## 3. Lợi ích mang lại
- **Tính dễ đọc (Readability):** Bất cứ ai tham gia vào file BattleState.cpp đều mường tượng được pipeline diễn ra trong mỗi frame mà không bị lạc trôi giữa những logic if/else mịt mù.
- **Tính dễ bảo trì (Maintainability):** Cấu trúc bị hỏng đâu thì fix ở đó. Muốn thêm tính năng Item / Guard? Mở BattleInputController thêm vào mà không ảnh hưởng tới logic đánh đấm. UI bị lỗi scale nhầm? Mở thẳng UpdateUIRenderers.
- **Dependency Injection (Khử phụ thuộc):** Controller mới được truyền các tham chiếu BattleManager thông qua Constructor rất minh bạch.
