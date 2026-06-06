#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "Session.h"
#include "Protocol.h"

// 전역 구조체로 선언 (Room 클래스 밖에)
struct ServerMonster
{
    int   monsterId = 0;
    int   type = 0;
    int   state = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rotY = 0.0f;
    float speed = 3.0f;
    float attackTimer = 0.0f;
    int   targetPlayerId = -1;
    int   hp = 100;
};

struct PlayerSnapshot
{
    int   playerId;
    float x, y, z;
    bool  isDead;
};

struct MonsterSnapshot
{
    int   monsterId;
    int   state;
    float x, y, z;
};

class Room
{
public:
    void Enter(std::shared_ptr<Session> session);
    void Leave(std::shared_ptr<Session> session);
    void Broadcast(void* msg, int len);
    void BroadcastExcept(std::shared_ptr<Session> excludeSession, void* msg, int len);

    void InitMonsters();
    void UpdateMonsters(float dt);
    void BroadcastMonsterSnapshots();
    void StartStage2();
    void BroadcastBossSnapshot();

    void SetHost(std::shared_ptr<Session> session);

    std::vector<PlayerSnapshot>  GetPlayerSnapshots();
    std::vector<MonsterSnapshot> GetMonsterSnapshots();
    std::shared_ptr<Session> GetHost();
    std::vector<int> GetPlayerIds();
    void SetPlayerReady(std::shared_ptr<Session> session, bool ready);
    bool CanStartGame(std::shared_ptr<Session> requester);

    bool ApplyDamageToMonster(int monsterId, int damage);
    bool SetDoorOpen(int doorId, bool isOpen);
    bool GetDoorOpen(int doorId);
    bool MarkPickupCollected(int pickupId);
    void AddLanternChargeForAll(float amount);
    void ConsumeLanternForAll();
    void ResetPlayerCombatStates();
    void SetGameStarted(bool gameStarted);
    bool CanEnter();
    int GetMonsterHp(int monsterId);
    int GetPlayerCount();

private:
    void BroadcastRoomInfoLocked();
    std::shared_ptr<Session> FindSessionByPlayerIdLocked(int playerId);
    void BroadcastPlayerHitLocked(const std::shared_ptr<Session>& targetSession);
    void BroadcastMonsterSyncLocked(const ServerMonster& monster);
    void BroadcastBossPatternLocked(int patternType, float x, float y, float z, float radius, float delay, int damage);
    int GetStage2BossLayerLocked() const;
    void UpdateStage2BossLocked(const std::vector<PlayerSnapshot>& players, float dt);

private:
    std::mutex _lock;
    std::vector<std::shared_ptr<Session>> _sessions;
    std::vector<ServerMonster>            _monsters;
    std::unordered_map<int, bool>         _doorOpenStates;
    std::unordered_set<int>               _collectedPickups;
    std::shared_ptr<Session> _host = nullptr;
    bool _gameStarted = false;
    int _currentStage = 1;
    ServerMonster _stage2Boss;
    bool _stage2BossActive = false;
    bool _stage2ShockwaveTriggered = false;
    bool _stage2MirrorTriggered = false;
    bool _stage2ShockwaveDamagePending = false;
    float _stage2ShockwaveTimer = 0.0f;
    float _stage2ShockwaveX = 0.0f;
    float _stage2ShockwaveY = 0.0f;
    float _stage2ShockwaveZ = 0.0f;
};

extern std::shared_ptr<Room> G_Room;
