#include "Room.h"
#include "Protocol.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>

std::shared_ptr<Room> G_Room = std::make_shared<Room>();

namespace
{
    constexpr bool kAllowSoloLobbyStart = true;
    constexpr int kMonsterAttackDamage = 10;
    constexpr float kMonsterAttackCooldownSeconds = 1.5f;

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

    for (const auto& m : _monsters)
    {
        PKT_S_MONSTER_SYNC syncPkt = {};
        syncPkt.header.size = sizeof(PKT_S_MONSTER_SYNC);
        syncPkt.header.id = PacketID::S_MONSTER_SYNC;
        syncPkt.monsterId = m.monsterId;
        syncPkt.monsterType = m.type;
        syncPkt.state = m.state;
        syncPkt.x = m.x;
        syncPkt.y = m.y;
        syncPkt.z = m.z;
        syncPkt.rotY = m.rotY;

        for (auto& session : _sessions)
        {
            if (session != nullptr)
            {
                session->Send(&syncPkt, sizeof(syncPkt));
            }
        }
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

void Room::ResetPlayerCombatStates()
{
    std::lock_guard<std::mutex> lock(_lock);
    for (auto& session : _sessions)
    {
        if (session != nullptr)
        {
            session->ResetPlayerCombatState();
            BroadcastPlayerHitLocked(session);
        }
    }
}

void Room::SetGameStarted(bool gameStarted)
{
    std::lock_guard<std::mutex> lock(_lock);
    _gameStarted = gameStarted;
}

std::vector<MonsterSnapshot> Room::GetMonsterSnapshots()
{
    std::lock_guard<std::mutex> lock(_lock);
    std::vector<MonsterSnapshot> result;
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

void Room::UpdateMonsters(float dt)
{
    auto players = GetPlayerSnapshots();

    std::lock_guard<std::mutex> lock(_lock);
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
                    targetSession->ApplyPlayerDamage(kMonsterAttackDamage);
                    BroadcastPlayerHitLocked(targetSession);
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

        PKT_S_MONSTER_SYNC syncPkt;
        syncPkt.header.size = sizeof(PKT_S_MONSTER_SYNC);
        syncPkt.header.id = PacketID::S_MONSTER_SYNC;
        syncPkt.monsterId = m.monsterId;
        syncPkt.monsterType = m.type;
        syncPkt.state = m.state;
        syncPkt.x = m.x;
        syncPkt.y = m.y;
        syncPkt.z = m.z;
        syncPkt.rotY = m.rotY;

        for (auto& session : _sessions)
            if (session) session->Send(&syncPkt, sizeof(syncPkt));
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
