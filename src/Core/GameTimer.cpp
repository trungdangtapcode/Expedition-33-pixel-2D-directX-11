#include "GameTimer.h"

GameTimer::GameTimer()
    : mSecondsPerCount(0.0), mDeltaTime(-1.0),
      mBaseTime(0), mPausedTime(0), mStopTime(0),
      mPrevTime(0), mCurrTime(0), mStopped(false)
{
    // Lấy tần số của bộ đếm hiệu năng cao của CPU
    // Tần số này cố định trong suốt vòng đời của process
    __int64 countsPerSec = 0;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    mSecondsPerCount = 1.0 / (double)countsPerSec;
}

// TotalTime: thời gian game đã chạy, KHÔNG tính thời gian pause
//
// Timeline minh họa:
//   |--pausedTime--|
//   start  stop  start   stop  start
//   |       |     |        |     |----> t
// Các đoạn paused được trừ đi khỏi TotalTime
float GameTimer::TotalTime() const {
    if (mStopped) {
        // Đang pause: không tính thời gian từ lúc stop đến hiện tại
        return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    } else {
        // Đang chạy: trừ toàn bộ thời gian đã pause ra khỏi tổng
        return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
}

float GameTimer::DeltaTime() const {
    return (float)mDeltaTime;
}

void GameTimer::Reset() {
    __int64 currTime = 0;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

    mBaseTime   = currTime;
    mPrevTime   = currTime;
    mStopTime   = 0;
    mPausedTime = 0;
    mStopped    = false;
}

void GameTimer::Stop() {
    // Chỉ xử lý nếu game chưa bị pause
    if (!mStopped) {
        __int64 currTime = 0;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

        mStopTime = currTime; // Ghi lại thời điểm pause
        mStopped  = true;
    }
}

void GameTimer::Start() {
    // Chỉ xử lý nếu game đang bị pause
    if (mStopped) {
        __int64 startTime = 0;
        QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

        // Cộng thêm thời gian đã bị dừng vào tổng mPausedTime
        // để TotalTime() trừ ra sau
        mPausedTime += (startTime - mStopTime);
        mPrevTime    = startTime; // Reset điểm tham chiếu tránh spike deltaTime
        mStopTime    = 0;
        mStopped     = false;
    }
}

void GameTimer::Tick() {
    if (mStopped) {
        // Đang pause: deltaTime = 0, không tốn thêm thời gian
        mDeltaTime = 0.0;
        return;
    }

    // Lấy giá trị counter tại frame này
    QueryPerformanceCounter((LARGE_INTEGER*)&mCurrTime);

    // Tính deltaTime = (thời gian frame này - frame trước) * số giây/count
    mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

    // Cập nhật mPrevTime cho frame sau
    mPrevTime = mCurrTime;

    // Bảo vệ khỏi deltaTime âm (có thể xảy ra do CPU power-saving
    // hoặc khi chạy trên multi-processor bất đồng bộ)
    if (mDeltaTime < 0.0) {
        mDeltaTime = 0.0;
    }
}
