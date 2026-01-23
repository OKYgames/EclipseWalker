#include "ServerPacketHandler.h"
#include "Session.h" // 여기서 Session을 include 해야 함

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
    std::cout << "[Packet] Login Request - ID: " << pkt.playerId << " HP: " << pkt.hp << std::endl;

    // TODO: 나중엔 여기서 DB 확인 등을 함
    // 지금은 무조건 성공이라고 응답 보냄
    PKT_S_LOGIN sendPkt;
    sendPkt.header.size = sizeof(PKT_S_LOGIN);
    sendPkt.header.id = PacketID::S_LOGIN;
    sendPkt.success = true;
    sendPkt.myPlayerId = pkt.playerId;

    session->Send(&sendPkt, sizeof(sendPkt));
}

// [채팅 로직]
void ServerPacketHandler::Handle_C_CHAT(std::shared_ptr<Session> session, PKT_C_CHAT& pkt)
{
    std::cout << "[Packet] Chat: " << pkt.msg << std::endl;

    // 받은 말을 그대로 에코(Echo) 해주거나, 다른 유저한테 뿌려줌
    PKT_S_CHAT sendPkt;
    sendPkt.header.size = sizeof(PKT_S_CHAT);
    sendPkt.header.id = PacketID::S_CHAT;
    sendPkt.playerId = 999; // 임시 ID
    strcpy_s(sendPkt.msg, pkt.msg);

    session->Send(&sendPkt, sizeof(sendPkt));
}