#include "Room.h"
#include "Protocol.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdlib>

std::shared_ptr<Room> G_Room = std::make_shared<Room>();

namespace
{
    constexpr bool kAllowSoloLobbyStart = true;
    constexpr int kMonsterAttackDamage = 10;
    constexpr float kMonsterAttackCooldownSeconds = 1.5f;
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
    constexpr float kStage2BossDetectRange = 24.0f;
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
    _monsters.clear();
    _doorOpenStates.clear();
    _collectedPickups.clear();
    _currentStage = 1;
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
        monster.hp = 100;
        _monsters.push_back(monster);
    }
}

void Room::BroadcastMonsterSnapshots()
{
    std::lock_guard<std::mutex> lock(_lock);

    if (_currentStage == 2 && _stage2BossActive)
    {
        BroadcastMonsterSyncLocked(_stage2Boss);
        return;
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
    const float respawnX = isStage2 ? kStage2PlayerRespawnX : kStage1PlayerRespawnX;
    const float respawnY = isStage2 ? kStage2PlayerRespawnY : kStage1PlayerRespawnY;
    const float respawnZ = isStage2 ? kStage2PlayerRespawnZ : kStage1PlayerRespawnZ;

    targetSession->RespawnPlayer(respawnX, respawnY, respawnZ);

    PKT_S_PLAYER_RESPAWN respawnPkt = {};
    respawnPkt.header.size = sizeof(PKT_S_PLAYER_RESPAWN);
    respawnPkt.header.id = PacketID::S_PLAYER_RESPAWN;
    respawnPkt.playerId = targetSession->GetPlayerId();
    respawnPkt.x = respawnX;
    respawnPkt.y = respawnY;
    respawnPkt.z = respawnZ;
    respawnPkt.remainHp = targetSession->GetPlayerHp();

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
    syncPkt.x = monster.x;
    syncPkt.y = monster.y;
    syncPkt.z = monster.z;
    syncPkt.rotY = monster.rotY;

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

void Room::StartStage2()
{
    std::lock_guard<std::mutex> lock(_lock);
    _currentStage = 2;
    _gameStarted = false;
    _monsters.clear();

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

    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->FillLanternGauge();
        }
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
        snap.state = _stage2Boss.state;
        snap.x = _stage2Boss.x;
        snap.y = _stage2Boss.y;
        snap.z = _stage2Boss.z;
        result.push_back(snap);
        return result;
    }

    for (auto& m : _monsters)
    {
        MonsterSnapshot snap;
        snap.monsterId = m.monsterId;
        snap.state = m.state;
        snap.x = m.x;
        snap.y = m.y;
        snap.z = m.z;
        result.push_back(snap);
    }
    return result;
}

bool Room::ApplyDamageToMonster(int monsterId, int damage)
{
    std::lock_guard<std::mutex> lock(_lock);
    if (_currentStage == 2 && _stage2BossActive && monsterId == STAGE2_BOSS_MONSTER_ID)
    {
        if (_stage2Boss.state == 3)
        {
            return true;
        }

        if (_stage2ShockwaveDamagePending || _stage2WipeDamagePending || _stage2MirrorInvulnerabilityTimer > 0.0f)
        {
            return false;
        }

        (void)damage;
        _stage2Boss.hp -= kStage2BossDamagePerHit;
        if (_stage2Boss.hp <= 0)
        {
            _stage2Boss.hp = 0;
            _stage2Boss.state = 3;
            _stage2BossActive = true;
            return true;
        }

        return false;
    }

    for (auto& m : _monsters)
    {
        if (m.monsterId == monsterId)
        {
            m.hp -= damage;
            if (m.hp <= 0)
            {
                m.hp = 0;
                m.state = 3; // DIE
                return true;
            }
            return false;
        }
    }
    return false;
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

void Room::FillLanternForAll()
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->FillLanternGauge();
        }
    }
}

void Room::StartWorldShiftForAll(float durationSeconds)
{
    std::lock_guard<std::mutex> lock(_lock);
    _teamOtherWorld = true;
    _teamOtherWorldTimer = (std::max)(0.0f, durationSeconds);
}

void Room::BroadcastLanternStates()
{
    std::lock_guard<std::mutex> lock(_lock);
    BroadcastLanternStatesLocked();
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
                        const bool died = targetSession->ApplyPlayerDamage(kStage2BossAttackDamage);
                        BroadcastPlayerHitLocked(targetSession);
                        if (died)
                        {
                            RespawnPlayerLocked(targetSession);
                        }
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
                    const bool died = targetSession->ApplyPlayerDamage(kStage2ShockwaveDamage);
                    BroadcastPlayerHitLocked(targetSession);
                    if (died)
                    {
                        RespawnPlayerLocked(targetSession);
                    }
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
                        const bool died = targetSession->ApplyPlayerDamage(kStage2WipeDamage);
                        BroadcastPlayerHitLocked(targetSession);
                        if (died)
                        {
                            RespawnPlayerLocked(targetSession);
                        }
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

void Room::UpdateMonsters(float dt)
{
    auto players = GetPlayerSnapshots();

    std::lock_guard<std::mutex> lock(_lock);
    if (_teamOtherWorld)
    {
        _teamOtherWorldTimer -= dt;
        if (_teamOtherWorldTimer <= 0.0f)
        {
            _teamOtherWorld = false;
            _teamOtherWorldTimer = 0.0f;
        }
    }

    if (_currentStage == 2 && _stage2BossActive)
    {
        UpdateStage2BossLocked(players, dt);
        return;
    }

    if (!_gameStarted)
    {
        return;
    }

    const float DETECT_RANGE = 20.0f;
    const float ATTACK_RANGE = 2.0f;

    for (auto& m : _monsters)
    {
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

            if (dist < DETECT_RANGE && dist < nearestDist)
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
        }
        else if (nearestDist <= ATTACK_RANGE)
        {
            m.state = 2;
            m.targetPlayerId = nearestId;

            const float dx = nearestX - m.x;
            const float dz = nearestZ - m.z;
            if ((dx * dx + dz * dz) > 0.0001f)
            {
                m.rotY = atan2f(dx, dz) * (180.0f / 3.14159265f);
            }

            if (m.attackTimer <= 0.0f)
            {
                auto targetSession = FindSessionByPlayerIdLocked(nearestId);
                if (targetSession != nullptr && !targetSession->IsPlayerDead())
                {
                    const bool died = targetSession->ApplyPlayerDamage(kMonsterAttackDamage);
                    BroadcastPlayerHitLocked(targetSession);
                    if (died)
                    {
                        RespawnPlayerLocked(targetSession);
                    }
                }

                m.attackTimer = kMonsterAttackCooldownSeconds;
            }
        }
        else
        {
            m.state = 1;
            m.targetPlayerId = nearestId;

            float dx = nearestX - m.x;
            float dz = nearestZ - m.z;
            float dist = sqrtf(dx * dx + dz * dz);

            if (dist > 0.001f)
            {
                m.x += (dx / dist) * m.speed * dt;
                m.z += (dz / dist) * m.speed * dt;
                m.rotY = atan2f(dx, dz) * (180.0f / 3.14159265f);
            }
        }

        BroadcastMonsterSyncLocked(m);
    }
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
    if (_host == nullptr || requester != _host || _sessions.size() != MAX_LOBBY_PLAYERS)
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
        }
        else
        {
            roomPkt.playerIds[i] = -1;
            roomPkt.readyStates[i] = false;
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
