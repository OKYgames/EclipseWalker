#include "Room.h"

// 전역 방 생성
std::shared_ptr<Room> G_Room = std::make_shared<Room>();

void Room::Enter(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_lock);
    _sessions.push_back(session);
}

void Room::Leave(std::shared_ptr<Session> session)
{
    std::lock_guard<std::mutex> lock(_lock);

    // 벡터에서 해당 세션 찾아서 삭제 (람다 사용)
    auto it = std::remove(_sessions.begin(), _sessions.end(), session);
    _sessions.erase(it, _sessions.end());
}

void Room::Broadcast(void* msg, int len)
{
    std::lock_guard<std::mutex> lock(_lock);

    for (auto& session : _sessions)
    {
        // 세션이 살아있는지 확인 후 전송
        if (session != nullptr)
            session->Send(msg, len);
    }
}