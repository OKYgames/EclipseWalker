#pragma once
#include "Define.h"
#include "RecvBuffer.h"
#include <queue>

class Session : public std::enable_shared_from_this<Session>
{
    friend class IocpCore;
public:
    Session();
    virtual ~Session();

    void Init(SOCKET socket, SOCKADDR_IN address);
    void Start();
    void Send(void* msg, int len);
    void RegisterRecv();
    void Dispatch(IocpEvent* iocpEvent, int numOfBytes);
    void Disconnect();

    int   GetPlayerId() { return _playerId; }
    float GetX() { return _x; }
    float GetY() { return _y; }
    float GetZ() { return _z; }
    bool  IsReady() { return _ready; }
    void  SetReady(bool ready) { _ready = ready; }
    void  SetPlayerInfo(int id, float x, float y, float z)
    {
        _playerId = id;
        _x = x;
        _y = y;
        _z = z;
    }

protected:
    virtual void OnConnected() {}
    virtual void OnDisconnected() {}
    virtual int  OnRecv(BYTE* buffer, int len) { return len; }
    virtual void OnSend(int len) {}

private:
    void HandleRecv(int numOfBytes);
    void HandleSend(int numOfBytes);
    void RegisterSend();

private:
    SOCKET      _socket;
    SOCKADDR_IN _addr;

    IocpEvent  _recvEvent;
    IocpEvent  _sendEvent;

    RecvBuffer _recvBuffer;

    std::queue<std::vector<BYTE>> _sendQueue;
    bool   _sendRegistered = false;
    WSABUF _sendWsaBuf;

    std::mutex _lock;
    bool _disconnected = false;

    int   _playerId = -1;
    float _x = 0.0f;
    float _y = 0.0f;
    float _z = 0.0f;
    bool  _ready = false;
};
