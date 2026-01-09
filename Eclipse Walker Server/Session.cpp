#include "Session.h"

Session::Session() : _recvEvent(EventType::Recv), _sendEvent(EventType::Send), _socket(INVALID_SOCKET)
{
    _recvEvent.owner = this;
    _sendEvent.owner = this;
}

Session::~Session()
{
    closesocket(_socket);
}

void Session::Init(SOCKET socket, SOCKADDR_IN address)
{
    _socket = socket;
    _addr = address;
    OnConnected();
    RegisterRecv();
}

void Session::Send(void* msg, int len)
{
    std::lock_guard<std::mutex> lock(_lock);

    WSABUF wsaBuf;
    wsaBuf.buf = (char*)msg;
    wsaBuf.len = len;

    DWORD numOfBytes = 0;

    if (WSASend(_socket, &wsaBuf, 1, &numOfBytes, 0, &_sendEvent, nullptr) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            LOG_ERROR("Send Error: %d", WSAGetLastError());
        }
    }
}

void Session::RegisterRecv()
{
    DWORD flags = 0;
    DWORD numOfBytes = 0;
    WSABUF wsaBuf;
    wsaBuf.buf = _recvBuffer;
    wsaBuf.len = sizeof(_recvBuffer);

    if (WSARecv(_socket, &wsaBuf, 1, &numOfBytes, &flags, &_recvEvent, nullptr) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            LOG_ERROR("Recv Error: %d", WSAGetLastError());
        }
    }
}

void Session::Dispatch(IocpEvent* iocpEvent, int numOfBytes)
{
    switch (iocpEvent->type)
    {
    case EventType::Recv:
        HandleRecv(numOfBytes);
        break;
    case EventType::Send:
        HandleSend(numOfBytes);
        break;
    }
}

void Session::HandleRecv(int numOfBytes)
{
    if (numOfBytes == 0)
    {
        OnDisconnected();
        return;
    }

    OnRecv((BYTE*)_recvBuffer, numOfBytes);

    RegisterRecv();
}

void Session::HandleSend(int numOfBytes)
{
    OnSend(numOfBytes);
}