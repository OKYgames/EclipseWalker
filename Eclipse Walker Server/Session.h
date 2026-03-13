#pragma once
#include "Define.h"
#include "RecvBuffer.h"
#include <queue> // 큐 추가

class Session : public std::enable_shared_from_this<Session>
{
    friend class IocpCore;
public:
    Session();
    virtual ~Session();

    void Init(SOCKET socket, SOCKADDR_IN address);

    // 이제 Send는 바로 보내지 않고 큐에 넣기만 함
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

    // [추가] 실제로 WSASend를 거는 함수
    void RegisterSend();

private:
    SOCKET _socket;
    SOCKADDR_IN _addr;

    IocpEvent _recvEvent;
    IocpEvent _sendEvent;

    RecvBuffer _recvBuffer;

    // [수정] 보내기 큐 (데이터를 쌓아두는 곳)
    std::queue<std::vector<BYTE>> _sendQueue;

    // [수정] 현재 전송 중인지 체크하는 플래그
    bool _sendRegistered = false;

    // [수정] WSASend에 넘겨줄 버퍼 구조체 (멤버변수로 유지해야 함)
    WSABUF _sendWsaBuf;

    std::mutex _lock;
};