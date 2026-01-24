#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include "Session.h" // 세션을 알아야 함

class Room
{
public:
    // 유저 입장
    void Enter(std::shared_ptr<Session> session);

    // 유저 퇴장
    void Leave(std::shared_ptr<Session> session);

    // 방에 있는 모두에게 패킷 뿌리기
    void Broadcast(void* msg, int len);

private:
    std::mutex _lock; // 방은 여러 스레드(패킷 핸들러)가 접근할 수 있으니 락 필요
    std::vector<std::shared_ptr<Session>> _sessions;
};

// 일단 전역으로 방 하나만 둠 (나중엔 RoomManager가 관리)
extern std::shared_ptr<Room> G_Room;