#include "ServerPacketHandler.h"
#include "Session.h" // 여기서 Session을 include 해야 함
#include "GlobalQueue.h"
#include "Room.h"

void ServerPacketHandler::HandlePacket(std::shared_ptr<Session> session, BYTE* buffer, int len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

    switch (header->id)
    {
    case PacketID::C_LOGIN:
    {
        // 버퍼 안전성 체크 (패킷 크기만큼 충분한지)
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

    default:
        std::cout << "Unknown Packet ID: " << header->id << std::endl;
        break;
    }
}

// [로그인 로직]
void ServerPacketHandler::Handle_C_LOGIN(std::shared_ptr<Session> session, PKT_C_LOGIN& pkt)
{
    // ★ 중요: 여기서 바로 로직을 짜지 않고, 람다(Lambda)로 포장해서 큐에 던짐

    // 데이터를 복사해서 람다 안에 캡처(Capture)해둬야 함
    // (함수가 끝나면 pkt가 사라지니까)
    PKT_C_LOGIN pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            // --- 여기서부터는 Logic Thread가 실행함 (안전지대) ---
            std::cout << "[Logic Thread] Login Request Process..." << std::endl;

            PKT_S_LOGIN sendPkt;
            sendPkt.header.size = sizeof(PKT_S_LOGIN);
            sendPkt.header.id = PacketID::S_LOGIN;
            sendPkt.success = true;
            sendPkt.myPlayerId = pktCopy.playerId;

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
            sendPkt.playerId = 999; // 나중에 session->GetPlayerId() 같은걸로 교체
            strcpy_s(sendPkt.msg, pktCopy.msg);

            // [수정] 나한테만(Send) 보내는 게 아니라, 방 전체(Broadcast)에 뿌림!
            if (G_Room != nullptr)
                G_Room->Broadcast(&sendPkt, sizeof(sendPkt));
        });
}