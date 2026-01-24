#pragma once
#include <queue>
#include <functional>
#include <mutex>
#include "Define.h"

// 일감(Job)은 "매개변수 없고 리턴값 없는 함수" 형태로 저장
using Job = std::function<void()>;

class GlobalQueue
{
public:
    GlobalQueue();
    ~GlobalQueue();

    // 일감 넣기 (웨이터가 주문표 꽂기)
    void Push(Job job);

    // 일감 꺼내서 처리하기 (주방장이 요리하기)
    // 일감이 없으면 기다림 (Block)
    void Execute();

private:
    std::queue<Job> _jobQueue;
    std::mutex _lock;
    std::condition_variable _condVar; // 일감 들어올 때까지 자면서 대기하게 해주는 도구
};

// 전역에서 어디서든 쓸 수 있게 선언
extern GlobalQueue* G_JobQueue;