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
    // 세션(소켓)을 IOCP 핸들과 연결. Key값으로 세션 포인터를 넘김
    CreateIoCompletionPort((HANDLE)session->_socket, _iocpHandle, (ULONG_PTR)session.get(), 0);
}

void IocpCore::WorkerThread()
{
    while (true)
    {
        DWORD bytesTransferred = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL ret = GetQueuedCompletionStatus(_iocpHandle, &bytesTransferred, &key, &overlapped, INFINITE);

        if (ret && key)
        {
            Session* session = (Session*)key;
            IocpEvent* iocpEvent = (IocpEvent*)overlapped;

            session->Dispatch(iocpEvent, bytesTransferred);
        }
        else
        {
            // 에러 처리 
        }
    }
}