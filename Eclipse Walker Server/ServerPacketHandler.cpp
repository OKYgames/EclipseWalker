#include "ServerPacketHandler.h"
#include "Session.h" // 여기서 Session을 include 해야 함
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
    // 데이터를 복사해서 람다 안에 캡처
    PKT_C_LOGIN pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            std::cout << "[Logic Thread] Login Request Process..." << std::endl;

            // 1. char[50] 배열을 바로 std::string으로 변환 (에러 해결)
            std::string inputId = pktCopy.id;
            std::string inputPw = pktCopy.password;
            int userUid = 0;

            // 2. DB 검증 시작
            bool isLoginSuccess = DBConnection::GetInstance()->Login(inputId, inputPw, userUid);

            // 3. 클라이언트에게 보낼 응답 패킷 세팅
            PKT_S_LOGIN sendPkt;
            sendPkt.header.size = sizeof(PKT_S_LOGIN);
            sendPkt.header.id = PacketID::S_LOGIN; // 본인 코드의 PacketID 네이밍에 맞게 확인하세요

            if (isLoginSuccess == true)
            {
                std::cout << "DB 로그인 성공! 부여받은 UID: " << userUid << std::endl;
                sendPkt.success = true;
                sendPkt.myPlayerId = userUid; // 에러 해결
            }
            else
            {
                std::cout << "DB 로그인 실패! (아이디/비번 오류)" << std::endl;
                sendPkt.success = false;
                sendPkt.myPlayerId = 0; // 에러 해결
            }

            // 4. 클라이언트에게 결과 전송
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

void ServerPacketHandler::Handle_C_PLAYER_MOVE(std::shared_ptr<Session> session, PKT_C_PLAYER_MOVE& pkt)
{
    PKT_C_PLAYER_MOVE pktCopy = pkt;

    G_JobQueue->Push([session, pktCopy]()
        {
            PKT_S_PLAYER_MOVE sendPkt;
            sendPkt.header.size = sizeof(PKT_S_PLAYER_MOVE);
            sendPkt.header.id = PacketID::S_PLAYER_MOVE; // 6번 패킷

            // 누가 움직였는지 식별하기 위해 임시로 100번 부여
            sendPkt.playerId = 100;

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