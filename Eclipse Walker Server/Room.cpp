#include "Room.h"
#include "Protocol.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>

std::shared_ptr<Room> G_Room = std::make_shared<Room>();

namespace
{
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

    if (session->GetPlayerId() <= 0)
    {
        session->SetPlayerInfo(MakeTemporaryPlayerId(session), 0.0f, 0.0f, 0.0f);
    }
    session->SetReady(false);

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

    ServerMonster m1;
    m1.monsterId = 1;
    m1.type = 0;
    m1.state = 0;
    m1.x = 7.0f; m1.y = 1.4f; m1.z = 7.0f;
    m1.rotY = 0.0f;
    m1.hp = 100;

    _monsters.push_back(m1);
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
        result.push_back(snap);
    }
    return result;
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

void Room::UpdateMonsters(float dt)
{
    auto players = GetPlayerSnapshots();

    std::lock_guard<std::mutex> lock(_lock);

    const float DETECT_RANGE = 20.0f;
    const float ATTACK_RANGE = 2.0f;

    for (auto& m : _monsters)
    {
        if (m.state == 3) continue; // DIE

        int   nearestId = -1;
        float nearestDist = FLT_MAX;
        float nearestX = 0.0f;
        float nearestZ = 0.0f;

        for (auto& p : players)
        {
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

        bool changed = false;

        if (nearestId == -1)
        {
            if (m.state != 0) { m.state = 0; changed = true; }
        }
        else if (nearestDist <= ATTACK_RANGE)
        {
            if (m.state != 2) { m.state = 2; changed = true; }
        }
        else
        {
            m.state = 1;
            m.targetPlayerId = nearestId;

            float dx = nearestX - m.x;
            float dz = nearestZ - m.z;
            float dist = sqrtf(dx * dx + dz * dz);

            m.x += (dx / dist) * m.speed * dt;
            m.z += (dz / dist) * m.speed * dt;
            m.rotY = atan2f(dx, dz) * (180.0f / 3.14159265f);

            changed = true;
        }

        if (changed || m.state == 1)
        {
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
    if (_host == nullptr || requester != _host || _sessions.empty())
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
