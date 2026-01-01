#pragma once
#include <windows.h>

class GameTimer
{
public:
    GameTimer();

    float TotalTime()const; // 게임 시작 후 총 흐른 시간 (초)
    float DeltaTime()const; // 이전 프레임부터 지금 프레임까지 걸린 시간 (초)

    void Reset(); // 타이머 리셋
    void Start(); // 타이머 시작 (일시정지 해제)
    void Stop();  // 타이머 일시정지
    void Tick();  // 매 프레임마다 호출해서 시간 갱신

private:
    double mSecondsPerCount;
    double mDeltaTime;

    __int64 mBaseTime;
    __int64 mPausedTime;
    __int64 mStopTime;
    __int64 mPrevTime;
    __int64 mCurrTime;

    bool mStopped;
};