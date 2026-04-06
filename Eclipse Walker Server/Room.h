#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "Session.h"

struct ServerMonster
{
    int   monsterId;
    int   type;
    int   state; // 0:IDLE, 1:TRACE, 2:ATTACK, 3:DIE
    float x, y, z;
    float rotY;

    float speed = 3.0f;
    float attackTimer = 0.0f;
    int   targetPlayerId = -1;
};

struct PlayerSnapshot
{
    int   playerId;
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

    std::vector<PlayerSnapshot> GetPlayerSnapshots();

private:
    std::mutex _lock;
    std::vector<std::shared_ptr<Session>> _sessions;
    std::vector<ServerMonster>            _monsters;
};

extern std::shared_ptr<Room> G_Room;