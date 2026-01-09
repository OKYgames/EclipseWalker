#include "IocpCore.h"
#include "Session.h"
#include "LogManager.h"

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

    virtual void OnRecv(BYTE* buffer, int len) override
    {
        // 여기서 패킷 ID 확인
        LOG_HEX("Recv Packet", buffer, len);

        Send(buffer, len);
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
        }
    }

    WSACleanup();
    LogManager::GetInstance()->Finalize();
    return 0;
}