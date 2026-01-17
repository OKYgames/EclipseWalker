#include "Session.h"

Session::Session() : _recvBuffer(65536), _recvEvent(EventType::Recv), _sendEvent(EventType::Send), _socket(INVALID_SOCKET)
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

    memcpy(_sendBuffer, msg, len);

    _sendWsaBuf.buf = _sendBuffer;
    _sendWsaBuf.len = len;

    DWORD numOfBytes = 0;

    if (WSASend(_socket, &_sendWsaBuf, 1, &numOfBytes, 0, &_sendEvent, nullptr) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            LOG_ERROR("Send Error: %d", WSAGetLastError());
        }
    }
}

void Session::RegisterRecv()
{
    _recvBuffer.Clean();

    WSABUF wsaBuf;
    wsaBuf.buf = (char*)_recvBuffer.WritePos();
    wsaBuf.len = _recvBuffer.FreeSize();

    DWORD numOfBytes = 0;
    DWORD flags = 0;

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

    if (_recvBuffer.OnWrite(numOfBytes) == false)
    {
        OnDisconnected();
        return;
    }

    int processLen = OnRecv(_recvBuffer.ReadPos(), _recvBuffer.DataSize());

    if (processLen < 0 || processLen > _recvBuffer.DataSize())
    {
        OnDisconnected();
        return;
    }

    if (_recvBuffer.OnRead(processLen) == false)
    {
        OnDisconnected();
        return;
    }

    RegisterRecv();
}

void Session::HandleSend(int numOfBytes)
{
    OnSend(numOfBytes);
}