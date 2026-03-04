#pragma once
#include <windows.h>

// ============================================================
// GameTimer - Quản lý thời gian game bằng High-Resolution Timer
//
// TUYỆT ĐỐI không dùng clock() hay time() vì độ phân giải thấp (~15ms).
// QueryPerformanceCounter cho độ phân giải < 1 microsecond.
//
// CÁCH DÙNG:
//   timer.Reset();            // Gọi 1 lần trước vòng lặp
//   timer.Tick();             // Gọi mỗi frame
//   float dt = timer.DeltaTime();   // Dùng ở khắp nơi
//   float t  = timer.TotalTime();   // Thời gian tổng (trừ thời gian paused)
// ============================================================
class GameTimer {
public:
    GameTimer();

    // Thời gian tổng game đã chạy (không tính thời gian paused)
    float TotalTime() const;

    // Thời gian giữa 2 frame liên tiếp (đơn vị: giây)
    // Dùng để nhân với mọi movement, animation, cooldown...
    float DeltaTime() const;

    void Reset();  // Gọi ngay trước khi bước vào game loop
    void Start();  // Gọi khi unpause (tiếp tục game sau khi pause)
    void Stop();   // Gọi khi pause (minimize, menu, ...)
    void Tick();   // Gọi mỗi frame - cập nhật deltaTime

private:
    double  mSecondsPerCount; // Nghịch đảo của tần số bộ đếm (1 / frequency)
    double  mDeltaTime;       // Thời gian frame hiện tại (giây)

    __int64 mBaseTime;        // Thời điểm Reset() được gọi
    __int64 mPausedTime;      // Tổng thời gian đã bị pause
    __int64 mStopTime;        // Thời điểm Stop() được gọi gần nhất
    __int64 mPrevTime;        // Giá trị counter ở frame trước
    __int64 mCurrTime;        // Giá trị counter ở frame hiện tại

    bool    mStopped;         // Game hiện có đang bị pause không?
};
