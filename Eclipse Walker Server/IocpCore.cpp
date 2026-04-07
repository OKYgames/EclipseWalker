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
    // shared_ptr을 하나 더 만들어서 raw pointer로 키값 사용
    // -> 세션이 살아있는 동안 참조 카운트가 유지됨
    std::shared_ptr<Session>* sessionPtr = new std::shared_ptr<Session>(session);

    CreateIoCompletionPort(
        (HANDLE)session->_socket,
        _iocpHandle,
        (ULONG_PTR)sessionPtr, // 키값으로 shared_ptr 포인터 사용
        0);
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

        if (!ret || overlapped == nullptr || key == 0) continue;

        // 락 없이 안전하게 shared_ptr 꺼내기
        std::shared_ptr<Session>* sessionPtr =
            reinterpret_cast<std::shared_ptr<Session>*>(key);

        std::shared_ptr<Session> session = *sessionPtr;

        IocpEvent* iocpEvent = (IocpEvent*)overlapped;
        session->Dispatch(iocpEvent, bytesTransferred);

        // 연결 끊김이면 shared_ptr 해제
        if (bytesTransferred == 0)
        {
            delete sessionPtr;
        }
    }
}