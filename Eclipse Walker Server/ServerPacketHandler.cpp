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

    default:
        std::cout << "Unknown Packet ID: " << header->id << std::endl;
        break;
    }
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

void ServerPacketHandler::Handle_C_PLAYER_MOVE(std::shared_ptr<Session> session, PKT_C_PLAYER_MOVE& pkt)
{
    PKT_C_PLAYER_MOVE pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            // 서버에 플레이어 위치 갱신 (몬스터 AI 연산에 사용)
            session->SetPlayerInfo(session->GetPlayerId(), pktCopy.x, pktCopy.y, pktCopy.z);

            PKT_S_PLAYER_MOVE sendPkt;
            sendPkt.header.size = sizeof(PKT_S_PLAYER_MOVE);
            sendPkt.header.id = PacketID::S_PLAYER_MOVE;
            sendPkt.playerId = session->GetPlayerId();
            sendPkt.x = pktCopy.x;
            sendPkt.y = pktCopy.y;
            sendPkt.z = pktCopy.z;
            sendPkt.rotY = pktCopy.rotY;

            if (G_Room != nullptr)
                G_Room->BroadcastExcept(session, &sendPkt, sizeof(sendPkt));
        });
}