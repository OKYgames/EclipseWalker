#include "IocpCore.h"
#include "Session.h"
#include "LogManager.h"
#include <vector> // 추가

// ★ [중요] 세션이 죽지 않게 잡아두는 창고 (전역 변수)
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
        // 나중에 여기서 G_Sessions에서 이 세션을 빼주는 코드가 필요함
    }

    virtual void OnRecv(BYTE* buffer, int len) override
    {
        // 1. 받은 내용 문자열로 변환 (눈으로 확인용)
        std::string recvMsg((char*)buffer, len);
        LOG_INFO("Recv: %s", recvMsg.c_str());

        // 2. 패킷 헥사값 찍어보기
        LOG_HEX("Recv Packet", buffer, len);

        // 3. 에코 (받은 거 그대로 돌려주기)
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

            G_Sessions.push_back(session);
        }
    }

    WSACleanup();
    LogManager::GetInstance()->Finalize();
    return 0;
}