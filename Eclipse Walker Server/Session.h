#pragma once
#include "Define.h"
#include "RecvBuffer.h"
#include <algorithm>
#include <queue>
#include <string>

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
    const std::string& GetDisplayName() const { return _displayName; }
    void  SetDisplayName(const std::string& displayName) { _displayName = displayName; }
    int   GetPlayerHp() const { return _playerHp; }
    bool  IsPlayerDead() const { return _playerDead; }
    void  ResetPlayerCombatState()
    {
        _playerHp = _playerMaxHp;
        _playerDead = false;
    }
    void  RespawnPlayer(float x, float y, float z)
    {
        _x = x;
        _y = y;
        _z = z;
        ResetPlayerCombatState();
    }
    bool  ApplyPlayerDamage(int damage)
    {
        if (damage <= 0 || _playerDead)
        {
            return _playerDead;
        }

        _playerHp -= damage;
        if (_playerHp <= 0)
        {
            _playerHp = 0;
            _playerDead = true;
        }

        return _playerDead;
    }
    void  ResetLanternState()
    {
        _lanternGauge = 0.0f;
        _lanternMaxGauge = 100.0f;
        _lanternLevel = 1;
    }
    float AddLanternCharge(float amount)
    {
        if (amount <= 0.0f)
        {
            return 0.0f;
        }

        const float before = _lanternGauge;
        _lanternGauge = (std::min)(_lanternGauge + amount, _lanternMaxGauge);
        return _lanternGauge - before;
    }
    bool CanUseWorldShift() const
    {
        return _lanternMaxGauge > 0.0f && _lanternGauge >= _lanternMaxGauge;
    }
    void ConsumeWorldShift()
    {
        _lanternGauge = 0.0f;
    }
    float GetLanternGauge() const { return _lanternGauge; }
    float GetLanternMaxGauge() const { return _lanternMaxGauge; }
    int   GetLanternLevel() const { return _lanternLevel; }
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
    std::string _displayName;
    int   _playerMaxHp = 200;
    int   _playerHp = 200;
    bool  _playerDead = false;
    float _lanternGauge = 0.0f;
    float _lanternMaxGauge = 100.0f;
    int   _lanternLevel = 1;
};
