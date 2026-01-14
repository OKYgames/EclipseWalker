#pragma once
#include "Define.h"

class Session : public std::enable_shared_from_this<Session>
{
    friend class IocpCore;
public:
    Session();
    virtual ~Session();

    void Init(SOCKET socket, SOCKADDR_IN address);

    void Send(void* msg, int len);

    void RegisterRecv();

    void Dispatch(IocpEvent* iocpEvent, int numOfBytes);

protected:
    virtual void OnConnected() {}
    virtual void OnDisconnected() {}
    virtual void OnRecv(BYTE* buffer, int len) {}
    virtual void OnSend(int len) {}

private:
    void HandleRecv(int numOfBytes);
    void HandleSend(int numOfBytes);

private:
    SOCKET _socket;
    SOCKADDR_IN _addr;

    IocpEvent _recvEvent;
    IocpEvent _sendEvent;

    char _recvBuffer[65536]; // 64KB 수신 버퍼
    std::mutex _lock;
};