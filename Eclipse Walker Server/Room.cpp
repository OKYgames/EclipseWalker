#include "Room.h"
#include "Protocol.h"
#include <cmath>
#include <cfloat>

std::shared_ptr<Room> G_Room = std::make_shared<Room>();

void Room::Enter(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_lock);
    _sessions.push_back(session);
}

void Room::Leave(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_lock);
    auto it = std::remove(_sessions.begin(), _sessions.end(), session);
    _sessions.erase(it, _sessions.end());
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

    ServerMonster m1;
    m1.monsterId = 1;
    m1.type = 0;
    m1.state = 0;
    m1.x = 10.0f; m1.y = 0.0f; m1.z = 10.0f;
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