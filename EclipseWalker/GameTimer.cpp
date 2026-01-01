#include "GameTimer.h"
#include <windows.h>

GameTimer::GameTimer()
    : mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
    mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    mSecondsPerCount = 1.0 / (double)countsPerSec;
}

// 총 게임 실행 시간 
float GameTimer::TotalTime()const
{
    if (mStopped)
    {
        return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
    else
    {
        return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
    }
}

float GameTimer::DeltaTime()const
{
    return (float)mDeltaTime;
}

void GameTimer::Reset()
{
    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

    mBaseTime = currTime;
    mPrevTime = currTime;
    mStopTime = 0;
    mStopped = false;
}

void GameTimer::Start()
{
    __int64 startTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

    if (mStopped)
    {
        mPausedTime += (startTime - mStopTime);
        mPrevTime = startTime;
        mStopTime = 0;
        mStopped = false;
    }
}

void GameTimer::Stop()
{
    if (!mStopped)
    {
        __int64 currTime;
        QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
        mStopTime = currTime;
        mStopped = true;
    }
}

void GameTimer::Tick()
{
    if (mStopped)
    {
        mDeltaTime = 0.0;
        return;
    }

    __int64 currTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    mCurrTime = currTime;

    // 현재 시간 - 이전 시간 = 델타 타임
    mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
    mPrevTime = mCurrTime;

    if (mDeltaTime < 0.0)
    {
        mDeltaTime = 0.0;
    }
}