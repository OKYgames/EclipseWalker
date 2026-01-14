#pragma once
#include "Define.h"
#include "RecvBuffer.h"

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
    virtual int OnRecv(BYTE* buffer, int len) { return len; }
    virtual void OnSend(int len) {}

private:
    void HandleRecv(int numOfBytes);
    void HandleSend(int numOfBytes);

private:
    SOCKET _socket;
    SOCKADDR_IN _addr;

    IocpEvent _recvEvent;
    IocpEvent _sendEvent;

    RecvBuffer _recvBuffer;

    char _sendBuffer[65536]; // [수정] 보내기용 버퍼를 따로 만듦
    WSABUF _sendWsaBuf;

    std::mutex _lock;
};