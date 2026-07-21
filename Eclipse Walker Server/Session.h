#pragma once
#include "Define.h"
#include "Protocol.h"
#include "RecvBuffer.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <limits>
#include <queue>
#include <string>
#include <unordered_set>

class Room;

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
    void SetIocpKey(std::shared_ptr<Session>* key);
    void CompletePendingIo();
    void TryReleaseIocpKey();

    int   GetPlayerId() { return _playerId; }
    float GetX() { return _x; }
    float GetY() { return _y; }
    float GetZ() { return _z; }
    float GetRotY() const { return _rotY; }
    int   GetPlayerClassType() const { return _playerClassType; }
    int   GetPlayerLevel() const { return _playerLevel; }
    bool  IsReady() { return _ready; }
    void  SetReady(bool ready) { _ready = ready; }
    std::shared_ptr<Room> GetRoom() const { return _room.lock(); }
    void  SetRoom(std::shared_ptr<Room> room) { _room = room; }
    void  ClearRoom()
    {
        _room.reset();
        _ready = false;
    }
    const std::string& GetDisplayName() const { return _displayName; }
    void  SetDisplayName(const std::string& displayName) { _displayName = displayName; }
    int   GetPlayerHp() const { return _playerHp; }
    int   GetPlayerMaxHp() const { return _playerMaxHp; }
    int   GetPlayerDefense() const { return GetDefenseForClass(_playerClassType, _playerLevel, _armorTier); }
    int   GetOutgoingStatDamageBonus(int skillType) const
    {
        const int statDamage = GetAttackStatDamageForClass(_playerClassType, _playerLevel, _weaponTier);
        if (_playerClassType == 2 && skillType == 2)
        {
            return static_cast<int>(std::lround(static_cast<float>(statDamage) / 3.0f));
        }

        return statDamage;
    }
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
        _battleElixirExpiresAt = {};
    }
    void  SetPlayerStartPosition(float x, float y, float z)
    {
        _x = x;
        _y = y;
        _z = z;
        _rotY = 0.0f;
        ResetMoveValidation();
        _hasStageStartPosition = true;
        _stageStartX = x;
        _stageStartY = y;
        _stageStartZ = z;
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
    bool  ApplyPlayerDamage(int damage, bool* outDamageApplied = nullptr, bool ignoreDefense = false)
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

        if (!ignoreDefense)
        {
            damage = ApplyDefenseToIncomingDamage(damage);
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
    bool  ApplyPlayerInstantKill(bool* outDamageApplied = nullptr)
    {
        if (outDamageApplied != nullptr)
        {
            *outDamageApplied = false;
        }

        if (_playerDead)
        {
            return true;
        }

        if (outDamageApplied != nullptr)
        {
            *outDamageApplied = true;
        }

        _playerHp = 0;
        _playerDead = true;
        _playerRespawnAllowedAt = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        return true;
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
    bool  TryConsumePlayerHpCost(int amount)
    {
        if (amount <= 0 || _playerDead || _playerHp <= amount)
        {
            return false;
        }

        _playerHp = (std::max)(1, _playerHp - amount);
        return true;
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
    int   GetCurrentScene() const { return _currentScene; }
    void  SetCurrentScene(int scene)
    {
        if (scene == PLAYER_SCENE_VILLAGE ||
            scene == PLAYER_SCENE_STAGE1 ||
            scene == PLAYER_SCENE_STAGE2)
        {
            _currentScene = scene;
        }
    }
    int   GetWeaponTier() const { return _weaponTier; }
    int   GetArmorTier() const { return _armorTier; }
    void  SetWeaponTier(int tier) { _weaponTier = ClampTier(tier); }
    bool  SetArmorTier(int tier)
    {
        const int beforeHp = _playerHp;
        const int beforeMaxHp = _playerMaxHp;
        _armorTier = ClampTier(tier);
        RefreshPlayerMaxHp();
        if (!_playerDead)
        {
            if (_playerMaxHp > beforeMaxHp)
            {
                _playerHp = (std::min)(_playerMaxHp, _playerHp + (_playerMaxHp - beforeMaxHp));
            }
            else
            {
                _playerHp = (std::min)(_playerHp, _playerMaxHp);
            }
        }

        return _playerHp != beforeHp;
    }
    int   GetGold() const { return _gold; }
    void  SetGold(int gold) { _gold = (std::max)(gold, 0); }
    void  AddGold(int amount)
    {
        if (amount <= 0)
        {
            return;
        }

        const long long updated = static_cast<long long>(_gold) + static_cast<long long>(amount);
        _gold = static_cast<int>((std::min)(updated, static_cast<long long>((std::numeric_limits<int>::max)())));
    }
    bool  TrySpendGold(int amount)
    {
        if (amount <= 0)
        {
            return true;
        }
        if (_gold < amount)
        {
            return false;
        }
        _gold -= amount;
        return true;
    }
    bool  HasPurchasedShopItem(int itemId) const
    {
        return _purchasedShopItems.find(itemId) != _purchasedShopItems.end();
    }
    void  MarkPurchasedShopItem(int itemId)
    {
        _purchasedShopItems.insert(itemId);
    }
    const std::array<int, 3>& GetPotionSlots() const { return _potionSlots; }
    void  RegisterPotionPurchase(int potionType)
    {
        const int slotIndex = GetPotionSlotIndex(potionType);
        if (slotIndex < 0)
        {
            return;
        }

        if (GetPotionRank(potionType) >= GetPotionRank(_potionSlots[slotIndex]))
        {
            _potionSlots[slotIndex] = potionType;
        }
    }
    void  ResetEconomyState()
    {
        _gold = kDefaultStartingGold;
        _weaponTier = 1;
        _armorTier = 1;
        _purchasedShopItems.clear();
        _potionSlots = { POTION_TYPE_EMPTY, POTION_TYPE_EMPTY, POTION_TYPE_EMPTY };
        _nextPotionAllowedAt.fill(std::chrono::steady_clock::time_point{});
        _battleElixirExpiresAt = {};
    }
    std::array<float, 3> GetPotionCooldownsRemaining() const
    {
        std::array<float, 3> cooldowns = { 0.0f, 0.0f, 0.0f };
        const auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < cooldowns.size(); ++i)
        {
            if (now < _nextPotionAllowedAt[i])
            {
                cooldowns[i] = std::chrono::duration<float>(_nextPotionAllowedAt[i] - now).count();
            }
        }
        return cooldowns;
    }
    bool HasBattleElixirBuff() const
    {
        return std::chrono::steady_clock::now() < _battleElixirExpiresAt;
    }
    float GetBattleElixirRemainingSeconds() const
    {
        const auto now = std::chrono::steady_clock::now();
        if (now >= _battleElixirExpiresAt)
        {
            return 0.0f;
        }
        return std::chrono::duration<float>(_battleElixirExpiresAt - now).count();
    }
    float GetOutgoingDamageMultiplier() const
    {
        return HasBattleElixirBuff() ? 1.5f : 1.0f;
    }
    struct PotionUseResult
    {
        bool success = false;
        bool hpChanged = false;
        int slotIndex = -1;
        int potionType = POTION_TYPE_EMPTY;
        int remainHp = 0;
        float mpRestoreAmount = 0.0f;
        bool battleElixirActive = false;
        float battleElixirRemainingSeconds = 0.0f;
        std::array<int, 3> potionSlots = { POTION_TYPE_EMPTY, POTION_TYPE_EMPTY, POTION_TYPE_EMPTY };
        std::array<float, 3> cooldowns = { 0.0f, 0.0f, 0.0f };
    };
    PotionUseResult TryUsePotionSlot(int slotIndex)
    {
        PotionUseResult result = {};
        result.slotIndex = slotIndex;
        result.remainHp = _playerHp;
        result.potionSlots = _potionSlots;

        if (slotIndex < 0 || slotIndex >= static_cast<int>(_potionSlots.size()) || _playerDead)
        {
            result.cooldowns = GetPotionCooldownsRemaining();
            return result;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < _nextPotionAllowedAt[slotIndex])
        {
            result.potionType = _potionSlots[slotIndex];
            result.cooldowns = GetPotionCooldownsRemaining();
            return result;
        }

        const int potionType = _potionSlots[slotIndex];
        result.potionType = potionType;
        switch (potionType)
        {
        case POTION_TYPE_HP_SMALL:
            result.hpChanged = ApplyPlayerHeal(60);
            break;
        case POTION_TYPE_HP_MEDIUM:
            result.hpChanged = ApplyPlayerHeal(120);
            break;
        case POTION_TYPE_MP_SMALL:
            result.mpRestoreAmount = 40.0f;
            break;
        case POTION_TYPE_MP_MEDIUM:
            result.mpRestoreAmount = 80.0f;
            break;
        case POTION_TYPE_BATTLE_ELIXIR:
        {
            const int healthCost = static_cast<int>(std::ceil(static_cast<float>(_playerMaxHp) * 0.30f));
            if (!TryConsumePlayerHpCost(healthCost))
            {
                result.cooldowns = GetPotionCooldownsRemaining();
                return result;
            }
            result.hpChanged = true;
            _battleElixirExpiresAt = now + std::chrono::seconds(10);
            break;
        }
        case POTION_TYPE_EMPTY:
        default:
            result.cooldowns = GetPotionCooldownsRemaining();
            return result;
        }

        _nextPotionAllowedAt[slotIndex] =
            now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<float>(GetPotionCooldownSeconds(slotIndex)));

        result.success = true;
        result.remainHp = _playerHp;
        result.battleElixirActive = HasBattleElixirBuff();
        result.battleElixirRemainingSeconds = GetBattleElixirRemainingSeconds();
        result.potionSlots = _potionSlots;
        result.cooldowns = GetPotionCooldownsRemaining();
        return result;
    }
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
        RefreshPlayerMaxHp();
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
    bool  RegisterPlayerLevel(int playerLevel, bool* hpChanged = nullptr)
    {
        constexpr int kMinPlayerLevel = 1;
        constexpr int kMaxPlayerLevel = 3;
        if (hpChanged != nullptr)
        {
            *hpChanged = false;
        }

        if (playerLevel < kMinPlayerLevel || playerLevel > kMaxPlayerLevel)
        {
            return false;
        }

        const int beforeLevel = _playerLevel;
        const int beforeHp = _playerHp;
        _playerLevel = playerLevel;
        RefreshPlayerMaxHp();
        if (!_playerDead)
        {
            if (_playerLevel > beforeLevel)
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
        _hasStageStartPosition = false;
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
            if (_hasStageStartPosition)
            {
                const float startDx = x - _stageStartX;
                const float startDy = y - _stageStartY;
                const float startDz = z - _stageStartZ;
                const float startDistanceSq =
                    startDx * startDx + startDy * startDy + startDz * startDz;
                if (startDistanceSq >
                    kStageStartAcceptRadius * kStageStartAcceptRadius)
                {
                    return false;
                }

                _hasStageStartPosition = false;
            }

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
        _playerMaxHp = GetMaxHpForClass(_playerClassType, _playerLevel, _armorTier);
        _playerHp = _playerMaxHp;
        _playerDead = false;
        _playerRespawnAllowedAt = {};
        _playerRespawnInvulnerableUntil = {};
        _currentScene = PLAYER_SCENE_VILLAGE;
        ResetEconomyState();
        ResetMoveValidation();
    }

protected:
    virtual void OnConnected() {}
    virtual void OnDisconnected() {}
    virtual int  OnRecv(BYTE* buffer, int len) { return len; }
    virtual void OnSend(int len) {}

private:
    static constexpr float kRespawnInvulnerabilitySeconds = 5.0f;
    static constexpr float kStageStartAcceptRadius = 6.0f;
    static constexpr int kDefaultStartingGold = 10000;

    static int GetBaseMaxHpForClass(int classType)
    {
        switch (classType)
        {
        case 0: return 500;
        case 1: return 200;
        case 2: return 250;
        default: return 200;
        }
    }

    static int GetMaxHpForClass(int classType, int playerLevel, int armorTier)
    {
        const int levelBonus = ((std::max)(playerLevel, 1) - 1) * 35;
        return GetBaseMaxHpForClass(classType) + levelBonus + GetArmorMaxHpBonus(classType, armorTier);
    }

    static int GetArmorMaxHpBonus(int classType, int armorTier)
    {
        const int bonusLevel = (std::max)(ClampTier(armorTier) - 1, 0);
        switch (classType)
        {
        case 0: return bonusLevel * 60;
        case 1: return bonusLevel * 30;
        case 2: return bonusLevel * 40;
        default: return 0;
        }
    }

    static int GetDefenseForClass(int classType, int playerLevel, int armorTier)
    {
        int defense = 8;
        switch (classType)
        {
        case 0: defense = 18; break;
        case 1: defense = 6; break;
        case 2: defense = 10; break;
        default: defense = 8; break;
        }

        defense += ((std::max)(playerLevel, 1) - 1) * 3;

        const int bonusLevel = (std::max)(ClampTier(armorTier) - 1, 0);
        switch (classType)
        {
        case 0: defense += bonusLevel * 8; break;
        case 1: defense += bonusLevel * 4; break;
        case 2: defense += bonusLevel * 6; break;
        default: break;
        }

        return defense;
    }

    static int GetAttackStatDamageForClass(int classType, int playerLevel, int weaponTier)
    {
        const int levelBonus = (std::max)(playerLevel, 1) - 1;
        const int weaponBonusLevel = (std::max)(ClampTier(weaponTier) - 1, 0);
        switch (classType)
        {
        case 0:
            return 16 + levelBonus * 5 + weaponBonusLevel * 8;
        case 1:
            return 24 + levelBonus * 6 + weaponBonusLevel * 10;
        case 2:
            return 14 + levelBonus * 5 + weaponBonusLevel * 7;
        default:
            return 10 + levelBonus * 5;
        }
    }

    static int ApplyDefenseToIncomingDamage(int damage, int defense)
    {
        if (damage <= 0)
        {
            return 0;
        }

        const float damageScale = 100.0f / (100.0f + static_cast<float>((std::max)(defense, 0)) * 5.0f);
        return (std::max)(1, static_cast<int>(std::lround(static_cast<float>(damage) * damageScale)));
    }

    int ApplyDefenseToIncomingDamage(int damage) const
    {
        return ApplyDefenseToIncomingDamage(damage, GetPlayerDefense());
    }

    void RefreshPlayerMaxHp()
    {
        _playerMaxHp = GetMaxHpForClass(_playerClassType, _playerLevel, _armorTier);
    }

    static int ClampTier(int tier)
    {
        return (std::max)(1, (std::min)(tier, 3));
    }

    static int GetPotionSlotIndex(int potionType)
    {
        switch (potionType)
        {
        case POTION_TYPE_HP_SMALL:
        case POTION_TYPE_HP_MEDIUM:
            return 0;
        case POTION_TYPE_MP_SMALL:
        case POTION_TYPE_MP_MEDIUM:
            return 1;
        case POTION_TYPE_BATTLE_ELIXIR:
            return 2;
        default:
            return -1;
        }
    }

    static int GetPotionRank(int potionType)
    {
        switch (potionType)
        {
        case POTION_TYPE_HP_MEDIUM:
        case POTION_TYPE_MP_MEDIUM:
            return 2;
        case POTION_TYPE_HP_SMALL:
        case POTION_TYPE_MP_SMALL:
        case POTION_TYPE_BATTLE_ELIXIR:
            return 1;
        default:
            return 0;
        }
    }

    static float GetPotionCooldownSeconds(int slotIndex)
    {
        switch (slotIndex)
        {
        case 0:
        case 1:
            return 10.0f;
        case 2:
            return 30.0f;
        default:
            return 0.0f;
        }
    }

    void HandleRecv(int numOfBytes);
    void HandleSend(int numOfBytes);
    void RegisterSend();

private:
    SOCKET      _socket;
    SOCKADDR_IN _addr;
    std::shared_ptr<Session>* _iocpKey = nullptr;
    std::atomic<int> _pendingIoCount = 0;
    std::atomic<bool> _iocpKeyReleased = false;

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
    int   _currentScene = PLAYER_SCENE_VILLAGE;
    int   _weaponTier = 1;
    int   _armorTier = 1;
    bool  _ready = false;
    std::weak_ptr<Room> _room;
    std::string _displayName;
    int   _playerMaxHp = 200;
    int   _playerHp = 200;
    bool  _playerDead = false;
    std::chrono::steady_clock::time_point _playerRespawnAllowedAt = {};
    std::chrono::steady_clock::time_point _playerRespawnInvulnerableUntil = {};
    float _lanternGauge = 0.0f;
    float _lanternMaxGauge = 250.0f;
    int   _lanternLevel = 1;
    int   _gold = kDefaultStartingGold;
    std::unordered_set<int> _purchasedShopItems;
    std::array<int, 3> _potionSlots = { POTION_TYPE_EMPTY, POTION_TYPE_EMPTY, POTION_TYPE_EMPTY };
    std::array<std::chrono::steady_clock::time_point, 3> _nextPotionAllowedAt = {};
    std::array<std::chrono::steady_clock::time_point, 3> _nextAttackAllowedAt = {};
    std::array<int, 3> _pendingPlayerAttackCounts = {};
    std::array<std::chrono::steady_clock::time_point, 3> _pendingPlayerAttackExpiresAt = {};
    std::chrono::steady_clock::time_point _archerAttackSpeedBuffExpiresAt = {};
    std::chrono::steady_clock::time_point _battleElixirExpiresAt = {};
    bool _hasAcceptedMove = false;
    float _moveBudget = 0.0f;
    std::chrono::steady_clock::time_point _lastAcceptedMoveAt;
    bool _hasStageStartPosition = false;
    float _stageStartX = 0.0f;
    float _stageStartY = 0.0f;
    float _stageStartZ = 0.0f;
};
