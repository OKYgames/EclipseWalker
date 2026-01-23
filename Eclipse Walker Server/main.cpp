#include "IocpCore.h"
#include "Session.h"
#include "LogManager.h"
#include "ServerPacketHandler.h"
#include <vector>

std::vector<std::shared_ptr<Session>> G_Sessions;

class GameSession : public Session
{
public:
    virtual void OnConnected() override
    {
        LOG_INFO("Client Connected!");
    }

    virtual void OnDisconnected() override
    {
        LOG_WARN("Client Disconnected");
    }

    virtual int OnRecv(BYTE* buffer, int len) override
    {
        int processLen = 0;

        while (true)
        {
            int dataSize = len - processLen;

            // 1. 헤더 크기 체크
            if (dataSize < sizeof(PacketHeader))
                break;

            PacketHeader* header = (PacketHeader*)&buffer[processLen];

            // 2. 패킷 전체 크기 체크
            if (dataSize < header->size)
                break;

            // 3. 패킷 핸들러 호출 (이제 로직은 저쪽에서 처리함)
            // shared_from_this()는 나 자신(Session)의 포인터를 안전하게 넘겨줌
            ServerPacketHandler::HandlePacket(shared_from_this(), &buffer[processLen], header->size);

            processLen += header->size;
        }

        return processLen;
    }
};

int main()
{
    LogManager::GetInstance()->Initialize();

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    IocpCore iocp;
    iocp.Initialize();

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(7777);

    bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);

    LOG_INFO("Listening on Port 7777...");

    while (true)
    {
        SOCKADDR_IN clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET)
        {
            std::shared_ptr<GameSession> session = std::make_shared<GameSession>();
            session->Init(clientSocket, clientAddr);
            iocp.Register(session);

            G_Sessions.push_back(session);
        }
    }

    WSACleanup();
    LogManager::GetInstance()->Finalize();
    return 0;
}