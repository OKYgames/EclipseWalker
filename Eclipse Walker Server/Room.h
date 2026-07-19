#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <map>
#include <string>
#include "Session.h"
#include "Protocol.h"
#include "NavigationGrid.h"

// 전역 구조체로 선언 (Room 클래스 밖에)
struct ServerMonster
{
    int   monsterId = 0;
    int   type = 0;
    int   state = 0;
    int   attackSequence = 0;
    int   attackType = BOSS_ATTACK_NONE;
    int   actionPhase = BOSS_PHASE_IDLE;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float rotY = 0.0f;
    float speed = 3.0f;
    float attackTimer = 0.0f;
    float actionTimer = 0.0f;
    float pendingDamageTimer = 0.0f;
    int   targetPlayerId = -1;
    int   pendingAttackTargetId = -1;
    int   lastAttackType = BOSS_ATTACK_NONE;
    int   lastTargetPlayerId = -1;
    int   hp = 100;
    int   maxHp = 100;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
    float spawnZ = 0.0f;
    float respawnTimer = 0.0f;
    bool  respawnEnabled = false;
    std::vector<std::pair<float, float>> navigationPath;
    size_t navigationPathIndex = 0;
    float navigationTargetX = 0.0f;
    float navigationTargetZ = 0.0f;
    bool navigationUsesOtherWorld = false;
    bool  pendingAttackDamage = false;
    float targetStickTimer = 0.0f;
};

struct PlayerSnapshot
{
    int   playerId;
    float x, y, z;
    bool  isDead;
    int   currentScene;
    int   hp;
    int   maxHp;
};

struct ServerMonsterArrow
{
    int   monsterId = 0;
    int   monsterType = 0;
    float startX = 0.0f;
    float startY = 0.0f;
    float startZ = 0.0f;
    float dirX = 0.0f;
    float dirZ = 0.0f;
    float age = 0.0f;
    float startDelay = 0.0f;
    float travelDistance = 0.0f;
    float motionDuration = 0.0f;
};

struct MonsterSnapshot
{
    int   monsterId;
    int   type;
    int   state;
    float x, y, z;
};

class Room : public std::enable_shared_from_this<Room>
{
public:
    void Configure(int roomId, const std::string& title);
    bool Enter(std::shared_ptr<Session> session);
    void Leave(std::shared_ptr<Session> session);
    void Broadcast(void* msg, int len);
    void BroadcastExcept(std::shared_ptr<Session> excludeSession, void* msg, int len);

    void InitMonsters();
    void UpdateMonsters(float dt);
    void BroadcastMonsterSnapshots();
    bool StartStage2();
    float GetStage2ElapsedSeconds();
    void BroadcastBossSnapshot();

    void SetHost(std::shared_ptr<Session> session);

    std::vector<PlayerSnapshot>  GetPlayerSnapshots();
    std::vector<MonsterSnapshot> GetMonsterSnapshots();
    std::shared_ptr<Session> GetHost();
    std::vector<int> GetPlayerIds();
    void SetPlayerReady(std::shared_ptr<Session> session, bool ready);
    bool CanStartGame(std::shared_ptr<Session> requester);

    bool ApplyDamageToMonster(int monsterId, int damage, int attackerPlayerId, int* outAppliedDamage = nullptr);
    bool ApplyMonsterKnockback(int monsterId, float sourceX, float sourceZ, float fallbackRotY, float distance);
    bool SetDoorOpen(int doorId, bool isOpen);
    bool GetDoorOpen(int doorId);
    bool MarkPickupCollected(int pickupId);
    bool TryCollectGoldPickup(const std::shared_ptr<Session>& session, int pickupGroupId, float x, float y, float z, float radius);
    bool MoveAllPlayersFromVillagePortalToStage1(const std::shared_ptr<Session>& triggerSession);
    bool MovePlayerToVillage(const std::shared_ptr<Session>& session);
    void AddLanternChargeForAll(float amount);
    void ConsumeLanternForAll();
    void StartWorldShiftForAll(float durationSeconds);
    void BroadcastLanternStates();
    void BroadcastPlayerHp(const std::shared_ptr<Session>& targetSession);
    void SendGoldUpdate(const std::shared_ptr<Session>& targetSession, int pickupGroupId = 0, bool pickupCollected = false);
    void RequestPlayerRespawn(const std::shared_ptr<Session>& targetSession);
    void HealPlayersAround(int healerPlayerId, float x, float y, float z, float radius, int amount);
    void ResetPlayerCombatStates();
    void ApplyStage1StartPositions();
    void SetGameStarted(bool gameStarted);
    bool IsCombatActive();
    bool CompleteStage2Boss();
    void FillStage2GameResultPacket(PKT_S_GAME_RESULT& outPacket);
    bool IsStage2BossIntroCutsceneActive();
    void TryTriggerStage2BossIntroCutscene(int triggerPlayerId, float x, float y, float z);
    bool CanEnter();
    bool IsStage2();
    int GetMonsterHp(int monsterId);
    int GetPlayerCount();
    int GetRoomId();
    std::string GetTitle();
    bool IsInGame();
    RoomListEntry GetListEntry();

private:
    void BroadcastRoomInfoLocked();
    std::shared_ptr<Session> FindSessionByPlayerIdLocked(int playerId);
    size_t GetPlayerSlotIndexLocked(const std::shared_ptr<Session>& session) const;
    void SendPlayerMoveSnapshotLocked(const std::shared_ptr<Session>& receiver, const std::shared_ptr<Session>& subject);
    void BroadcastPlayerMoveSnapshotLocked(const std::shared_ptr<Session>& subject);
    void SendScenePlayerSnapshotsLocked(const std::shared_ptr<Session>& receiver);
    void BroadcastAllPlayerMoveSnapshotsLocked();
    void BroadcastPlayerHitLocked(const std::shared_ptr<Session>& targetSession, bool wasImmune = false);
    void RespawnPlayerLocked(const std::shared_ptr<Session>& targetSession);
    void BroadcastLanternStatesLocked();
    void BroadcastMonsterSyncLocked(const ServerMonster& monster);
    void BroadcastBossPatternLocked(int patternType, float x, float y, float z, float radius, float delay, int damage, int patternData = 0);
    void BroadcastStage2BossIntroCutsceneLocked(int triggerPlayerId);
    int GetStage2BossLayerLocked() const;
    void RecordStage2BossDamageLocked(int attackerPlayerId, int appliedDamage);
    void UpdateStage2BossLocked(const std::vector<PlayerSnapshot>& players, float dt);
    const NavigationGrid& GetActiveMonsterNavigationLocked() const;
    bool MoveMonsterAlongNavigationPathLocked(ServerMonster& monster, float targetX, float targetZ, float dt);
    void SpawnMonsterArrowLocked(const ServerMonster& monster);
    void UpdateMonsterArrowsLocked(const std::vector<PlayerSnapshot>& players, float dt);

private:
    std::mutex _lock;
    int _roomId = 0;
    std::string _title = "Room";
    std::vector<std::shared_ptr<Session>> _sessions;
    std::vector<ServerMonster>            _monsters;
    std::vector<ServerMonsterArrow>       _monsterArrows;
    NavigationGrid                         _stage1RealNavigation;
    NavigationGrid                         _stage1OtherNavigation;
    NavigationGrid                         _stage2Navigation;
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
    bool _stage2MirrorPatternActive = false;
    bool _stage2ShockwaveDamagePending = false;
    bool _stage2WipeDamagePending = false;
    bool _teamOtherWorld = false;
    float _stage2ShockwaveTimer = 0.0f;
    float _stage2WipeTimer = 0.0f;
    float _stage2MirrorInvulnerabilityTimer = 0.0f;
    float _stage2MirrorRecoveryTimer = 0.0f;
    float _stage2MirrorHealTimer = 0.0f;
    float _teamOtherWorldTimer = 0.0f;
    float _stage2BossIntroCutsceneRemaining = 0.0f;
    float _stage2ShockwaveX = 0.0f;
    float _stage2ShockwaveY = 0.0f;
    float _stage2ShockwaveZ = 0.0f;
    int _stage2MirrorRealIndex = 0;
    std::chrono::steady_clock::time_point _stage2StartedAt{};
    float _stage2ClearTimeSeconds = 0.0f;
    std::unordered_map<int, int> _stage2BossDamageByPlayerId;
    bool _stage2BossIntroCutscenePlayed = false;
};

class RoomManager
{
public:
    std::shared_ptr<Room> CreateRoom(const std::string& title);
    bool JoinRoom(int roomId, std::shared_ptr<Session> session);
    void LeaveCurrentRoom(std::shared_ptr<Session> session);
    std::vector<RoomListEntry> GetRoomList();
    void UpdateRooms(float dt);

private:
    void RemoveEmptyRoomsLocked();

private:
    std::mutex _lock;
    int _nextRoomId = 1;
    std::map<int, std::shared_ptr<Room>> _rooms;
};

extern std::shared_ptr<Room> G_Room;
extern std::shared_ptr<RoomManager> G_RoomManager;
