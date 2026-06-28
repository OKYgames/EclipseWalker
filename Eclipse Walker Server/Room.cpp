#include "Room.h"
#include "Protocol.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstring>

std::shared_ptr<Room> G_Room = std::make_shared<Room>();

namespace
{
    constexpr bool kAllowSoloLobbyStart = true;
    constexpr int kMonsterAttackDamage = 10;
    constexpr int kStage2BossMaxHp = 1200;
    constexpr int kStage2BossDamagePerHit = 60;
    constexpr int kStage2BossAttackDamage = 15;
    constexpr int kStage2ShockwaveDamage = 35;
    constexpr int kStage2WipeDamage = 200;
    constexpr int kStage2ShockwaveLayer = 150;
    constexpr int kStage2WipeLayer = 100;
    constexpr int kStage2MirrorLayer = 50;
    constexpr float kStage2BossSpawnX = -8.81673f;
    constexpr float kStage2BossSpawnY = 7.71219f;
    constexpr float kStage2BossSpawnZ = 23.2462f;
    constexpr float kStage2BossDetectRange = 22.0f;
    constexpr float kStage2BossAttackRange = 4.0f;
    constexpr float kStage2BossAttackCooldownSeconds = 2.4f;
    constexpr float kStage2ShockwaveRadius = 5.0f;
    constexpr float kStage2ShockwaveDelay = 2.0f;
    constexpr float kStage2WipeDelay = 5.0f;
    constexpr float kStage2MirrorInvulnerabilityDelay = 1.18f;
    constexpr int kStage2MirrorSlotCount = 3;
    constexpr float kStage1PlayerRespawnX = 1.0f;
    constexpr float kStage1PlayerRespawnY = 5.0f;
    constexpr float kStage1PlayerRespawnZ = 0.0f;
    constexpr float kStage2PlayerRespawnX = -4.81673f;
    constexpr float kStage2PlayerRespawnY = 6.01219f;
    constexpr float kStage2PlayerRespawnZ = 23.2462f;
    constexpr int kStage2SkeletonSpawnBaseId = 1101;
    constexpr int kSpectralArcherMonsterType = 4;
    constexpr int kSpectralImpMonsterType = 5;
    constexpr float kPi = 3.14159265f;
    constexpr float kRealSkeletonArcherAttackRange = 10.5f;
    constexpr float kSpectralArcherAttackRange = 10.0f;
    constexpr float kSkeletonArcherReleaseFraction = 0.70f;
    constexpr float kImpArcherReleaseFraction = 0.56f;
    constexpr float kSkeletonArcherArrowHeight = 0.30f;
    constexpr float kImpArcherArrowHeight = 0.18f;
    constexpr float kSkeletonArcherArrowRightOffset = 0.10f;
    constexpr float kImpArcherArrowRightOffset = -0.05f;
    constexpr float kMonsterArrowStartForwardOffset = 0.8f;
    constexpr float kMonsterArrowExtraTravelDistance = 1.5f;
    constexpr float kMonsterArrowSpeed = 20.0f;
    constexpr float kMonsterArrowMinDistance = 3.0f;
    constexpr float kMonsterArrowMaxDistance = 30.0f;
    constexpr float kMonsterArrowLifePaddingSeconds = 0.10f;
    constexpr float kMonsterArrowVisualSyncDelaySeconds = 0.18f;
    constexpr float kMonsterArrowDamageSampleBacktrackSeconds = 0.05f;
    constexpr float kMonsterArrowPlayerHitRadius = 0.15f;
    constexpr float kMonsterArrowPlayerHitHalfHeight = 0.65f;

    struct MonsterArrowPosition
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    bool IsOtherWorldMonsterType(int type)
    {
        return type == kSpectralArcherMonsterType ||
            type == kSpectralImpMonsterType;
    }

    bool IsRangedMonsterType(int type)
    {
        return type == 0 || type == kSpectralArcherMonsterType;
    }

    float ClampFloat(float value, float minValue, float maxValue)
    {
        return (std::max)(minValue, (std::min)(value, maxValue));
    }

    float GetRangedMonsterVisualAttackRange(int type)
    {
        return type == kSpectralArcherMonsterType
            ? kSpectralArcherAttackRange
            : kRealSkeletonArcherAttackRange;
    }

    float GetMonsterArrowReleaseDelay(int type)
    {
        const float releaseDelay = type == kSpectralArcherMonsterType
            ? kImpArcherReleaseFraction
            : kSkeletonArcherReleaseFraction;
        return releaseDelay + kMonsterArrowVisualSyncDelaySeconds;
    }

    float GetMonsterArrowStartHeight(int type)
    {
        return type == kSpectralArcherMonsterType
            ? kImpArcherArrowHeight
            : kSkeletonArcherArrowHeight;
    }

    float GetMonsterArrowStartRightOffset(int type)
    {
        return type == kSpectralArcherMonsterType
            ? kImpArcherArrowRightOffset
            : kSkeletonArcherArrowRightOffset;
    }

    float EaseOutQuart(float t)
    {
        t = ClampFloat(t, 0.0f, 1.0f);
        const float inv = 1.0f - t;
        return 1.0f - inv * inv * inv * inv;
    }

    MonsterArrowPosition GetMonsterArrowPositionAt(const ServerMonsterArrow& arrow, float age)
    {
        if (age <= arrow.startDelay)
        {
            return { arrow.startX, arrow.startY, arrow.startZ };
        }

        const float arrowAge = age - arrow.startDelay;
        const float t = arrow.motionDuration > 0.0f
            ? ClampFloat(arrowAge / arrow.motionDuration, 0.0f, 1.0f)
            : 1.0f;
        const float distance = arrow.travelDistance * EaseOutQuart(t);
        return
        {
            arrow.startX + arrow.dirX * distance,
            arrow.startY,
            arrow.startZ + arrow.dirZ * distance
        };
    }

    float DistancePointToSegmentSqXZ(float px, float pz, const MonsterArrowPosition& a, const MonsterArrowPosition& b)
    {
        const float abX = b.x - a.x;
        const float abZ = b.z - a.z;
        const float lenSq = abX * abX + abZ * abZ;
        if (lenSq <= 0.000001f)
        {
            const float dx = px - a.x;
            const float dz = pz - a.z;
            return dx * dx + dz * dz;
        }

        float t = ((px - a.x) * abX + (pz - a.z) * abZ) / lenSq;
        t = ClampFloat(t, 0.0f, 1.0f);

        const float closestX = a.x + abX * t;
        const float closestZ = a.z + abZ * t;
        const float dx = px - closestX;
        const float dz = pz - closestZ;
        return dx * dx + dz * dz;
    }

    bool DoesMonsterArrowHitPlayer(
        const PlayerSnapshot& player,
        const MonsterArrowPosition& previousPosition,
        const MonsterArrowPosition& currentPosition)
    {
        if (player.isDead)
        {
            return false;
        }

        const float arrowMinY = (std::min)(previousPosition.y, currentPosition.y);
        const float arrowMaxY = (std::max)(previousPosition.y, currentPosition.y);
        const float playerMinY = player.y - kMonsterArrowPlayerHitHalfHeight;
        const float playerMaxY = player.y + kMonsterArrowPlayerHitHalfHeight;
        if (arrowMaxY < playerMinY || arrowMinY > playerMaxY)
        {
            return false;
        }

        const float hitRadiusSq = kMonsterArrowPlayerHitRadius * kMonsterArrowPlayerHitRadius;
        return DistancePointToSegmentSqXZ(player.x, player.z, previousPosition, currentPosition) <= hitRadiusSq;
    }

    int MakeTemporaryPlayerId(const std::shared_ptr<Session>& session)
    {
        return static_cast<int>(reinterpret_cast<intptr_t>(session.get()) & 0x7FFFFFFF);
    }
}

void Room::Enter(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (session == nullptr)
    {
        return;
    }

    if (std::find(_sessions.begin(), _sessions.end(), session) != _sessions.end())
    {
        BroadcastRoomInfoLocked();
        return;
    }

    if (_sessions.size() >= MAX_LOBBY_PLAYERS)
    {
        PKT_S_LOGIN loginPkt = {};
        loginPkt.header.size = sizeof(PKT_S_LOGIN);
        loginPkt.header.id = PacketID::S_LOGIN;
        loginPkt.success = false;
        loginPkt.myPlayerId = 0;
        session->Send(&loginPkt, sizeof(loginPkt));
        return;
    }

    const int requestedPlayerId = session->GetPlayerId();
    if (requestedPlayerId > 0)
    {
        for (const auto& other : _sessions)
        {
            if (other != nullptr && other->GetPlayerId() == requestedPlayerId)
            {
                PKT_S_LOGIN loginPkt = {};
                loginPkt.header.size = sizeof(PKT_S_LOGIN);
                loginPkt.header.id = PacketID::S_LOGIN;
                loginPkt.success = false;
                loginPkt.myPlayerId = 0;
                session->Send(&loginPkt, sizeof(loginPkt));
                return;
            }
        }
    }

    if (session->GetPlayerId() <= 0)
    {
        session->SetPlayerInfo(MakeTemporaryPlayerId(session), 0.0f, 0.0f, 0.0f);
    }
    session->SetReady(false);
    session->ResetPlayerCombatState();
    session->ResetLanternState();

    _sessions.push_back(session);

    if (_host == nullptr)
    {
        _host = session;
    }

    PKT_S_LOGIN loginPkt = {};
    loginPkt.header.size = sizeof(PKT_S_LOGIN);
    loginPkt.header.id = PacketID::S_LOGIN;
    loginPkt.success = true;
    loginPkt.myPlayerId = session->GetPlayerId();
    session->Send(&loginPkt, sizeof(loginPkt));

    PKT_S_PLAYER_ENTER enterPkt = {};
    enterPkt.header.size = sizeof(PKT_S_PLAYER_ENTER);
    enterPkt.header.id = PacketID::S_PLAYER_ENTER;
    enterPkt.playerId = session->GetPlayerId();

    for (auto& other : _sessions)
    {
        if (other != nullptr && other != session)
        {
            other->Send(&enterPkt, sizeof(enterPkt));
        }
    }

    BroadcastRoomInfoLocked();
}

void Room::Leave(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_lock);
    const int leavingPlayerId = (session != nullptr) ? session->GetPlayerId() : -1;

    auto it = std::remove(_sessions.begin(), _sessions.end(), session);
    _sessions.erase(it, _sessions.end());
    if (_sessions.empty())
    {
        _gameStarted = false;
        _gameFinished = false;
        _currentStage = 1;
        _monsters.clear();
        _doorOpenStates.clear();
        _collectedPickups.clear();
        _stage2Boss = {};
        _stage2BossActive = false;
        _stage2ShockwaveTriggered = false;
        _stage2WipeTriggered = false;
        _stage2MirrorTriggered = false;
        _stage2ShockwaveDamagePending = false;
        _stage2WipeDamagePending = false;
        _stage2ShockwaveTimer = 0.0f;
        _stage2WipeTimer = 0.0f;
        _stage2MirrorInvulnerabilityTimer = 0.0f;
        _teamOtherWorld = false;
        _teamOtherWorldTimer = 0.0f;
    }

    if (_host == session)
    {
        _host = _sessions.empty() ? nullptr : _sessions.front();
    }

    if (leavingPlayerId > 0)
    {
        PKT_S_PLAYER_LEAVE leavePkt = {};
        leavePkt.header.size = sizeof(PKT_S_PLAYER_LEAVE);
        leavePkt.header.id = PacketID::S_PLAYER_LEAVE;
        leavePkt.playerId = leavingPlayerId;

        for (auto& other : _sessions)
        {
            if (other != nullptr)
            {
                other->Send(&leavePkt, sizeof(leavePkt));
            }
        }
    }

    BroadcastRoomInfoLocked();
}

void Room::Broadcast(void* msg, int len)
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& session : _sessions)
        if (session != nullptr)
            session->Send(msg, len);
}

void Room::BroadcastExcept(std::shared_ptr<Session> excludeSession, void* msg, int len)
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& session : _sessions)
        if (session != nullptr && session != excludeSession)
            session->Send(msg, len);
}

void Room::InitMonsters()
{
    std::lock_guard<std::mutex> lock(_lock);
    if (!_stage1RealNavigation.IsReady())
    {
        _stage1RealNavigation.Load(
            "Stage1Map/RealFloorCollider.fbx",
            "Stage1Map/RealWallCollider.fbx");
    }
    if (!_stage1OtherNavigation.IsReady())
    {
        _stage1OtherNavigation.Load(
            "Stage1Map/OtherFloorCollider.fbx",
            "Stage1Map/OtherWallCollider.fbx");
    }

    _monsters.clear();
    _monsterArrows.clear();
    _doorOpenStates.clear();
    _collectedPickups.clear();
    _currentStage = 1;
    _gameFinished = false;
    _stage2BossActive = false;
    _stage2ShockwaveTriggered = false;
    _stage2WipeTriggered = false;
    _stage2MirrorTriggered = false;
    _stage2ShockwaveDamagePending = false;
    _stage2WipeDamagePending = false;
    _stage2WipeTimer = 0.0f;
    _stage2MirrorInvulnerabilityTimer = 0.0f;
    _stage2MirrorRealIndex = 0;
    _teamOtherWorld = false;
    _teamOtherWorldTimer = 0.0f;
    _stage2ShockwaveTimer = 0.0f;

    struct MonsterSpawn
    {
        int id;
        int type;
        float x;
        float y;
        float z;
    };

    const MonsterSpawn monsterSpawns[] =
    {
        { 1, 2, 7.25678f, 0.407884f, -3.65645f },
        { 2, 0, -2.50433f, 0.407884f, -1.72859f },
        { 3, 2, 1.67656f, 0.407884f, 1.17098f },
        { 4, 0, 4.34725f, 0.407884f, 1.92283f },
        { 5, 2, 0.274773f, -2.33052f, 23.6689f },
        { 6, 0, 5.1849f, -2.33052f, 23.7464f },
        { 7, 2, 16.9976f, -2.22412f, 9.39922f },
        { 8, 0, 17.2824f, -2.22412f, 16.5349f },
        { 9, 2, 17.3924f, -2.22412f, 22.6391f },
        { 10, 0, 16.7717f, -2.22412f, 26.8362f },
        { 11, 2, -20.1836f, -3.79212f, 27.992f },
        { 12, 0, -24.1076f, -3.79212f, 24.2108f },
        { 13, kSpectralImpMonsterType, -26.1271f, -2.35852f, 7.28663f },
        { 14, kSpectralArcherMonsterType, -26.4611f, -2.35852f, 9.10912f },
        { 15, kSpectralImpMonsterType, -22.9359f, -2.35852f, 5.91600f },
        { 16, kSpectralArcherMonsterType, -22.7634f, -2.35852f, 11.3304f },
        { 17, kSpectralImpMonsterType, -19.4180f, -2.35852f, 5.46392f },
        { 18, kSpectralImpMonsterType, 7.25678f, 0.407884f, -3.65645f },
        { 19, kSpectralArcherMonsterType, -2.50433f, 0.407884f, -1.72859f },
    };

    for (const MonsterSpawn& spawn : monsterSpawns)
    {
        ServerMonster monster;
        monster.monsterId = spawn.id;
        monster.type = spawn.type;
        monster.state = 0;
        monster.x = spawn.x;
        monster.y = spawn.y;
        monster.z = spawn.z;
        monster.rotY = 0.0f;
        monster.speed = spawn.type == kSpectralImpMonsterType ? 6.0f :
            spawn.type == kSpectralArcherMonsterType ? 3.4f : 3.0f;
        monster.hp = spawn.type == kSpectralArcherMonsterType ? 110 : 100;
        _monsters.push_back(monster);
    }
}

void Room::BroadcastMonsterSnapshots()
{
    std::lock_guard<std::mutex> lock(_lock);

    if (_currentStage == 2 && _stage2BossActive)
    {
        BroadcastMonsterSyncLocked(_stage2Boss);
    }

    for (const auto& m : _monsters)
    {
        BroadcastMonsterSyncLocked(m);
    }
}

std::vector<PlayerSnapshot> Room::GetPlayerSnapshots()
{
    std::lock_guard<std::mutex> lock(_lock);
    std::vector<PlayerSnapshot> result;
    for (auto& s : _sessions)
    {
        if (s == nullptr) continue;
        PlayerSnapshot snap;
        snap.playerId = s->GetPlayerId();
        snap.x = s->GetX();
        snap.y = s->GetY();
        snap.z = s->GetZ();
        snap.isDead = s->IsPlayerDead();
        result.push_back(snap);
    }
    return result;
}

std::shared_ptr<Session> Room::FindSessionByPlayerIdLocked(int playerId)
{
    for (auto& session : _sessions)
    {
        if (session != nullptr && session->GetPlayerId() == playerId)
        {
            return session;
        }
    }

    return nullptr;
}

void Room::BroadcastPlayerHitLocked(const std::shared_ptr<Session>& targetSession)
{
    if (targetSession == nullptr)
    {
        return;
    }

    PKT_S_PLAYER_HIT hitPkt = {};
    hitPkt.header.size = sizeof(PKT_S_PLAYER_HIT);
    hitPkt.header.id = PacketID::S_PLAYER_HIT;
    hitPkt.playerId = targetSession->GetPlayerId();
    hitPkt.remainHp = targetSession->GetPlayerHp();
    hitPkt.isDead = targetSession->IsPlayerDead();

    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->Send(&hitPkt, sizeof(hitPkt));
        }
    }
}

void Room::BroadcastLanternStatesLocked()
{
    for (auto& playerSession : _sessions)
    {
        if (playerSession == nullptr)
        {
            continue;
        }

        PKT_S_LANTERN_GAUGE lanternPkt = {};
        lanternPkt.header.size = sizeof(PKT_S_LANTERN_GAUGE);
        lanternPkt.header.id = PacketID::S_LANTERN_GAUGE;
        lanternPkt.playerId = playerSession->GetPlayerId();
        lanternPkt.gauge = playerSession->GetLanternGauge();
        lanternPkt.maxGauge = playerSession->GetLanternMaxGauge();
        lanternPkt.level = playerSession->GetLanternLevel();

        for (auto& receiver : _sessions)
        {
            if (receiver != nullptr)
            {
                receiver->Send(&lanternPkt, sizeof(lanternPkt));
            }
        }
    }
}

void Room::RespawnPlayerLocked(const std::shared_ptr<Session>& targetSession)
{
    if (targetSession == nullptr)
    {
        return;
    }

    const bool isStage2 = _currentStage == 2;
    float respawnX = targetSession->GetX();
    float respawnY = targetSession->GetY();
    float respawnZ = targetSession->GetZ();
    if (!std::isfinite(respawnX) || !std::isfinite(respawnY) || !std::isfinite(respawnZ))
    {
        respawnX = isStage2 ? kStage2PlayerRespawnX : kStage1PlayerRespawnX;
        respawnY = isStage2 ? kStage2PlayerRespawnY : kStage1PlayerRespawnY;
        respawnZ = isStage2 ? kStage2PlayerRespawnZ : kStage1PlayerRespawnZ;
    }

    targetSession->RespawnPlayer(respawnX, respawnY, respawnZ);

    PKT_S_PLAYER_RESPAWN respawnPkt = {};
    respawnPkt.header.size = sizeof(PKT_S_PLAYER_RESPAWN);
    respawnPkt.header.id = PacketID::S_PLAYER_RESPAWN;
    respawnPkt.playerId = targetSession->GetPlayerId();
    respawnPkt.x = respawnX;
    respawnPkt.y = respawnY;
    respawnPkt.z = respawnZ;
    respawnPkt.remainHp = targetSession->GetPlayerHp();
    respawnPkt.classType = targetSession->GetPlayerClassType();
    respawnPkt.playerLevel = targetSession->GetPlayerLevel();

    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->Send(&respawnPkt, sizeof(respawnPkt));
        }
    }
}

void Room::BroadcastMonsterSyncLocked(const ServerMonster& monster)
{
    PKT_S_MONSTER_SYNC syncPkt = {};
    syncPkt.header.size = sizeof(PKT_S_MONSTER_SYNC);
    syncPkt.header.id = PacketID::S_MONSTER_SYNC;
    syncPkt.monsterId = monster.monsterId;
    syncPkt.monsterType = monster.type;
    syncPkt.state = monster.state;
    syncPkt.attackSequence = monster.attackSequence;
    syncPkt.x = monster.x;
    syncPkt.y = monster.y;
    syncPkt.z = monster.z;
    syncPkt.rotY = monster.rotY;
    syncPkt.remainHp = monster.hp;
    syncPkt.isDead = (monster.state == 3);

    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->Send(&syncPkt, sizeof(syncPkt));
        }
    }
}

void Room::BroadcastBossPatternLocked(int patternType, float x, float y, float z, float radius, float delay, int damage, int patternData)
{
    PKT_S_BOSS_PATTERN patternPkt = {};
    patternPkt.header.size = sizeof(PKT_S_BOSS_PATTERN);
    patternPkt.header.id = PacketID::S_BOSS_PATTERN;
    patternPkt.patternType = patternType;
    patternPkt.x = x;
    patternPkt.y = y;
    patternPkt.z = z;
    patternPkt.radius = radius;
    patternPkt.delay = delay;
    patternPkt.damage = damage;
    patternPkt.patternData = patternData;

    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->Send(&patternPkt, sizeof(patternPkt));
        }
    }
}

int Room::GetStage2BossLayerLocked() const
{
    if (!_stage2BossActive || _stage2Boss.hp <= 0)
    {
        return 0;
    }

    constexpr int bossHpLayerCount = 200;
    const float hpPerLayer = static_cast<float>(kStage2BossMaxHp) / static_cast<float>(bossHpLayerCount);
    int layer = static_cast<int>(std::ceil(static_cast<float>(_stage2Boss.hp) / hpPerLayer));
    if (layer < 1)
    {
        layer = 1;
    }
    if (layer > bossHpLayerCount)
    {
        layer = bossHpLayerCount;
    }
    return layer;
}

void Room::ResetPlayerCombatStates()
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->ResetPlayerCombatState();
            session->ResetMoveValidation();
            session->ResetLanternState();
            BroadcastPlayerHitLocked(session);
        }
    }

    BroadcastLanternStatesLocked();
}

void Room::SetGameStarted(bool gameStarted)
{
    std::lock_guard<std::mutex> lock(_lock);
    _gameStarted = gameStarted;
}

bool Room::IsCombatActive()
{
    std::lock_guard<std::mutex> lock(_lock);
    return !_gameFinished &&
        (_gameStarted || (_currentStage == 2 && _stage2BossActive && _stage2Boss.state != 3));
}

bool Room::StartStage2()
{
    std::lock_guard<std::mutex> lock(_lock);
    if (_currentStage != 1 || !_gameStarted || _gameFinished)
    {
        return false;
    }

    _currentStage = 2;
    _gameStarted = false;
    _monsters.clear();
    _monsterArrows.clear();
    if (!_stage2Navigation.IsReady())
    {
        _stage2Navigation.Load(
            "Stage2Map/FloorCollider.fbx",
            "Stage2Map/Stage2WallCollider.fbx");
    }

    _stage2Boss = {};
    _stage2Boss.monsterId = STAGE2_BOSS_MONSTER_ID;
    _stage2Boss.type = STAGE2_BOSS_MONSTER_TYPE;
    _stage2Boss.state = 0;
    _stage2Boss.x = kStage2BossSpawnX;
    _stage2Boss.y = kStage2BossSpawnY;
    _stage2Boss.z = kStage2BossSpawnZ;
    _stage2Boss.rotY = 0.0f;
    _stage2Boss.speed = 2.0f;
    _stage2Boss.attackTimer = 0.0f;
    _stage2Boss.targetPlayerId = -1;
    _stage2Boss.hp = kStage2BossMaxHp;

    _stage2BossActive = true;
    _stage2ShockwaveTriggered = false;
    _stage2WipeTriggered = false;
    _stage2MirrorTriggered = false;
    _stage2ShockwaveDamagePending = false;
    _stage2WipeDamagePending = false;
    _stage2ShockwaveTimer = 0.0f;
    _stage2WipeTimer = 0.0f;
    _stage2MirrorInvulnerabilityTimer = 0.0f;
    _stage2MirrorRealIndex = 0;
    _teamOtherWorld = false;
    _teamOtherWorldTimer = 0.0f;
    _stage2ShockwaveX = _stage2Boss.x;
    _stage2ShockwaveY = _stage2Boss.y;
    _stage2ShockwaveZ = _stage2Boss.z;
    _stage2StartedAt = std::chrono::steady_clock::now();
    _stage2ClearTimeSeconds = 0.0f;
    _stage2BossDamageByPlayerId.clear();

    struct Stage2MonsterSpawn
    {
        int id;
        int type;
        float x;
        float y;
        float z;
    };

    const Stage2MonsterSpawn stage2MonsterSpawns[] =
    {
        { kStage2SkeletonSpawnBaseId + 0,  2, -9.40608f, -2.37823f,   9.0817f },
        { kStage2SkeletonSpawnBaseId + 1,  0, -3.57432f, -2.37823f,   9.14398f },
        { kStage2SkeletonSpawnBaseId + 2,  2, -5.49912f,  0.409166f, -1.35533f },
        { kStage2SkeletonSpawnBaseId + 3,  0, -5.68869f,  0.409166f, -3.96669f },
        { kStage2SkeletonSpawnBaseId + 4,  2, -9.66672f, -2.37823f,  -8.98436f },
        { kStage2SkeletonSpawnBaseId + 5,  0, -13.9063f, -2.37823f, -14.2775f },
        { kStage2SkeletonSpawnBaseId + 6,  2, -5.00478f, -2.37823f, -22.1984f },
        { kStage2SkeletonSpawnBaseId + 7,  0, -2.36333f, -2.37823f, -20.7057f },
        { kStage2SkeletonSpawnBaseId + 8,  2, 10.6695f,  -2.37823f, -22.9485f },
        { kStage2SkeletonSpawnBaseId + 9,  0, 10.0559f,  -2.37823f, -14.2374f },
        { kStage2SkeletonSpawnBaseId + 10, 2, 10.2588f,  -0.992236f,  3.82154f },
        { kStage2SkeletonSpawnBaseId + 11, 0, 12.6068f,  -0.992236f,  3.3069f },
        { kStage2SkeletonSpawnBaseId + 12, 2, 19.1168f,  -2.38803f, -7.4035f },
        { kStage2SkeletonSpawnBaseId + 13, 0, 21.2676f,  -2.38803f, -7.87047f },
        { kStage2SkeletonSpawnBaseId + 14, 2, -0.77279f,  0.410567f, -6.81426f },
        { kStage2SkeletonSpawnBaseId + 15, 0, -1.34973f,  0.410567f, -3.53298f },
        { kStage2SkeletonSpawnBaseId + 16, 2, -1.36433f,  0.410567f,  0.608799f },
        { kStage2SkeletonSpawnBaseId + 17, 0, 3.58349f,   0.410567f,  2.19363f },
        { kStage2SkeletonSpawnBaseId + 18, 2, 3.86871f,   0.410567f, -0.590942f },
        { kStage2SkeletonSpawnBaseId + 19, 0, 4.76613f,   0.410567f, -4.00824f },
    };

    for (const Stage2MonsterSpawn& spawn : stage2MonsterSpawns)
    {
        ServerMonster monster;
        monster.monsterId = spawn.id;
        monster.type = spawn.type;
        monster.state = 0;
        monster.x = spawn.x;
        monster.y = spawn.y;
        monster.z = spawn.z;
        monster.rotY = 0.0f;
        monster.speed = (spawn.type == 0) ? 2.6f : 3.2f;
        monster.attackTimer = 0.0f;
        monster.targetPlayerId = -1;
        monster.hp = 100;
        _monsters.push_back(monster);
    }

    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->ResetMoveValidation();
        }
    }

    return true;
}

bool Room::CompleteStage2Boss()
{
    std::lock_guard<std::mutex> lock(_lock);
    if (_gameFinished ||
        _currentStage != 2 ||
        !_stage2BossActive ||
        _stage2Boss.state != 3)
    {
        return false;
    }

    _gameFinished = true;
    _gameStarted = false;
    _stage2ShockwaveDamagePending = false;
    _stage2WipeDamagePending = false;
    _stage2ShockwaveTimer = 0.0f;
    _stage2WipeTimer = 0.0f;
    _stage2MirrorInvulnerabilityTimer = 0.0f;
    _teamOtherWorld = false;
    _teamOtherWorldTimer = 0.0f;
    if (_stage2StartedAt != std::chrono::steady_clock::time_point{})
    {
        const auto elapsed = std::chrono::steady_clock::now() - _stage2StartedAt;
        _stage2ClearTimeSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() / 1000.0f;
    }
    else
    {
        _stage2ClearTimeSeconds = 0.0f;
    }
    return true;
}

void Room::FillStage2GameResultPacket(PKT_S_GAME_RESULT& outPacket)
{
    std::lock_guard<std::mutex> lock(_lock);

    outPacket.clearTimeSeconds = _stage2ClearTimeSeconds;
    outPacket.playerCount = 0;

    std::vector<std::pair<int, std::shared_ptr<Session>>> orderedSessions;
    orderedSessions.reserve(_sessions.size());

    for (const auto& session : _sessions)
    {
        if (session == nullptr || session->GetPlayerId() <= 0)
        {
            continue;
        }

        orderedSessions.push_back({ _stage2BossDamageByPlayerId[session->GetPlayerId()], session });
    }

    std::sort(
        orderedSessions.begin(),
        orderedSessions.end(),
        [](const auto& lhs, const auto& rhs)
        {
            if (lhs.first != rhs.first)
            {
                return lhs.first > rhs.first;
            }

            return lhs.second->GetPlayerId() < rhs.second->GetPlayerId();
        });

    const int maxEntries = (std::min)(MAX_LOBBY_PLAYERS, static_cast<int>(orderedSessions.size()));
    for (int i = 0; i < maxEntries; ++i)
    {
        const auto& session = orderedSessions[static_cast<size_t>(i)].second;
        const int playerId = session->GetPlayerId();
        std::string displayName = session->GetDisplayName();
        if (displayName.empty())
        {
            displayName = "Player " + std::to_string(playerId);
        }

        outPacket.playerIds[i] = playerId;
        outPacket.bossDamageDealt[i] = orderedSessions[static_cast<size_t>(i)].first;
        strncpy_s(outPacket.playerNames[i], displayName.c_str(), _TRUNCATE);
        ++outPacket.playerCount;
    }

    for (int i = maxEntries; i < MAX_LOBBY_PLAYERS; ++i)
    {
        outPacket.playerIds[i] = -1;
        outPacket.bossDamageDealt[i] = 0;
        outPacket.playerNames[i][0] = '\0';
    }
}

void Room::BroadcastBossSnapshot()
{
    std::lock_guard<std::mutex> lock(_lock);
    if (_stage2BossActive)
    {
        BroadcastMonsterSyncLocked(_stage2Boss);
    }
}

std::vector<MonsterSnapshot> Room::GetMonsterSnapshots()
{
    std::lock_guard<std::mutex> lock(_lock);
    std::vector<MonsterSnapshot> result;
    if (_currentStage == 2 && _stage2BossActive)
    {
        MonsterSnapshot snap;
        snap.monsterId = _stage2Boss.monsterId;
        snap.type = _stage2Boss.type;
        snap.state = _stage2Boss.state;
        snap.x = _stage2Boss.x;
        snap.y = _stage2Boss.y;
        snap.z = _stage2Boss.z;
        result.push_back(snap);
    }

    for (auto& m : _monsters)
    {
        MonsterSnapshot snap;
        snap.monsterId = m.monsterId;
        snap.type = m.type;
        snap.state = m.state;
        snap.x = m.x;
        snap.y = m.y;
        snap.z = m.z;
        result.push_back(snap);
    }
    return result;
}

bool Room::ApplyDamageToMonster(int monsterId, int damage, int attackerPlayerId, int* outAppliedDamage)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (outAppliedDamage != nullptr)
    {
        *outAppliedDamage = 0;
    }

    if (_currentStage == 2 && _stage2BossActive && monsterId == STAGE2_BOSS_MONSTER_ID)
    {
        if (_stage2Boss.state == 3)
        {
            return false;
        }

        if (_stage2ShockwaveDamagePending || _stage2WipeDamagePending || _stage2MirrorInvulnerabilityTimer > 0.0f)
        {
            return false;
        }

        const int beforeHp = _stage2Boss.hp;
        const int bossDamage = damage > 0 ? damage : kStage2BossDamagePerHit;
        _stage2Boss.hp -= bossDamage;
        if (_stage2Boss.hp <= 0)
        {
            _stage2Boss.hp = 0;
            _stage2Boss.state = 3;
            _stage2BossActive = true;
            const int appliedDamage = beforeHp;
            if (outAppliedDamage != nullptr)
            {
                *outAppliedDamage = appliedDamage;
            }
            RecordStage2BossDamageLocked(attackerPlayerId, appliedDamage);
            return true;
        }

        const int appliedDamage = beforeHp - _stage2Boss.hp;
        if (outAppliedDamage != nullptr)
        {
            *outAppliedDamage = appliedDamage;
        }
        RecordStage2BossDamageLocked(attackerPlayerId, appliedDamage);
        return false;
    }

    for (auto& m : _monsters)
    {
        if (m.monsterId == monsterId)
        {
            if (m.state == 3)
            {
                return false;
            }

            const int beforeHp = m.hp;
            m.hp -= damage;
            if (m.hp <= 0)
            {
                m.hp = 0;
                m.state = 3; // DIE
                if (outAppliedDamage != nullptr)
                {
                    *outAppliedDamage = beforeHp;
                }
                return true;
            }

            if (outAppliedDamage != nullptr)
            {
                *outAppliedDamage = beforeHp - m.hp;
            }
            return false;
        }
    }
    return false;
}

void Room::RecordStage2BossDamageLocked(int attackerPlayerId, int appliedDamage)
{
    if (attackerPlayerId <= 0 || appliedDamage <= 0)
    {
        return;
    }

    _stage2BossDamageByPlayerId[attackerPlayerId] += appliedDamage;
}

int Room::GetMonsterHp(int monsterId)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (_currentStage == 2 && _stage2BossActive && monsterId == STAGE2_BOSS_MONSTER_ID)
    {
        return _stage2Boss.hp;
    }

    for (auto& m : _monsters)
        if (m.monsterId == monsterId)
            return m.hp;
    return 0;
}

bool Room::SetDoorOpen(int doorId, bool isOpen)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (doorId <= 0)
    {
        return false;
    }

    _doorOpenStates[doorId] = isOpen;
    return true;
}

bool Room::GetDoorOpen(int doorId)
{
    std::lock_guard<std::mutex> lock(_lock);
    const auto it = _doorOpenStates.find(doorId);
    return it != _doorOpenStates.end() && it->second;
}

bool Room::MarkPickupCollected(int pickupId)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (pickupId <= 0)
    {
        return false;
    }

    return _collectedPickups.insert(pickupId).second;
}

void Room::AddLanternChargeForAll(float amount)
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->AddLanternCharge(amount);
        }
    }
}

void Room::ConsumeLanternForAll()
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->ConsumeWorldShift();
        }
    }
}

void Room::StartWorldShiftForAll(float durationSeconds)
{
    std::lock_guard<std::mutex> lock(_lock);
    UNREFERENCED_PARAMETER(durationSeconds);
    _teamOtherWorld = !_teamOtherWorld;
    _teamOtherWorldTimer = 0.0f;
}

void Room::BroadcastLanternStates()
{
    std::lock_guard<std::mutex> lock(_lock);
    BroadcastLanternStatesLocked();
}

void Room::BroadcastPlayerHp(const std::shared_ptr<Session>& targetSession)
{
    std::lock_guard<std::mutex> lock(_lock);
    BroadcastPlayerHitLocked(targetSession);
}

void Room::RequestPlayerRespawn(const std::shared_ptr<Session>& targetSession)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (targetSession == nullptr || _gameFinished)
    {
        return;
    }

    const bool stage2Active =
        _currentStage == 2 &&
        _stage2BossActive &&
        _stage2Boss.state != 3;
    if (!_gameStarted && !stage2Active)
    {
        return;
    }

    if (!targetSession->IsPlayerDead() && !stage2Active)
    {
        return;
    }

    RespawnPlayerLocked(targetSession);
}

void Room::HealPlayersAround(int healerPlayerId, float x, float y, float z, float radius, int amount)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (healerPlayerId <= 0 || radius <= 0.0f || amount <= 0 || _gameFinished)
    {
        return;
    }

    const float radiusSq = radius * radius;
    constexpr float kHealVerticalTolerance = 4.0f;

    for (auto& session : _sessions)
    {
        if (session == nullptr || session->IsPlayerDead())
        {
            continue;
        }

        const float dx = session->GetX() - x;
        const float dy = session->GetY() - y;
        const float dz = session->GetZ() - z;
        if (std::fabs(dy) > kHealVerticalTolerance ||
            (dx * dx + dz * dz) > radiusSq)
        {
            continue;
        }

        if (session->ApplyPlayerHeal(amount))
        {
            BroadcastPlayerHitLocked(session);
        }
    }
}

void Room::UpdateStage2BossLocked(const std::vector<PlayerSnapshot>& players, float dt)
{
    if (!_stage2BossActive)
    {
        return;
    }

    ServerMonster& boss = _stage2Boss;

    if (boss.state != 3)
    {
        boss.attackTimer -= dt;
        if (boss.attackTimer < 0.0f)
        {
            boss.attackTimer = 0.0f;
        }

        int nearestId = -1;
        float nearestDist = FLT_MAX;
        float nearestX = 0.0f;
        float nearestZ = 0.0f;

        for (const auto& p : players)
        {
            if (p.isDead)
            {
                continue;
            }

            const float dx = p.x - boss.x;
            const float dz = p.z - boss.z;
            const float dist = sqrtf(dx * dx + dz * dz);
            if (dist < kStage2BossDetectRange && dist < nearestDist)
            {
                nearestDist = dist;
                nearestId = p.playerId;
                nearestX = p.x;
                nearestZ = p.z;
            }
        }

        if (nearestId == -1)
        {
            boss.state = 0;
            boss.targetPlayerId = -1;
        }
        else
        {
            const float dx = nearestX - boss.x;
            const float dz = nearestZ - boss.z;
            if ((dx * dx + dz * dz) > 0.0001f)
            {
                boss.rotY = atan2f(dx, dz) * (180.0f / 3.14159265f);
            }

            boss.targetPlayerId = nearestId;

            if (nearestDist <= kStage2BossAttackRange)
            {
                boss.state = 2;
                if (boss.attackTimer <= 0.0f)
                {
                    auto targetSession = FindSessionByPlayerIdLocked(nearestId);
                    if (targetSession != nullptr && !targetSession->IsPlayerDead())
                    {
                        targetSession->ApplyPlayerDamage(kStage2BossAttackDamage);
                        BroadcastPlayerHitLocked(targetSession);
                    }
                    boss.attackTimer = kStage2BossAttackCooldownSeconds;
                }
            }
            else
            {
                boss.state = 1;
                if (nearestDist > 0.001f)
                {
                    boss.x += (dx / nearestDist) * boss.speed * dt;
                    boss.z += (dz / nearestDist) * boss.speed * dt;
                }
            }
        }

        const int bossLayer = GetStage2BossLayerLocked();
        if (!_stage2ShockwaveTriggered && bossLayer > 0 && bossLayer <= kStage2ShockwaveLayer)
        {
            _stage2ShockwaveTriggered = true;
            _stage2ShockwaveDamagePending = true;
            _stage2ShockwaveTimer = kStage2ShockwaveDelay;
            _stage2ShockwaveX = boss.x;
            _stage2ShockwaveY = boss.y;
            _stage2ShockwaveZ = boss.z;
            BroadcastBossPatternLocked(
                BOSS_PATTERN_STAGE2_SHOCKWAVE,
                _stage2ShockwaveX,
                _stage2ShockwaveY,
                _stage2ShockwaveZ,
                kStage2ShockwaveRadius,
                kStage2ShockwaveDelay,
                kStage2ShockwaveDamage);
        }

        if (!_stage2WipeTriggered && bossLayer > 0 && bossLayer <= kStage2WipeLayer)
        {
            _stage2WipeTriggered = true;
            _stage2WipeDamagePending = true;
            _stage2WipeTimer = kStage2WipeDelay;
            BroadcastBossPatternLocked(
                BOSS_PATTERN_STAGE2_WIPE,
                boss.x,
                boss.y,
                boss.z,
                0.0f,
                kStage2WipeDelay,
                kStage2WipeDamage);
        }

        if (!_stage2MirrorTriggered && bossLayer > 0 && bossLayer <= kStage2MirrorLayer)
        {
            _stage2MirrorTriggered = true;
            _stage2MirrorRealIndex = std::rand() % kStage2MirrorSlotCount;
            _stage2MirrorInvulnerabilityTimer = kStage2MirrorInvulnerabilityDelay;
            BroadcastBossPatternLocked(
                BOSS_PATTERN_STAGE2_MIRROR,
                boss.x,
                boss.y,
                boss.z,
                0.0f,
                kStage2MirrorInvulnerabilityDelay,
                0,
                _stage2MirrorRealIndex);
        }
    }

    if (_stage2ShockwaveDamagePending)
    {
        _stage2ShockwaveTimer -= dt;
        if (_stage2ShockwaveTimer <= 0.0f)
        {
            _stage2ShockwaveDamagePending = false;
            _stage2ShockwaveTimer = 0.0f;

            for (const auto& p : players)
            {
                if (p.isDead)
                {
                    continue;
                }

                const float dx = p.x - _stage2ShockwaveX;
                const float dz = p.z - _stage2ShockwaveZ;
                if ((dx * dx + dz * dz) > (kStage2ShockwaveRadius * kStage2ShockwaveRadius))
                {
                    continue;
                }

                auto targetSession = FindSessionByPlayerIdLocked(p.playerId);
                if (targetSession != nullptr && !targetSession->IsPlayerDead())
                {
                    targetSession->ApplyPlayerDamage(kStage2ShockwaveDamage);
                    BroadcastPlayerHitLocked(targetSession);
                }
            }
        }
    }

    if (_stage2WipeDamagePending)
    {
        _stage2WipeTimer -= dt;
        if (_stage2WipeTimer <= 0.0f)
        {
            _stage2WipeDamagePending = false;
            _stage2WipeTimer = 0.0f;

            if (!_teamOtherWorld)
            {
                for (const auto& p : players)
                {
                    if (p.isDead)
                    {
                        continue;
                    }

                    auto targetSession = FindSessionByPlayerIdLocked(p.playerId);
                    if (targetSession != nullptr && !targetSession->IsPlayerDead())
                    {
                        targetSession->ApplyPlayerDamage(kStage2WipeDamage);
                        BroadcastPlayerHitLocked(targetSession);
                    }
                }
            }
        }
    }

    if (_stage2MirrorInvulnerabilityTimer > 0.0f)
    {
        _stage2MirrorInvulnerabilityTimer = (std::max)(0.0f, _stage2MirrorInvulnerabilityTimer - dt);
    }

    BroadcastMonsterSyncLocked(boss);
}

void Room::SpawnMonsterArrowLocked(const ServerMonster& monster)
{
    if (!IsRangedMonsterType(monster.type) || monster.state == 3)
    {
        return;
    }

    const float rotRad = monster.rotY * (kPi / 180.0f);
    const float forwardX = sinf(rotRad);
    const float forwardZ = cosf(rotRad);
    const float rightX = cosf(rotRad);
    const float rightZ = -sinf(rotRad);
    const float startRightOffset = GetMonsterArrowStartRightOffset(monster.type);
    const float travelDistance = ClampFloat(
        GetRangedMonsterVisualAttackRange(monster.type) + kMonsterArrowExtraTravelDistance,
        kMonsterArrowMinDistance,
        kMonsterArrowMaxDistance);

    ServerMonsterArrow arrow;
    arrow.monsterId = monster.monsterId;
    arrow.monsterType = monster.type;
    arrow.startX = monster.x + forwardX * kMonsterArrowStartForwardOffset + rightX * startRightOffset;
    arrow.startY = monster.y + GetMonsterArrowStartHeight(monster.type);
    arrow.startZ = monster.z + forwardZ * kMonsterArrowStartForwardOffset + rightZ * startRightOffset;
    arrow.dirX = forwardX;
    arrow.dirZ = forwardZ;
    arrow.startDelay = GetMonsterArrowReleaseDelay(monster.type);
    arrow.travelDistance = travelDistance;
    arrow.motionDuration = (std::max)(travelDistance / kMonsterArrowSpeed, 0.12f);

    _monsterArrows.push_back(arrow);
}

void Room::UpdateMonsterArrowsLocked(const std::vector<PlayerSnapshot>& players, float dt)
{
    if (dt <= 0.0f || _monsterArrows.empty())
    {
        return;
    }

    auto arrowIt = _monsterArrows.begin();
    while (arrowIt != _monsterArrows.end())
    {
        if (_currentStage == 1 &&
            IsOtherWorldMonsterType(arrowIt->monsterType) != _teamOtherWorld)
        {
            arrowIt = _monsterArrows.erase(arrowIt);
            continue;
        }

        const float previousAge = arrowIt->age;
        arrowIt->age += dt;

        bool shouldRemove = false;
        const float previousDamageAge = previousAge - kMonsterArrowDamageSampleBacktrackSeconds;
        const float currentDamageAge = arrowIt->age - kMonsterArrowDamageSampleBacktrackSeconds;
        if (currentDamageAge >= arrowIt->startDelay)
        {
            const float previousSampleAge = (std::max)(previousDamageAge, arrowIt->startDelay);
            const float currentSampleAge = (std::min)(
                currentDamageAge,
                arrowIt->startDelay + arrowIt->motionDuration);
            if (currentSampleAge >= previousSampleAge)
            {
                const MonsterArrowPosition previousPosition = GetMonsterArrowPositionAt(*arrowIt, previousSampleAge);
                const MonsterArrowPosition currentPosition = GetMonsterArrowPositionAt(*arrowIt, currentSampleAge);

                for (const PlayerSnapshot& player : players)
                {
                    if (!DoesMonsterArrowHitPlayer(player, previousPosition, currentPosition))
                    {
                        continue;
                    }

                    auto targetSession = FindSessionByPlayerIdLocked(player.playerId);
                    if (targetSession != nullptr && !targetSession->IsPlayerDead())
                    {
                        targetSession->ApplyPlayerDamage(kMonsterAttackDamage);
                        BroadcastPlayerHitLocked(targetSession);
                        shouldRemove = true;
                        break;
                    }
                }
            }
        }

        const float lifeTime =
            arrowIt->startDelay +
            arrowIt->motionDuration +
            kMonsterArrowLifePaddingSeconds;
        if (shouldRemove || arrowIt->age >= lifeTime)
        {
            arrowIt = _monsterArrows.erase(arrowIt);
        }
        else
        {
            ++arrowIt;
        }
    }
}

void Room::UpdateMonsters(float dt)
{
    auto players = GetPlayerSnapshots();

    std::lock_guard<std::mutex> lock(_lock);
    const bool stage2Active =
        _currentStage == 2 &&
        _stage2BossActive &&
        _stage2Boss.state != 3;

    if (stage2Active)
    {
        UpdateStage2BossLocked(players, dt);
    }

    if (!_gameStarted && !stage2Active)
    {
        _monsterArrows.clear();
        return;
    }

    UpdateMonsterArrowsLocked(players, dt);

    const NavigationGrid& navigation = GetActiveMonsterNavigationLocked();

    for (auto& m : _monsters)
    {
        const bool isOtherWorldMonster = IsOtherWorldMonsterType(m.type);
        if (_currentStage == 1 && isOtherWorldMonster != _teamOtherWorld)
        {
            m.state = 0;
            m.attackTimer = 0.0f;
            m.targetPlayerId = -1;
            m.navigationPath.clear();
            m.navigationPathIndex = 0;
            BroadcastMonsterSyncLocked(m);
            continue;
        }

        const bool isRangedMonster = IsRangedMonsterType(m.type);
        const float detectRange = isRangedMonster ? 10.0f : 5.0f;
        const float attackRange = isRangedMonster ? 9.5f : 1.8f;
        const float attackCooldown =
            m.type == kSpectralArcherMonsterType ? 2.0f :
            m.type == kSpectralImpMonsterType ? 1.0f :
            isRangedMonster ? 4.0f : 2.0f;

        if (m.state == 3) continue; // DIE
        m.attackTimer -= dt;
        if (m.attackTimer < 0.0f)
        {
            m.attackTimer = 0.0f;
        }

        int   nearestId = -1;
        float nearestDist = FLT_MAX;
        float nearestX = 0.0f;
        float nearestZ = 0.0f;

        for (auto& p : players)
        {
            if (p.isDead)
            {
                continue;
            }

            float dx = p.x - m.x;
            float dz = p.z - m.z;
            float dist = sqrtf(dx * dx + dz * dz);

            if (dist < detectRange &&
                (!navigation.IsReady() || navigation.HasDirectPath(m.x, m.z, p.x, p.z)) &&
                dist < nearestDist)
            {
                nearestDist = dist;
                nearestId = p.playerId;
                nearestX = p.x;
                nearestZ = p.z;
            }
        }

        if (nearestId == -1)
        {
            m.state = 0;
            m.targetPlayerId = -1;
            m.navigationPath.clear();
            m.navigationPathIndex = 0;
        }
        else
        {
            const bool canAttackTarget =
                nearestDist <= attackRange &&
                (!navigation.IsReady() || navigation.HasDirectPath(m.x, m.z, nearestX, nearestZ));

            if (canAttackTarget)
            {
                m.state = 2;
                m.targetPlayerId = nearestId;

                const float dx = nearestX - m.x;
                const float dz = nearestZ - m.z;
                if ((dx * dx + dz * dz) > 0.0001f)
                {
                    m.rotY = atan2f(dx, dz) * (180.0f / kPi);
                }

                if (m.attackTimer <= 0.0f)
                {
                    ++m.attackSequence;
                    if (isRangedMonster)
                    {
                        SpawnMonsterArrowLocked(m);
                    }
                    else
                    {
                        auto targetSession = FindSessionByPlayerIdLocked(nearestId);
                        if (targetSession != nullptr && !targetSession->IsPlayerDead())
                        {
                            targetSession->ApplyPlayerDamage(kMonsterAttackDamage);
                            BroadcastPlayerHitLocked(targetSession);
                        }
                    }

                    m.attackTimer = attackCooldown;
                }
            }
            else
            {
                m.state = 1;
                m.targetPlayerId = nearestId;
                if (!MoveMonsterAlongNavigationPathLocked(m, nearestX, nearestZ, dt))
                {
                    m.state = 0;
                    m.targetPlayerId = -1;
                }
            }
        }

        BroadcastMonsterSyncLocked(m);
    }
}

const NavigationGrid& Room::GetActiveMonsterNavigationLocked() const
{
    if (_currentStage == 2)
    {
        return _stage2Navigation;
    }

    return _teamOtherWorld ? _stage1OtherNavigation : _stage1RealNavigation;
}

bool Room::MoveMonsterAlongNavigationPathLocked(ServerMonster& monster, float targetX, float targetZ, float dt)
{
    constexpr float kTargetRefreshDistance = 1.0f;
    constexpr float kWaypointReachDistance = 0.22f;

    const NavigationGrid& navigation = GetActiveMonsterNavigationLocked();
    if (!navigation.IsReady())
    {
        const float dx = targetX - monster.x;
        const float dz = targetZ - monster.z;
        const float distance = sqrtf(dx * dx + dz * dz);
        if (distance <= 0.001f)
        {
            return true;
        }

        const float moveDistance = (std::min)(monster.speed * dt, distance);
        monster.x += (dx / distance) * moveDistance;
        monster.z += (dz / distance) * moveDistance;
        monster.rotY = atan2f(dx, dz) * (180.0f / 3.14159265f);
        return true;
    }

    const float targetDx = targetX - monster.navigationTargetX;
    const float targetDz = targetZ - monster.navigationTargetZ;
    const bool targetMoved =
        (targetDx * targetDx + targetDz * targetDz) >=
        (kTargetRefreshDistance * kTargetRefreshDistance);
    const bool pathExhausted = monster.navigationPathIndex >= monster.navigationPath.size();
    const bool worldChanged = monster.navigationUsesOtherWorld != _teamOtherWorld;
    if (monster.navigationPath.empty() || pathExhausted || targetMoved || worldChanged)
    {
        monster.navigationPath.clear();
        monster.navigationPathIndex = 0;
        monster.navigationTargetX = targetX;
        monster.navigationTargetZ = targetZ;
        monster.navigationUsesOtherWorld = _teamOtherWorld;

        if (!navigation.FindPath(
            monster.x,
            monster.z,
            targetX,
            targetZ,
            monster.navigationPath))
        {
            return false;
        }
    }

    while (monster.navigationPathIndex < monster.navigationPath.size())
    {
        const auto& waypoint = monster.navigationPath[monster.navigationPathIndex];
        const float dx = waypoint.first - monster.x;
        const float dz = waypoint.second - monster.z;
        if ((dx * dx + dz * dz) > (kWaypointReachDistance * kWaypointReachDistance))
        {
            break;
        }
        ++monster.navigationPathIndex;
    }

    if (monster.navigationPathIndex >= monster.navigationPath.size())
    {
        return true;
    }

    const auto& waypoint = monster.navigationPath[monster.navigationPathIndex];
    const float dx = waypoint.first - monster.x;
    const float dz = waypoint.second - monster.z;
    const float distance = sqrtf(dx * dx + dz * dz);
    if (distance <= 0.001f)
    {
        return true;
    }

    const float moveDistance = (std::min)(monster.speed * dt, distance);
    monster.x += (dx / distance) * moveDistance;
    monster.z += (dz / distance) * moveDistance;
    monster.rotY = atan2f(dx, dz) * (180.0f / 3.14159265f);
    return true;
}

void Room::SetHost(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_lock);
    _host = session;
    BroadcastRoomInfoLocked();
}

std::shared_ptr<Session> Room::GetHost()
{
    std::lock_guard<std::mutex> lock(_lock);
    return _host;
}

std::vector<int> Room::GetPlayerIds()
{
    std::lock_guard<std::mutex> lock(_lock);
    std::vector<int> result;
    for (auto& session : _sessions)
    {
        if (session != nullptr && session->GetPlayerId() > 0)
        {
            result.push_back(session->GetPlayerId());
        }
    }
    return result;
}

void Room::SetPlayerReady(std::shared_ptr<Session> session, bool ready)
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& playerSession : _sessions)
    {
        if (playerSession == session)
        {
            playerSession->SetReady(ready);
            break;
        }
    }

    BroadcastRoomInfoLocked();
}

bool Room::CanStartGame(std::shared_ptr<Session> requester)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (_host == nullptr || requester != _host || _sessions.size() != MAX_LOBBY_PLAYERS ||
        _gameStarted || _currentStage != 1 || _gameFinished)
    {
        return false;
    }

    for (auto& session : _sessions)
    {
        if (session == nullptr || !session->IsReady())
        {
            return false;
        }
    }

    return true;
}

bool Room::CanEnter()
{
    std::lock_guard<std::mutex> lock(_lock);
    return _sessions.size() < MAX_LOBBY_PLAYERS;
}

bool Room::IsStage2()
{
    std::lock_guard<std::mutex> lock(_lock);
    return _currentStage == 2;
}

int Room::GetPlayerCount()
{
    std::lock_guard<std::mutex> lock(_lock);
    return static_cast<int>(_sessions.size());
}

void Room::BroadcastRoomInfoLocked()
{
    PKT_S_ROOM_INFO roomPkt = {};
    roomPkt.header.size = sizeof(PKT_S_ROOM_INFO);
    roomPkt.header.id = PacketID::S_ROOM_INFO;
    roomPkt.playerCount = static_cast<int>(_sessions.size());

    for (int i = 0; i < MAX_LOBBY_PLAYERS && i < static_cast<int>(_sessions.size()); ++i)
    {
        if (_sessions[i] != nullptr)
        {
            roomPkt.playerIds[i] = _sessions[i]->GetPlayerId();
            roomPkt.readyStates[i] = _sessions[i]->IsReady();
            std::string displayName = _sessions[i]->GetDisplayName();
            if (displayName.empty())
            {
                displayName = "Player " + std::to_string(_sessions[i]->GetPlayerId());
            }
            strncpy_s(roomPkt.playerNames[i], displayName.c_str(), _TRUNCATE);
        }
        else
        {
            roomPkt.playerIds[i] = -1;
            roomPkt.readyStates[i] = false;
            roomPkt.playerNames[i][0] = '\0';
        }
    }

    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->Send(&roomPkt, sizeof(roomPkt));
        }
    }
}
