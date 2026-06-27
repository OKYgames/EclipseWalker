#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include "Session.h"
#include "Protocol.h"
#include "NavigationGrid.h"

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
    std::vector<std::pair<float, float>> navigationPath;
    size_t navigationPathIndex = 0;
    float navigationTargetX = 0.0f;
    float navigationTargetZ = 0.0f;
    bool navigationUsesOtherWorld = false;
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
    bool StartStage2();
    void BroadcastBossSnapshot();

    void SetHost(std::shared_ptr<Session> session);

    std::vector<PlayerSnapshot>  GetPlayerSnapshots();
    std::vector<MonsterSnapshot> GetMonsterSnapshots();
    std::shared_ptr<Session> GetHost();
    std::vector<int> GetPlayerIds();
    void SetPlayerReady(std::shared_ptr<Session> session, bool ready);
    bool CanStartGame(std::shared_ptr<Session> requester);

    bool ApplyDamageToMonster(int monsterId, int damage, int attackerPlayerId, int* outAppliedDamage = nullptr);
    bool SetDoorOpen(int doorId, bool isOpen);
    bool GetDoorOpen(int doorId);
    bool MarkPickupCollected(int pickupId);
    void AddLanternChargeForAll(float amount);
    void ConsumeLanternForAll();
    void FillLanternForAll();
    void StartWorldShiftForAll(float durationSeconds);
    void BroadcastLanternStates();
    void BroadcastPlayerHp(const std::shared_ptr<Session>& targetSession);
    void RequestPlayerRespawn(const std::shared_ptr<Session>& targetSession);
    void HealPlayersAround(int healerPlayerId, float x, float y, float z, float radius, int amount);
    void ResetPlayerCombatStates();
    void SetGameStarted(bool gameStarted);
    bool IsCombatActive();
    bool CompleteStage2Boss();
    void FillStage2GameResultPacket(PKT_S_GAME_RESULT& outPacket);
    bool CanEnter();
    bool IsStage2();
    int GetMonsterHp(int monsterId);
    int GetPlayerCount();

private:
    void BroadcastRoomInfoLocked();
    std::shared_ptr<Session> FindSessionByPlayerIdLocked(int playerId);
    void BroadcastPlayerHitLocked(const std::shared_ptr<Session>& targetSession);
    void RespawnPlayerLocked(const std::shared_ptr<Session>& targetSession);
    void BroadcastLanternStatesLocked();
    void BroadcastMonsterSyncLocked(const ServerMonster& monster);
    void BroadcastBossPatternLocked(int patternType, float x, float y, float z, float radius, float delay, int damage, int patternData = 0);
    int GetStage2BossLayerLocked() const;
    void RecordStage2BossDamageLocked(int attackerPlayerId, int appliedDamage);
    void UpdateStage2BossLocked(const std::vector<PlayerSnapshot>& players, float dt);
    bool MoveMonsterAlongNavigationPathLocked(ServerMonster& monster, float targetX, float targetZ, float dt);

private:
    std::mutex _lock;
    std::vector<std::shared_ptr<Session>> _sessions;
    std::vector<ServerMonster>            _monsters;
    NavigationGrid                         _stage1RealNavigation;
    NavigationGrid                         _stage1OtherNavigation;
    std::unordered_map<int, bool>         _doorOpenStates;
    std::unordered_set<int>               _collectedPickups;
    std::shared_ptr<Session> _host = nullptr;
    bool _gameStarted = false;
    bool _gameFinished = false;
    int _currentStage = 1;
    ServerMonster _stage2Boss;
    bool _stage2BossActive = false;
    bool _stage2ShockwaveTriggered = false;
    bool _stage2WipeTriggered = false;
    bool _stage2MirrorTriggered = false;
    bool _stage2ShockwaveDamagePending = false;
    bool _stage2WipeDamagePending = false;
    bool _teamOtherWorld = false;
    float _stage2ShockwaveTimer = 0.0f;
    float _stage2WipeTimer = 0.0f;
    float _stage2MirrorInvulnerabilityTimer = 0.0f;
    float _teamOtherWorldTimer = 0.0f;
    float _stage2ShockwaveX = 0.0f;
    float _stage2ShockwaveY = 0.0f;
    float _stage2ShockwaveZ = 0.0f;
    int _stage2MirrorRealIndex = 0;
    std::chrono::steady_clock::time_point _stage2StartedAt{};
    float _stage2ClearTimeSeconds = 0.0f;
    std::unordered_map<int, int> _stage2BossDamageByPlayerId;
};

extern std::shared_ptr<Room> G_Room;
