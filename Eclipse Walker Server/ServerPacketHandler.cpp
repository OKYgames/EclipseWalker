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

            if (isLoginSuccess == true)
            {
                std::cout << "DB �α��� ����! �ο����� UID: " << userUid << std::endl;
                sendPkt.success = true;
                sendPkt.myPlayerId = userUid; // ���� �ذ�
            }
            else
            {
                std::cout << "DB �α��� ����! (���̵�/��� ����)" << std::endl;
                sendPkt.success = false;
                sendPkt.myPlayerId = 0; // ���� �ذ�
            }

            // 4. Ŭ���̾�Ʈ���� ��� ����
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
            sendPkt.playerId = 999; // ���߿� session->GetPlayerId() �����ɷ� ��ü
            strcpy_s(sendPkt.msg, pktCopy.msg);

            // [����] �����׸�(Send) ������ �� �ƴ϶�, �� ��ü(Broadcast)�� �Ѹ�!
            if (G_Room != nullptr)
                G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
        });
}

void ServerPacketHandler::Handle_C_PLAYER_MOVE(std::shared_ptr<Session> session, PKT_C_PLAYER_MOVE& pkt)
{
    PKT_C_PLAYER_MOVE pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            PKT_S_PLAYER_MOVE sendPkt;
            sendPkt.header.size = sizeof(PKT_S_PLAYER_MOVE);
            sendPkt.header.id = PacketID::S_PLAYER_MOVE; // 6�� ��Ŷ

            // ���� ���������� �ĺ��ϱ� ���� �ӽ÷� 100�� �ο�
            sendPkt.playerId = static_cast<int>(reinterpret_cast<intptr_t>(session.get()) & 0x7FFFFFFF);

            sendPkt.x = pktCopy.x;
            sendPkt.y = pktCopy.y;
            sendPkt.z = pktCopy.z;
            sendPkt.rotY = pktCopy.rotY;

            if (G_Room != nullptr)
            {
                G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
            }
        });
}