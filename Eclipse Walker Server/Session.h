#pragma once
#include "Define.h"
#include "RecvBuffer.h"
#include <algorithm>
#include <array>
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
    int   GetPlayerLevel() const { return _playerLevel; }
    bool  IsReady() { return _ready; }
    void  SetReady(bool ready) { _ready = ready; }
    const std::string& GetDisplayName() const { return _displayName; }
    void  SetDisplayName(const std::string& displayName) { _displayName = displayName; }
    int   GetPlayerHp() const { return _playerHp; }
    int   GetPlayerMaxHp() const { return _playerMaxHp; }
    bool  IsPlayerDead() const { return _playerDead; }
    void  ResetPlayerCombatState()
    {
        _playerHp = _playerMaxHp;
        _playerDead = false;
        _playerRespawnAllowedAt = {};
        _playerRespawnInvulnerableUntil = {};
        _nextAttackAllowedAt.fill(std::chrono::steady_clock::time_point{});
        _pendingPlayerAttackCounts.fill(0);
        _pendingPlayerAttackExpiresAt.fill(std::chrono::steady_clock::time_point{});
        _archerAttackSpeedBuffExpiresAt = {};
    }
    void  RespawnPlayer(float x, float y, float z)
    {
        _x = x;
        _y = y;
        _z = z;
        _rotY = 0.0f;
        ResetPlayerCombatState();
        _playerRespawnInvulnerableUntil =
            std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<float>(kRespawnInvulnerabilitySeconds));
        ResetMoveValidation();
    }
    bool  ApplyPlayerDamage(int damage, bool* outDamageApplied = nullptr)
    {
        if (outDamageApplied != nullptr)
        {
            *outDamageApplied = false;
        }

        if (damage <= 0 || _playerDead || IsPlayerRespawnInvulnerable())
        {
            return _playerDead;
        }

        if (outDamageApplied != nullptr)
        {
            *outDamageApplied = true;
        }

        _playerHp -= damage;
        if (_playerHp <= 0)
        {
            _playerHp = 0;
            _playerDead = true;
            _playerRespawnAllowedAt = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        }

        return _playerDead;
    }
    bool  CanRespawnPlayer() const
    {
        return _playerDead && std::chrono::steady_clock::now() >= _playerRespawnAllowedAt;
    }
    bool  IsPlayerRespawnInvulnerable() const
    {
        return !_playerDead &&
            std::chrono::steady_clock::now() < _playerRespawnInvulnerableUntil;
    }
    bool  ApplyPlayerHeal(int amount)
    {
        if (amount <= 0 || _playerDead || _playerHp >= _playerMaxHp)
        {
            return false;
        }

        const int beforeHp = _playerHp;
        _playerHp = (std::min)(_playerHp + amount, _playerMaxHp);
        return _playerHp != beforeHp;
    }
    void  ResetLanternState()
    {
        _lanternGauge = 0.0f;
        _lanternMaxGauge = 250.0f;
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
    bool  RegisterPlayerClass(int classType, bool* hpChanged = nullptr)
    {
        constexpr int kFirstPlayerClass = 0;
        constexpr int kLastPlayerClass = 2;
        if (hpChanged != nullptr)
        {
            *hpChanged = false;
        }

        if (classType < kFirstPlayerClass || classType > kLastPlayerClass)
        {
            return false;
        }

        const bool classWasRegistered =
            _playerClassType >= kFirstPlayerClass && _playerClassType <= kLastPlayerClass;
        if (classWasRegistered &&
            _playerClassType != classType)
        {
            return false;
        }

        const int beforeHp = _playerHp;
        _playerClassType = classType;
        _playerMaxHp = GetMaxHpForClass(classType);
        if (!_playerDead)
        {
            if (!classWasRegistered || _playerHp <= 0)
            {
                _playerHp = _playerMaxHp;
            }
            else
            {
                _playerHp = (std::min)(_playerHp, _playerMaxHp);
            }
        }

        if (hpChanged != nullptr)
        {
            *hpChanged = _playerHp != beforeHp;
        }
        return true;
    }
    bool  RegisterPlayerLevel(int playerLevel)
    {
        constexpr int kMinPlayerLevel = 1;
        constexpr int kMaxPlayerLevel = 3;
        if (playerLevel < kMinPlayerLevel || playerLevel > kMaxPlayerLevel)
        {
            return false;
        }

        _playerLevel = playerLevel;
        return true;
    }
    bool  TryBeginPlayerAttack(int skillType, float cooldownSeconds, int pendingImpactCount = 1)
    {
        if (_playerDead || skillType < 0 || skillType > 2 || cooldownSeconds <= 0.0f || pendingImpactCount <= 0)
        {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < _nextAttackAllowedAt[skillType])
        {
            return false;
        }

        _nextAttackAllowedAt[skillType] = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(cooldownSeconds));
        _pendingPlayerAttackCounts[skillType] += pendingImpactCount;
        _pendingPlayerAttackExpiresAt[skillType] = now + std::chrono::seconds(3);
        return true;
    }
    bool  TryConsumePlayerAttackImpact(int skillType)
    {
        const auto now = std::chrono::steady_clock::now();
        if (_playerDead || skillType < 0 || skillType > 2 ||
            _pendingPlayerAttackCounts[skillType] <= 0 ||
            now > _pendingPlayerAttackExpiresAt[skillType])
        {
            if (skillType >= 0 && skillType <= 2)
            {
                _pendingPlayerAttackCounts[skillType] = 0;
                _pendingPlayerAttackExpiresAt[skillType] = std::chrono::steady_clock::time_point{};
            }
            return false;
        }

        --_pendingPlayerAttackCounts[skillType];
        if (_pendingPlayerAttackCounts[skillType] <= 0)
        {
            _pendingPlayerAttackExpiresAt[skillType] = std::chrono::steady_clock::time_point{};
        }
        return true;
    }
    void  ActivateArcherAttackSpeedBuff(float durationSeconds)
    {
        if (durationSeconds <= 0.0f)
        {
            _archerAttackSpeedBuffExpiresAt = {};
            return;
        }

        _archerAttackSpeedBuffExpiresAt = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<float>(durationSeconds));
    }
    bool  HasArcherAttackSpeedBuff() const
    {
        return std::chrono::steady_clock::now() < _archerAttackSpeedBuffExpiresAt;
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
        _playerLevel = 1;
        _playerMaxHp = 200;
        _playerHp = _playerMaxHp;
        _playerDead = false;
        _playerRespawnAllowedAt = {};
        _playerRespawnInvulnerableUntil = {};
        ResetMoveValidation();
    }

protected:
    virtual void OnConnected() {}
    virtual void OnDisconnected() {}
    virtual int  OnRecv(BYTE* buffer, int len) { return len; }
    virtual void OnSend(int len) {}

private:
    static constexpr float kRespawnInvulnerabilitySeconds = 2.5f;

    static int GetMaxHpForClass(int classType)
    {
        switch (classType)
        {
        case 0: return 500;
        case 1: return 150;
        case 2: return 250;
        default: return 200;
        }
    }

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
    int   _playerLevel = 1;
    bool  _ready = false;
    std::string _displayName;
    int   _playerMaxHp = 200;
    int   _playerHp = 200;
    bool  _playerDead = false;
    std::chrono::steady_clock::time_point _playerRespawnAllowedAt = {};
    std::chrono::steady_clock::time_point _playerRespawnInvulnerableUntil = {};
    float _lanternGauge = 0.0f;
    float _lanternMaxGauge = 250.0f;
    int   _lanternLevel = 1;
    std::array<std::chrono::steady_clock::time_point, 3> _nextAttackAllowedAt = {};
    std::array<int, 3> _pendingPlayerAttackCounts = {};
    std::array<std::chrono::steady_clock::time_point, 3> _pendingPlayerAttackExpiresAt = {};
    std::chrono::steady_clock::time_point _archerAttackSpeedBuffExpiresAt = {};
    bool _hasAcceptedMove = false;
    float _moveBudget = 0.0f;
    std::chrono::steady_clock::time_point _lastAcceptedMoveAt;
};
