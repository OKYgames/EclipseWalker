#include "IocpCore.h"

bool IocpCore::Initialize()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int threadCount = si.dwNumberOfProcessors * 2;

    _iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, threadCount);
    if (_iocpHandle == nullptr) return false;

    LOG_INFO("IOCP Created. Threads: %d", threadCount);

    for (int i = 0; i < threadCount; i++)
        _threads.push_back(std::thread(&IocpCore::WorkerThread, this));

    return true;
}

void IocpCore::Register(std::shared_ptr<Session> session)
{
    ULONG_PTR key = (ULONG_PTR)session.get();

    // shared_ptr을 맵에 보관 (세션이 살아있는 동안 해제 방지)
    {
        std::lock_guard<std::mutex> lock(_sessionLock);
        _sessionMap[key] = session;
    }

    CreateIoCompletionPort((HANDLE)session->_socket, _iocpHandle, key, 0);
}

void IocpCore::WorkerThread()
{
    while (true)
    {
        DWORD bytesTransferred = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL ret = GetQueuedCompletionStatus(
            _iocpHandle, &bytesTransferred, &key, &overlapped, INFINITE);

        if (ret && key)
        {
            // raw pointer 대신 맵에서 shared_ptr 꺼내기
            std::shared_ptr<Session> session;
            {
                std::lock_guard<std::mutex> lock(_sessionLock);
                auto it = _sessionMap.find(key);
                if (it == _sessionMap.end()) continue;
                session = it->second;
            }

            IocpEvent* iocpEvent = (IocpEvent*)overlapped;
            session->Dispatch(iocpEvent, bytesTransferred);

            // 연결 끊김 처리 (bytesTransferred == 0)
            if (bytesTransferred == 0)
            {
                std::lock_guard<std::mutex> lock(_sessionLock);
                _sessionMap.erase(key);
            }
        }
        else
        {
            // 에러 처리
            if (overlapped != nullptr && key != 0)
            {
                std::lock_guard<std::mutex> lock(_sessionLock);
                _sessionMap.erase(key);
            }
        }
    }
}