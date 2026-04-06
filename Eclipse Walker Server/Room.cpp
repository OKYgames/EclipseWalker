#include "Room.h"
#include "Protocol.h" // [추가] 몬스터 동기화 패킷(PKT_S_MONSTER_SYNC) 사용을 위해 포함
#include <cmath>

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
    {
        if (session != nullptr)
            session->Send(msg, len);
    }
}

// ========================================================================
// [추가 1] 나를 제외한 방 인원에게만 패킷 브로드캐스트 (캐릭터 겹침 방지용)
// ========================================================================
void Room::BroadcastExcept(std::shared_ptr<Session> excludeSession, void* msg, int len)
{
    std::lock_guard<std::mutex> lock(_lock);

    for (auto& session : _sessions)
    {
        // 세션이 살아있고, 제외할 세션(본인)이 아닐 때만 전송
        if (session != nullptr && session != excludeSession)
        {
            session->Send(msg, len);
        }
    }
}

// ========================================================================
// [추가 2] 방 생성 시 몬스터 초기 스폰 로직 (최초 1회 호출)
// ========================================================================
void Room::InitMonsters()
{
    std::lock_guard<std::mutex> lock(_lock);

    // 예시: 1번 몬스터(임프) 생성
    ServerMonster m1;
    m1.monsterId = 1;
    m1.type = 0; // 0 = REAL_IMP
    m1.state = 0; // 0 = IDLE
    m1.x = 10.0f; m1.y = 0.0f; m1.z = 10.0f;
    m1.rotY = 0.0f;

    _monsters.push_back(m1);
}

// ========================================================================
// [추가 3] 서버 틱(Tick)마다 실행될 몬스터 AI 로직
// ========================================================================
void Room::UpdateMonsters(float dt)
{
    std::lock_guard<std::mutex> lock(_lock);

    for (auto& m : _monsters)
    {
        if (m.state == 3) continue; // 3 = DIE (죽은 몬스터는 연산 제외)

        bool isMovedOrStateChanged = false;

        // ----------------------------------------------------
        // TODO: 유저와의 거리 계산 및 상태(FSM) 전이 로직 작성
        // 현재는 테스트를 위해 TRACE(1) 상태일 때 직진만 하도록 구현
        // ----------------------------------------------------
        if (m.state == 1) // 1 = TRACE
        {
            m.x -= m.speed * dt;
            isMovedOrStateChanged = true;
        }

        // 몬스터의 상태나 위치가 변했다면, 방 안의 유저들에게 동기화 패킷 쏘기
        if (isMovedOrStateChanged)
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

            // 주의: 여기서 Broadcast() 함수를 부르면 이미 _lock이 잡혀있어 데드락이 발생함!
            // 따라서 내부에서 _sessions 리스트를 직접 순회하여 Send 처리합니다.
            for (auto& session : _sessions)
            {
                if (session != nullptr)
                {
                    session->Send(&syncPkt, sizeof(syncPkt));
                }
            }
        }
    }
}