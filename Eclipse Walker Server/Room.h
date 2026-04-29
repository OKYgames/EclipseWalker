#pragma once
#include <vector>
#include <memory>
#include <mutex>
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

    void SetHost(std::shared_ptr<Session> session);

    std::vector<PlayerSnapshot>  GetPlayerSnapshots();
    std::vector<MonsterSnapshot> GetMonsterSnapshots();
    std::shared_ptr<Session> GetHost();
    std::vector<int> GetPlayerIds();

    bool ApplyDamageToMonster(int monsterId, int damage);
    bool CanEnter();
    int GetMonsterHp(int monsterId);
    int GetPlayerCount();

private:
    std::mutex _lock;
    std::vector<std::shared_ptr<Session>> _sessions;
    std::vector<ServerMonster>            _monsters;
    std::shared_ptr<Session> _host = nullptr;
};

extern std::shared_ptr<Room> G_Room;