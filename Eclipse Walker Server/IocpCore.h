#pragma once
#include "Define.h"
#include "Session.h"

class IocpCore
{
public:
    bool Initialize();
    void Start(); // 스레드 시작
    void Register(std::shared_ptr<Session> session); // 소켓을 IOCP에 등록

private:
    void WorkerThread();

private:
    HANDLE _iocpHandle;
    std::vector<std::thread> _threads;
};