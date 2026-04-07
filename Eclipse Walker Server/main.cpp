#include "Define.h"
#include "IocpCore.h"
#include "Session.h"
#include "LogManager.h"
#include "ServerPacketHandler.h"
#include "GlobalQueue.h"
#include "Room.h"
#include "DBConnection.h"
#include <mutex> // ← 추가

// G_Sessions 보호용 mutex 추가
std::vector<std::shared_ptr<Session>> G_Sessions;
std::mutex G_SessionLock; // ← 추가

class GameSession : public Session
{
public:
    virtual void OnConnected() override
    {
        LOG_INFO("Client Connected!");
        G_Room->Enter(shared_from_this());
    }

    virtual void OnDisconnected() override
    {
        LOG_WARN("Client Disconnected");
        G_Room->Leave(shared_from_this());

        // 세션 목록에서 제거 (락 보호)
        std::lock_guard<std::mutex> lock(G_SessionLock);
        auto it = std::remove_if(G_Sessions.begin(), G_Sessions.end(),
            [this](const std::shared_ptr<Session>& s) {
                return s.get() == this;
            });
        G_Sessions.erase(it, G_Sessions.end());
    }

    virtual int OnRecv(BYTE* buffer, int len) override
    {
        int processLen = 0;

        while (true)
        {
            int dataSize = len - processLen;
            if (dataSize < sizeof(PacketHeader)) break;

            PacketHeader* header = (PacketHeader*)&buffer[processLen];
            // 비정상 패킷 크기 방어 코드
            if (header->size <= 0 || header->size > 4096) {
                LOG_ERROR("Invalid packet size: %d ", header->size);
                break;
            }
            if (dataSize < header->size) break;

            ServerPacketHandler::HandlePacket(
                shared_from_this(), &buffer[processLen], header->size);
            processLen += header->size;
        }

        return processLen;
    }
};

int main()
{
    // 1. 로그 매니저 초기화
    LogManager::GetInstance()->Initialize();

    // 2. 잡 큐 생성
    G_JobQueue = new GlobalQueue();

    // 3. 몬스터 초기 스폰 (1회)
    G_Room->InitMonsters();

    // 4. 윈속 초기화
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 5. IOCP 초기화
    IocpCore iocp;
    iocp.Initialize();

    // 6. 로직 스레드
    std::thread logicThread([]()
        {
            while (true)
                G_JobQueue->Execute();
        });
    logicThread.detach();

    // 7. 틱 스레드 (몬스터 AI 20틱/초)
    std::thread tickThread([]()
        {
            using namespace std::chrono;
            auto prev = steady_clock::now();

            while (true)
            {
                auto  now = steady_clock::now();
                float dt = duration<float>(now - prev).count();
                prev = now;

                if (G_Room)
                    G_Room->UpdateMonsters(dt);

                std::this_thread::sleep_for(milliseconds(50));
            }
        });
    tickThread.detach();

    // 8. 리스닝 소켓 설정
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(7777);

    bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);

    LOG_INFO("Listening on Port 7777...");

    // 9. Accept 루프
    while (true)
    {
        SOCKADDR_IN clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET)
        {
            auto session = std::make_shared<GameSession>();
            session->Init(clientSocket, clientAddr);
            iocp.Register(session);

            // 락 보호해서 push
            std::lock_guard<std::mutex> lock(G_SessionLock);
            G_Sessions.push_back(session);
        }
    }

    WSACleanup();
}