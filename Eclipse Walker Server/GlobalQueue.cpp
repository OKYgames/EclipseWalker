#include "GlobalQueue.h"

GlobalQueue* G_JobQueue = nullptr;

GlobalQueue::GlobalQueue()
{
}

GlobalQueue::~GlobalQueue()
{
}

void GlobalQueue::Push(Job job)
{
    std::lock_guard<std::mutex> lock(_lock);
    _jobQueue.push(job);

    // "일감 들어왔다! 일어나라!" 하고 자고 있는 스레드 깨움
    _condVar.notify_one();
}

void GlobalQueue::Execute()
{
    std::unique_lock<std::mutex> lock(_lock);

    // 큐가 비어있으면 
    if (_jobQueue.empty())
    {
        // 누가 깨울 때까지(Push 할 때까지) 여기서 잠듬 (CPU 사용률 0%)
        _condVar.wait(lock);
    }

    // 일어났는데 큐에 뭔가 있다?
    if (_jobQueue.empty() == false)
    {
        Job job = _jobQueue.front();
        _jobQueue.pop();

        lock.unlock(); // 일할 때는 락 풀고 함 (중요)

        job(); // 실제 함수 실행!
    }
}