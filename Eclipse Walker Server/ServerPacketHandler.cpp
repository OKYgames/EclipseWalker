#include "ServerPacketHandler.h"
#include "Session.h"
#include "GlobalQueue.h"
#include "Room.h"
#include "DBConnection.h"

void ServerPacketHandler::HandlePacket(std::shared_ptr<Session> session, BYTE* buffer, int len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

    switch (header->id)
    {
    case PacketID::C_LOGIN:
    {
        if (len < sizeof(PKT_C_LOGIN)) break;
        PKT_C_LOGIN* pkt = reinterpret_cast<PKT_C_LOGIN*>(buffer);
        Handle_C_LOGIN(session, *pkt);
    }
    break;

    case PacketID::C_CHAT:
    {
        if (len < sizeof(PKT_C_CHAT)) break;
        PKT_C_CHAT* pkt = reinterpret_cast<PKT_C_CHAT*>(buffer);
        Handle_C_CHAT(session, *pkt);
    }
    break;

    case PacketID::C_PLAYER_MOVE:
    {
        if (len < sizeof(PKT_C_PLAYER_MOVE)) break;
        PKT_C_PLAYER_MOVE* pkt = reinterpret_cast<PKT_C_PLAYER_MOVE*>(buffer);
        Handle_C_PLAYER_MOVE(session, *pkt);
    }
    break;
    case PacketID::C_PLAYER_ATTACK:
    {
        if (len < sizeof(PKT_C_PLAYER_ATTACK)) break;
        PKT_C_PLAYER_ATTACK* pkt = reinterpret_cast<PKT_C_PLAYER_ATTACK*>(buffer);
        Handle_C_PLAYER_ATTACK(session, *pkt);
    }
    break;

    case PacketID::C_GAME_START:
    {
        if (len < sizeof(PKT_C_GAME_START)) break;
        PKT_C_GAME_START* pkt = reinterpret_cast<PKT_C_GAME_START*>(buffer);
        Handle_C_GAME_START(session, *pkt);
    }
    break;


    default:
        std::cout << "Unknown Packet ID: " << header->id << std::endl;
        break;
    }
}

void ServerPacketHandler::Handle_C_PLAYER_ATTACK(std::shared_ptr<Session> session, PKT_C_PLAYER_ATTACK& pkt)
{
    PKT_C_PLAYER_ATTACK pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Player Attack - skillType: " << pktCopy.skillType << std::endl;

            // 공격 범위 설정 (스킬 타입별)
            float attackRange = 3.0f;
            float attackDamage = 10.0f;

            if (pktCopy.skillType == 1) { attackRange = 5.0f; attackDamage = 25.0f; }
            if (pktCopy.skillType == 2) { attackRange = 7.0f; attackDamage = 40.0f; }

            // Room에서 몬스터 목록 가져와서 범위 판정
            // (현재는 단순 거리 계산, 나중에 히트박스로 교체)
            if (G_Room != nullptr)
            {
                auto snapshots = G_Room->GetMonsterSnapshots();

                for (auto& m : snapshots)
                {
                    if (m.state == 3) continue; // DIE 상태 제외

                    float dx = m.x - pktCopy.x;
                    float dz = m.z - pktCopy.z;
                    float dist = sqrtf(dx * dx + dz * dz);

                    if (dist <= attackRange)
                    {
                        // 피해 적용 및 결과 브로드캐스트
                        bool isDead = G_Room->ApplyDamageToMonster(m.monsterId, (int)attackDamage);

                        PKT_S_MONSTER_HIT hitPkt;
                        hitPkt.header.size = sizeof(PKT_S_MONSTER_HIT);
                        hitPkt.header.id = PacketID::S_MONSTER_HIT;
                        hitPkt.monsterId = m.monsterId;
                        hitPkt.remainHp = G_Room->GetMonsterHp(m.monsterId);
                        hitPkt.isDead = isDead;

                        G_Room->Broadcast(&hitPkt, sizeof(hitPkt));
                    }
                }
            }
        });
}

void ServerPacketHandler::Handle_C_LOGIN(std::shared_ptr<Session> session, PKT_C_LOGIN& pkt)
{
    PKT_C_LOGIN pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Login Request Process..." << std::endl;

            std::string inputId = pktCopy.id;
            std::string inputPw = pktCopy.password;
            int userUid = 0;

            bool isLoginSuccess = DBConnection::GetInstance()->Login(inputId, inputPw, userUid);

            PKT_S_LOGIN sendPkt;
            sendPkt.header.size = sizeof(PKT_S_LOGIN);
            sendPkt.header.id = PacketID::S_LOGIN;

            if (isLoginSuccess)
            {
                sendPkt.success = true;
                sendPkt.myPlayerId = userUid;
                session->SetPlayerInfo(userUid, 0.0f, 0.0f, 0.0f); // 로그인 성공 시 ID 등록
            }
            else
            {
                sendPkt.success = false;
                sendPkt.myPlayerId = 0;
            }

            session->Send(&sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_CHAT(std::shared_ptr<Session> session, PKT_C_CHAT& pkt)
{
    PKT_C_CHAT pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Chat Broadcast: " << pktCopy.msg << std::endl;

            PKT_S_CHAT sendPkt;
            sendPkt.header.size = sizeof(PKT_S_CHAT);
            sendPkt.header.id = PacketID::S_CHAT;
            sendPkt.playerId = session->GetPlayerId();
            strcpy_s(sendPkt.msg, pktCopy.msg);

            if (G_Room != nullptr)
                G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_GAME_START(std::shared_ptr<Session> session, PKT_C_GAME_START& pkt)
{
    UNREFERENCED_PARAMETER(pkt);

    G_JobQueue->Push([session]()
        {
            if (G_Room == nullptr)
            {
                return;
            }

            auto host = G_Room->GetHost();
            if (host != nullptr && host != session)
            {
                return;
            }

            PKT_S_GAME_START sendPkt = {};
            sendPkt.header.size = sizeof(PKT_S_GAME_START);
            sendPkt.header.id = PacketID::S_GAME_START;
            G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_PLAYER_MOVE(std::shared_ptr<Session> session, PKT_C_PLAYER_MOVE& pkt)
{
    PKT_C_PLAYER_MOVE pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            int playerId = static_cast<int>(reinterpret_cast<intptr_t>(session.get()) & 0x7FFFFFFF);

            session->SetPlayerInfo(playerId, pktCopy.x, pktCopy.y, pktCopy.z);

            PKT_S_PLAYER_MOVE sendPkt;
            sendPkt.header.size = sizeof(PKT_S_PLAYER_MOVE);
            sendPkt.header.id = PacketID::S_PLAYER_MOVE;
            sendPkt.playerId = playerId;
            sendPkt.x = pktCopy.x;
            sendPkt.y = pktCopy.y;
            sendPkt.z = pktCopy.z;
            sendPkt.rotY = pktCopy.rotY;

            if (G_Room != nullptr)
                G_Room->BroadcastExcept(session, &sendPkt, sizeof(sendPkt));
        });
}
