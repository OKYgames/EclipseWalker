#pragma once
#include "Define.h"
#include "Session.h"
#include <unordered_map>
#include <mutex>

class IocpCore
{
public:
    bool Initialize();
    void Start();
    void Register(std::shared_ptr<Session> session);

private:
    void WorkerThread();

private:
    HANDLE _iocpHandle;
    std::vector<std::thread> _threads;
    std::unordered_map<ULONG_PTR, std::shared_ptr<Session>> _sessionMap;
    std::mutex _sessionLock;
};