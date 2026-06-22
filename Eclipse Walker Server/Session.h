#pragma once
#include "Define.h"
#include "RecvBuffer.h"
#include <algorithm>
#include <chrono>
#include <cmath>
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
    float GetRotY() const { return _rotY; }
    int   GetPlayerClassType() const { return _playerClassType; }
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
        _nextAttackAllowedAt = {};
        _hasPendingPlayerAttack = false;
        _pendingPlayerAttackSkillType = -1;
        _pendingPlayerAttackExpiresAt = {};
    }
    void  RespawnPlayer(float x, float y, float z)
    {
        _x = x;
        _y = y;
        _z = z;
        _rotY = 0.0f;
        ResetPlayerCombatState();
        ResetMoveValidation();
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
    void  FillLanternGauge()
    {
        _lanternGauge = _lanternMaxGauge;
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
    bool  RegisterPlayerClass(int classType)
    {
        constexpr int kFirstPlayerClass = 0;
        constexpr int kLastPlayerClass = 2;
        if (classType < kFirstPlayerClass || classType > kLastPlayerClass)
        {
            return false;
        }

        if (_playerClassType < 0)
        {
            _playerClassType = classType;
        }

        return _playerClassType == classType;
    }
    bool  TryBeginPlayerAttack(int skillType, float cooldownSeconds)
    {
        if (_playerDead || skillType < 0 || skillType > 2 || cooldownSeconds <= 0.0f)
        {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < _nextAttackAllowedAt)
        {
            return false;
        }

        _nextAttackAllowedAt = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(cooldownSeconds));
        _hasPendingPlayerAttack = true;
        _pendingPlayerAttackSkillType = skillType;
        _pendingPlayerAttackExpiresAt = now + std::chrono::seconds(3);
        return true;
    }
    bool  TryConsumePlayerAttackImpact(int skillType)
    {
        const auto now = std::chrono::steady_clock::now();
        if (_playerDead || !_hasPendingPlayerAttack ||
            _pendingPlayerAttackSkillType != skillType || now > _pendingPlayerAttackExpiresAt)
        {
            return false;
        }

        _hasPendingPlayerAttack = false;
        _pendingPlayerAttackSkillType = -1;
        _pendingPlayerAttackExpiresAt = {};
        return true;
    }
    void  ResetMoveValidation()
    {
        _hasAcceptedMove = false;
        _moveBudget = 0.0f;
        _lastAcceptedMoveAt = {};
    }
    bool  TryUpdatePlayerPosition(float x, float y, float z, float rotY, float maxSpeed, float maxBurstDistance)
    {
        if (_playerDead || maxSpeed <= 0.0f || maxBurstDistance <= 0.0f)
        {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!_hasAcceptedMove)
        {
            _x = x;
            _y = y;
            _z = z;
            _rotY = rotY;
            _hasAcceptedMove = true;
            _moveBudget = maxBurstDistance;
            _lastAcceptedMoveAt = now;
            return true;
        }

        const float elapsedSeconds = (std::max)(
            0.0f,
            std::chrono::duration<float>(now - _lastAcceptedMoveAt).count());
        _moveBudget = (std::min)(maxBurstDistance, _moveBudget + maxSpeed * elapsedSeconds);

        const float dx = x - _x;
        const float dy = y - _y;
        const float dz = z - _z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        if (distanceSq > _moveBudget * _moveBudget)
        {
            return false;
        }

        _moveBudget = (std::max)(0.0f, _moveBudget - std::sqrt(distanceSq));
        _x = x;
        _y = y;
        _z = z;
        _rotY = rotY;
        _lastAcceptedMoveAt = now;
        return true;
    }
    void  SetPlayerInfo(int id, float x, float y, float z)
    {
        _playerId = id;
        _x = x;
        _y = y;
        _z = z;
        _rotY = 0.0f;
        _playerClassType = -1;
        ResetMoveValidation();
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
    float _rotY = 0.0f;
    int   _playerClassType = -1;
    bool  _ready = false;
    std::string _displayName;
    int   _playerMaxHp = 200;
    int   _playerHp = 200;
    bool  _playerDead = false;
    float _lanternGauge = 0.0f;
    float _lanternMaxGauge = 100.0f;
    int   _lanternLevel = 1;
    std::chrono::steady_clock::time_point _nextAttackAllowedAt;
    bool _hasPendingPlayerAttack = false;
    int _pendingPlayerAttackSkillType = -1;
    std::chrono::steady_clock::time_point _pendingPlayerAttackExpiresAt;
    bool _hasAcceptedMove = false;
    float _moveBudget = 0.0f;
    std::chrono::steady_clock::time_point _lastAcceptedMoveAt;
};
