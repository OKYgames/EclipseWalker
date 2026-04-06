#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "Session.h" 

// [추가 1] 서버에서 몬스터의 현재 상태와 위치를 기억할 구조체
struct ServerMonster
{
    int monsterId;
    int type;
    int state; // 0:IDLE, 1:TRACE, 2:ATTACK, 3:DIE
    float x, y, z;
    float rotY;

    // AI 연산용 내부 변수
    float speed = 3.0f;
    float attackTimer = 0.0f;
    int targetPlayerId = -1;
};

class Room
{
public:
    void Enter(std::shared_ptr<Session> session);
    void Leave(std::shared_ptr<Session> session);
    void Broadcast(void* msg, int len);

    // [추가 2] public 함수
    void BroadcastExcept(std::shared_ptr<Session> excludeSession, void* msg, int len);
    void InitMonsters();
    void UpdateMonsters(float dt);

private:
    std::mutex _lock;
    std::vector<std::shared_ptr<Session>> _sessions;

    // [추가 3] private 멤버 변수
    std::vector<ServerMonster> _monsters;
};

extern std::shared_ptr<Room> G_Room;