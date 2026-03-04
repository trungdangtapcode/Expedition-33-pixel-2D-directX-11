# Game Loop & Time Management

## Vấn đề cần giải quyết

Một game cần chạy **liên tục** và **đều đặn**, bất kể máy tính nhanh hay chậm.
Nếu không quản lý thời gian đúng cách:

- Trên máy chậm: nhân vật di chuyển chậm hơn.
- Trên máy nhanh: nhân vật bay đi vì logic chạy quá nhiều lần/giây.
- Giải pháp: **Delta Time** — nhân mọi chuyển động với thời gian thực tế của frame.

---

## Tại sao không dùng `clock()` hay `time()`?

| Hàm | Độ phân giải | Vấn đề |
|---|---|---|
| `clock()` | ~15ms (Win32) | Quá thô — 1 frame 60fps chỉ ~16.6ms, sai số lên tới 90% |
| `time()` | 1 giây | Hoàn toàn vô dụng cho game |
| `QueryPerformanceCounter` | < 1 microsecond | ✅ Chính xác, dùng trong mọi game engine thực tế |

---

## Cách `GameTimer` hoạt động

```
src/Core/GameTimer.h
src/Core/GameTimer.cpp
```

### Khái niệm cốt lõi: Performance Counter

Windows có một bộ đếm phần cứng chạy với tần số rất cao (thường vài triệu tới vài tỷ "ticks" mỗi giây):

```cpp
__int64 frequency;
QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
// frequency ≈ 10,000,000 trên hầu hết Windows 10/11

double secondsPerCount = 1.0 / (double)frequency;
// secondsPerCount ≈ 0.0000001 giây/tick = 100 nanoseconds/tick
```

Mỗi frame, ta đọc giá trị counter hiện tại và trừ đi giá trị frame trước:

```cpp
QueryPerformanceCounter((LARGE_INTEGER*)&mCurrTime);
mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
mPrevTime  = mCurrTime;
```

### Timeline của TotalTime (trừ thời gian Pause)

```
       |-- paused --|       |-- paused --|
  Base  Stop  Start   Stop   Start        Now
  |      |     |        |     |            |
  |======|     |========|     |============|  ← Thời gian thực sự chạy
  
TotalTime = (Now - Base) - TotalPausedDuration
```

### API

```cpp
GameTimer timer;

timer.Reset();             // Gọi 1 lần trước game loop
timer.Tick();              // Gọi đầu mỗi frame
float dt = timer.DeltaTime();  // Giây kể từ frame trước (~0.016 ở 60fps)
float t  = timer.TotalTime();  // Tổng thời gian game đã chạy (không tính pause)

timer.Stop();              // Khi alt-tab hoặc minimize
timer.Start();             // Khi cửa sổ được focus lại
```

### Cách dùng deltaTime đúng

```cpp
// ✅ ĐÚNG: chuyển động độc lập với FPS
position.x += speed * dt;       // speed = 200 units/giây
cooldown   -= dt;                // Giảm dần theo thời gian thực
animation  += dt * frameRate;    // Phát animation đúng tốc độ

// ❌ SAI: phụ thuộc FPS
position.x += 3.3f;  // Trên 60fps: +3.3/frame. Trên 120fps: nhanh gấp đôi!
```

---

## Cấu trúc Game Loop

```cpp
// src/Core/GameApp.cpp
int GameApp::Run() {
    MSG msg = {};
    mTimer.Reset();

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            // 1. Ưu tiên xử lý sự kiện Windows (Input, Resize...)
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // 2. Windows rảnh → chạy game
            mTimer.Tick();

            if (!mPaused) {
                Update(mTimer.DeltaTime());
                Render();
            } else {
                Sleep(16); // Nhường CPU khi minimize
            }
        }
    }
}
```

### Tại sao dùng `PeekMessage` thay vì `GetMessage`?

| Hàm | Hành vi | Phù hợp cho |
|---|---|---|
| `GetMessage` | **Chặn** luồng, chờ đến khi có sự kiện | Ứng dụng GUI thông thường |
| `PeekMessage` | **Không chặn**, trả về ngay lập tức | Game (cần render liên tục dù không có input) |

### Sleep khi Pause

Khi game bị minimize hoặc mất focus, ta gọi `Sleep(16)`:
- Tránh tiêu thụ 100% CPU cho vòng lặp rỗng.
- 16ms ≈ 1 frame tại 60fps — đủ responsive nhưng không tốn tài nguyên.
- `GameTimer::Stop()` được gọi đồng thời → `deltaTime = 0` → không bị "time jump" khi resume.
