#pragma once
#include "Define.h"
#include "Session.h"

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
};